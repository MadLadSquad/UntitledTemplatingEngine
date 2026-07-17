#include "CoreFuncs.hpp"
#include "Generator.hpp"


UTTE::Variable UTTE::CoreFuncs::funcIf(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    // The function name and boolean expression are mandatory, followed by the true branch. The false branch is an
    // optional fallback: 3 args = no fallback (empty value on false), 4 args = explicit false branch.
    if (args.size() != 3 && args.size() != 4)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    // The condition is expected to arrive as an already-evaluated value. A deferred body ({{ func ... }} → FUNCTION
    // type) is intentionally NOT evaluated here: getBooleanV runs on the raw body text, which almost always reads as
    // true. Write the condition as a normal expression (e.g. {{ == a b }}) so the parser evaluates it while cutting
    // the arguments.
    const uint8_t index = getBooleanV(args[1].value) ? 2 : 3;

    // The branch we selected is the missing fallback: return an empty value instead of erroring out.
    if (index >= args.size())
        return { .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

    // A branch may be given either as a deferred body ({{ func ... }} → FUNCTION, evaluated lazily below so the
    // untaken branch is never rendered) or as a plain, already-evaluated value (any other type — the parser rendered
    // it eagerly while cutting the arguments, e.g. a bare {{ additional_cdn_hosts }}). Only the *selected* branch is
    // ever inspected, so an eager value in the untaken branch never matters. For an already-evaluated selected branch
    // the work is done: hand the value straight back rather than rejecting it as the wrong type. This is why `if`
    // (unlike switch/cond, whose value/branch pairing is distinguished purely by type) tolerates NORMAL branches.
    if (args[index].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        return { .value = args[index].value, .type = args[index].type };

    Generator gen(generator);

    gen.loadFromString(args[index].value);
    const auto result = gen.parse();
    if (result.status != UTTE_PARSE_STATUS_SUCCESS)
        return UTTE_ERROR(result.status);

    return { .value = *result.result, .type = result._internalBuffer.type };
}

// Parses a non-negative decimal index. Returns false (so callers can raise an error) for an empty string or any
// non-digit character, instead of the old std::istringstream, which was both slow and silently yielded 0/garbage on
// malformed input. A single decimal pass keeps this off the stream-parsing path.
static bool parseIndex(const utte_string& str, size_t& out) noexcept
{
    if (str.empty())
        return false;

    size_t value = 0;
    for (const char c : str)
    {
        if (c < '0' || c > '9')
            return false;
        value = value * 10 + static_cast<size_t>(c - '0');
    }
    out = value;
    return true;
}

UTTE::Variable UTTE::CoreFuncs::funcAt(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() != 3)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    if (args[1].type == UTTE_VARIABLE_TYPE_HINT_MAP)
    {
        const auto map = getMap(args[1]);
        if (map == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        // Use the map's own lookup instead of a linear scan — with a hash map (e.g. the UTTE_CUSTOM_MAP override)
        // this is O(1)
        const auto it = map->find(args[2].value);
        if (it != map->end())
            return { .value = it->second, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    }

    size_t index;
    if (!parseIndex(args[2].value, index))
        return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

    if (args[1].type == UTTE_VARIABLE_TYPE_HINT_ARRAY)
    {
        const auto array = getArray(args[1]);
        if (array == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        return (array->size() <= index) ? UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE)
                                        : Variable{ .value = (*array)[index], .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    }

    return (args[1].value.length() <= index) ? Variable{ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }
                                             : Variable{ .value = (utte_string() + args[1].value[index]), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

static UTTE::Variable parseBranch(UTTE::Generator& gen, const utte_string& body) noexcept
{
    gen.loadFromString(body);
    const auto r = gen.parse();
    if (r.status != UTTE_PARSE_STATUS_SUCCESS)
        return UTTE_ERROR(r.status);
    return { .value = *r.result, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

// The branch bodies render in `gen`, a child generator that resolves names through the parent chain instead of copying
// the whole registry. The child is constructed by the caller (funcSwitch/funcCond) because Generator's child
// constructor is private and only CoreFuncs members are friends of Generator — a free function cannot build one.
static UTTE::Variable evaluateBranchChain(const std::vector<UTTE::Variable>& args, const size_t start, const UTTE::Variable* switchValue, UTTE::Generator& gen) noexcept
{
    for (size_t i = start; i < args.size(); i++)
    {
        if ((i + 1) < args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i + 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        {
            // switch compares the case value against the subject; cond evaluates the test as a boolean. Like `if`,
            // a cond test must be an already-evaluated (NORMAL) value — the (value, branch) pairing requires it, so a
            // deferred {{ func ... }} test is never evaluated as a condition.
            const bool matched = switchValue != nullptr ? (*switchValue == args[i]) : UTTE::CoreFuncs::getBooleanV(args[i].value);
            if (matched)
                return parseBranch(gen, args[i + 1].value);
            ++i;
        } // A trailing NORMAL after a consumed (value, function) pair: no fallback, so return an empty value.
        else if ((i + 1) == args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i - 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
            return { .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
        else if ((i + 1) == args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION) // Last argument is the fallback branch
            return parseBranch(gen, args[i].value);
        else // Last element is not a function, therefore return an invalid type
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);
    }
    // No case matched and no fallback branch was supplied: return an empty value.
    return { .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcSwitch(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    // Build the lightweight child generator here (CoreFuncs is a friend of Generator, so it may call the private child
    // constructor) rather than deep-copying the parent.
    Generator gen(generator);
    // Cases start at index 2; each is compared against the subject at args[1].
    return evaluateBranchChain(args, 2, &args[1], gen);
}

UTTE::Variable UTTE::CoreFuncs::funcCond(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    // Build the lightweight child generator here (CoreFuncs is a friend of Generator, so it may call the private child
    // constructor) rather than deep-copying the parent.
    Generator gen(generator);
    // Tests start at index 1 and are evaluated as booleans (no subject value).
    return evaluateBranchChain(args, 1, nullptr, gen);
}

UTTE::Variable UTTE::CoreFuncs::funcFor(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 4 || args.size() > 5)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    Variable result;
    // This will interpret the body of the for loop. It resolves names through the parent chain, so its own registry
    // only ever holds the loop's iterator variable(s) as an overlay.
    Generator gen(generator);

    // 4 is the magic number corresponding to the number of arguments needed for a "for" loop of an array
    if (args.size() == 4)
    {
        if (args[3].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);

        std::vector<utte_string>* array = getArray(args[2]);
        if (array == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        auto& key = gen.pushVariable({ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }, args[1].value);
        for (auto& a : *array)
        {
            UTTE_VARIABLE_SET_NEW_VAL(key, a, a, UTTE_VARIABLE_TYPE_HINT_NORMAL);
            gen.loadFromString(args[3].value);

            const auto r = gen.parse();
            if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                return UTTE_ERROR(r.status);
            result.value += *r.result;
        }
    } // 5 is the magic number corresponding to the number of arguments needed for a "for" loop of a map
    else if (args.size() == 5)
    {
        // Maps are shifted by 1 position to account to the fact that we're dealing with 2 iterators
        if (args[4].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);

        utte_map<utte_string, utte_string>* map = getMap(args[3]);
        if (map == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        // Push these variables then use the references to set new values in the loop. The registry is a node-based
        // hash set, so references to its elements stay valid across later pushes — no reserve dance needed.
        auto& key = gen.pushVariable({ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }, args[1].value);
        auto& val = gen.pushVariable({ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }, args[2].value);
        for (auto& a : *map)
        {
            UTTE_VARIABLE_SET_NEW_VAL(key, a, a.first, UTTE_VARIABLE_TYPE_HINT_NORMAL);
            UTTE_VARIABLE_SET_NEW_VAL(val, a, a.second, UTTE_VARIABLE_TYPE_HINT_NORMAL);

            gen.loadFromString(args[4].value);
            const auto r = gen.parse();
            if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                return UTTE_ERROR(r.status);
            result.value += *r.result;
        }
    }
    return result;
}

template<bool equal>
static inline UTTE::Variable funcBooleanCompare(std::vector<UTTE::Variable>& args) noexcept
{
    UTTE::Variable result = { .value = "1", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    // Need the name plus at least two operands to compare; "{{ == a }}" is meaningless, so treat it as an error
    // (matching the arity requirement of the other n-ary boolean ops).
    if (args.size() < 3)
    {
        result.status = UTTE_PARSE_STATUS_OUT_OF_BOUNDS;
        return result;
    }

    const UTTE::Variable* variable = nullptr;
    bool cond = false;

    for (size_t i = 2; i < args.size(); i++)
    {
        if (i == 2)
            variable = &args[1];

        if constexpr (equal)
            cond = *variable != args[i];
        else
            cond = *variable == args[i];

        if (cond)
        {
            result.value[0] = '0';
            break;
        }
    }

    return result;
}

UTTE::Variable UTTE::CoreFuncs::funcBoolEqual(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    return funcBooleanCompare<true>(args);
}

UTTE::Variable UTTE::CoreFuncs::funcBoolNotEqual(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    return funcBooleanCompare<false>(args);
}

UTTE::Variable UTTE::CoreFuncs::funcBoolNot(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    const char result[2]  = { static_cast<char>(!getBooleanV(args[1].value) + '0'), '\0' };
    return { .value = result, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcBoolAnd(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() < 3)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    const Variable& comparator = args[1];
    Variable result = { .value = "1", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

    for (size_t i = 2; i < args.size(); i++)
    {
        if (!(getBooleanV(comparator.value) && getBooleanV(args[i].value)))
        {
            result.value[0] = '0';
            break;
        }
    }
    return result;
}

UTTE::Variable UTTE::CoreFuncs::funcBoolOr(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() < 3)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    Variable& comparator = args[1];
    Variable result = { .value = "1", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    if (getBooleanV(comparator.value))
        return result;

    result.value[0] = '0';
    for (size_t i = 2; i < args.size(); i++)
    {
        if (getBooleanV(args[i].value))
        {
            result.value[0] = '1';
            break;
        }
    }
    return result;
}

UTTE::Variable UTTE::CoreFuncs::funcFunc(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() > 1)
    {
        args[1].type = UTTE_VARIABLE_TYPE_HINT_FUNCTION;
        return args[1];
    }
    return Variable{ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_FUNCTION };
}

UTTE::Variable UTTE::CoreFuncs::funcRaw(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    // First argument will be the raw string. If no second value exists return empty
    return args.size() > 1 ? args[1] : Variable{ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcComment(std::vector<Variable>&, UTTE::Generator*) noexcept
{
    return
    {
        .value = "",
        .type = UTTE_VARIABLE_TYPE_HINT_NORMAL,
        ._internalBoolComment = true,
    };
}

UTTE::Variable UTTE::CoreFuncs::funcList(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    auto& arr = generator->requestArrayWithGC();
    for (size_t i = 1; i < args.size(); i++)
        arr.push_back(args[i].value);

    return UTTE::Generator::makeArray(arr);
}

bool UTTE::CoreFuncs::getBooleanV(const utte_string& str) noexcept
{
    // str[0] is safe even for an empty string: since C++11, operator[](size()) is guaranteed to return a reference
    // to a null character, so an empty value reads as false without a separate empty() check
    return !(str[0] == 0 || str[0] == '0' || str == "false");
}

std::vector<utte_string>* UTTE::CoreFuncs::getArray(const UTTE::Variable& variable) noexcept
{
    if (variable.type != UTTE_VARIABLE_TYPE_HINT_ARRAY)
        return nullptr;

    // Arrays encode their pointer as raw bytes (see Generator::encodePointer); decodePointer hands it back as an
    // intptr_t, so reinterpret that integer back into the concrete pointer type.
    return reinterpret_cast<std::vector<utte_string>*>(Generator::decodePointer(variable.value));
}

utte_map<utte_string, utte_string>* UTTE::CoreFuncs::getMap(const UTTE::Variable& variable) noexcept
{
    if (variable.type != UTTE_VARIABLE_TYPE_HINT_MAP)
        return nullptr;

    // Maps encode their pointer as raw bytes (see Generator::encodePointer); decodePointer hands it back as an
    // intptr_t, so reinterpret that integer back into the concrete pointer type.
    return reinterpret_cast<utte_map<utte_string, utte_string>*>(Generator::decodePointer(variable.value));
}

UTTE::Variable UTTE::CoreFuncs::funcDict(std::vector<Variable>& args, Generator* generator) noexcept
{
    auto& map = generator->requestMapWithGC();
    // Duplicate keys are intentionally ignored: insert() keeps the first occurrence, so {{ dict a 1 a 2 }} yields
    // { a: "1" }
    for (size_t i = 1; i < args.size(); i++)
        if ((i % 2) == 0)
            map.insert({ args[i - 1].value, args[i].value });

    // This will only be called if we have odd arguments. The check is for even because the function name adds 1
    if (args.size() % 2 == 0)
        map.insert({ args.back().value, "" });

    return Generator::makeMap(map);
}