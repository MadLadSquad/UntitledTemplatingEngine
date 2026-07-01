#include "CoreFuncs.hpp"
#include "Generator.hpp"
#include <sstream>


UTTE::Variable UTTE::CoreFuncs::funcIf(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    // The function name and boolean expression are mandatory, followed by the true branch. The false branch is an
    // optional fallback: 3 args = no fallback (empty value on false), 4 args = explicit false branch.
    if (args.size() != 3 && args.size() != 4)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    if (args[2].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);
    if (args.size() == 4 && args[3].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);

    uint8_t index = 2;
    if (!getBooleanV(args[1].value))
        index = 3;

    // The branch we selected is the missing fallback: return an empty value instead of erroring out.
    if (index >= args.size())
        return { .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

    Generator gen(generator);

    gen.loadFromString(args[index].value);
    const auto result = gen.parse();
    if (result.status != UTTE_PARSE_STATUS_SUCCESS)
        return UTTE_ERROR(result.status);

    return { .value = *result.result, .type = result._internalBuffer.type };
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

        for (const auto& a : *map)
            if (args[2].value == a.first)
                return { .value = a.second, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    }

    if (args[1].type == UTTE_VARIABLE_TYPE_HINT_ARRAY)
    {
        const auto array = getArray(args[1]);
        if (array == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        size_t index;
        std::istringstream(args[2].value) >> index;

        return (array->size() <= index) ? UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE)
                                        : Variable{ .value = (*array)[index], .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    }

    size_t index;
    std::istringstream(args[2].value) >> index;

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

static UTTE::Variable evaluateBranchChain(const std::vector<UTTE::Variable>& args, const size_t start, const UTTE::Variable* switchValue, UTTE::Generator* generator) noexcept
{
    // The branch bodies render in a child generator that resolves names through the parent chain instead of copying
    // the whole registry.
    UTTE::Generator gen(*generator);

    for (size_t i = start; i < args.size(); i++)
    {
        if ((i + 1) < args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i + 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        {
            // switch compares the case value against the subject; cond evaluates the test as a boolean.
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

    // Cases start at index 2; each is compared against the subject at args[1].
    return evaluateBranchChain(args, 2, &args[1], generator);
}

UTTE::Variable UTTE::CoreFuncs::funcCond(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    // Tests start at index 1 and are evaluated as booleans (no subject value).
    return evaluateBranchChain(args, 1, nullptr, generator);
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

        // Push these variables then use the reference to append new values in the loop. Reserve up front so the
        // second push cannot reallocate the registry and dangle the reference taken from the first.
        gen.getFunctionsRegistry().reserve(2);
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
    if (args.size() < 2)
    {
        result.status = UTTE_PARSE_STATUS_OUT_OF_BOUNDS;
        return result;
    }

    const UTTE::Variable* variable = nullptr;
    bool cond;

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
    if (args.size() == 1)
        return { .value = "0", .type = UTTE_VARIABLE_TYPE_HINT_ARRAY };

    auto& arr = generator->requestArrayWithGC();
    for (size_t i = 1; i < args.size(); i++)
        arr.push_back(args[i].value);

    return UTTE::Generator::makeArray(arr);
}

bool UTTE::CoreFuncs::getBooleanV(const utte_string& str) noexcept
{
    // On 0 characters will return 0 - guaranteed by ISO C++ 11, on 1 character will return a valid bool
    return str[0] || str == "true";
}

std::vector<utte_string>* UTTE::CoreFuncs::getArray(const UTTE::Variable& variable) noexcept
{
    if (variable.type != UTTE_VARIABLE_TYPE_HINT_ARRAY)
        return nullptr;

    // Get memory address of array. Arrays and maps encode their pointers as strings
    auto addr = reinterpret_cast<intptr_t>(nullptr);
    std::istringstream(variable.value) >> addr;

    return reinterpret_cast<std::vector<utte_string>*>(addr);
}

utte_map<utte_string, utte_string>* UTTE::CoreFuncs::getMap(const UTTE::Variable& variable) noexcept
{
    if (variable.type != UTTE_VARIABLE_TYPE_HINT_MAP)
        return nullptr;

    // Get memory address of map. Arrays and maps encode their pointers as strings
    auto addr = reinterpret_cast<intptr_t>(nullptr);
    std::istringstream(variable.value) >> addr;

    return reinterpret_cast<utte_map<utte_string, utte_string>*>(addr);
}

UTTE::Variable UTTE::CoreFuncs::funcDict(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() == 1)
        return { .value = "0", .type = UTTE_VARIABLE_TYPE_HINT_MAP };

    auto& map = generator->requestMapWithGC();
    for (size_t i = 1; i < args.size(); i++)
        if ((i % 2) == 0)
            map.insert({ args[i - 1].value, args[i].value });

    // This will only be called if we have odd arguments. The check is for even because the function name adds 1
    if (args.size() % 2 == 0)
        map.insert({ args.back().value, "" });

    return Generator::makeMap(map);
}