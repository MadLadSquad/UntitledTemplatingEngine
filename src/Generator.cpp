#include "Generator.hpp"
#include <fstream>

UTTE::Generator::Generator() noexcept
    : functions
    {
        { .name = "func",       .function = CoreFuncs::funcFunc,            ._internalOwner = this      },
        { .name = "raw",        .function = CoreFuncs::funcRaw,             ._internalOwner = this      },
        { .name = "comment",    .function = CoreFuncs::funcComment,         ._internalOwner = this      },
        { .name = "if",         .function = CoreFuncs::funcIf,              ._internalOwner = this      },
        { .name = "switch",     .function = CoreFuncs::funcSwitch,          ._internalOwner = this      },
        { .name = "at",         .function = CoreFuncs::funcAt,              ._internalOwner = this      },
        { .name = "cond",       .function = CoreFuncs::funcCond,            ._internalOwner = this      },
        { .name = "for",        .function = CoreFuncs::funcFor,             ._internalOwner = this      },
        { .name = "==",         .function = CoreFuncs::funcBoolEqual,       ._internalOwner = this      },
        { .name = "!=",         .function = CoreFuncs::funcBoolNotEqual,    ._internalOwner = this      },
        { .name = "!",          .function = CoreFuncs::funcBoolNot,         ._internalOwner = this      },
        { .name = "&&",         .function = CoreFuncs::funcBoolAnd,         ._internalOwner = this      },
        { .name = "||",         .function = CoreFuncs::funcBoolOr,          ._internalOwner = this      },
        { .name = "list",       .function = CoreFuncs::funcList,            ._internalOwner = this      },
        { .name = "dict",       .function = CoreFuncs::funcDict,            ._internalOwner = this	},
    }
{}

const UTTE::Function* UTTE::Generator::findFunction(const utte_string& name) const noexcept
{
    // Search this generator's own registry first, then fall through to the parent chain. This gives inner scopes
    // (e.g. a for-loop's iterator variable) precedence over outer ones while still resolving the standard library and
    // user-pushed values held by the root. Each registry is hashed by name, so a lookup is O(1) per scope instead of
    // the linear scan the old vector registry required.
    for (auto g = this; g != nullptr; g = g->parent)
    {
        const auto f = g->functions.find(name);
        if (f != g->functions.end())
            return &*f;
    }
    return nullptr;
}

const UTTE::Function* UTTE::Generator::findSpecialFunction(const utte_string& name) const noexcept
{
    // The body-preserving special functions live in the root generator's registry, so walk up to it before checking
    // the name against the specialFunctionNames list.
    auto root = this;
    while (root->parent != nullptr)
        root = root->parent;

    for (const auto special : specialFunctionNames)
    {
        if (name == special)
        {
            const auto f = root->functions.find(name);
            return f != root->functions.end() ? &*f : nullptr;
        }
    }
    return nullptr;
}

UTTE::InitialisationResult UTTE::Generator::loadFromFile(const utte_string& location) noexcept
{
    // Binary mode: text mode on Windows collapses CRLF pairs, so read() returns fewer bytes than the tellg()-based
    // size and the tail of `data` would be left as NUL padding
    std::ifstream in(location, std::ios::binary);
    if (!in)
        return UTTE_INITIALISATION_RESULT_INVALID_FILE;
    in.seekg(0, std::ios::end);

    const std::streamoff size = in.tellg();
    if (size < 0)
        return UTTE_INITIALISATION_RESULT_INVALID_FILE;
    data.resize(static_cast<size_t>(size));

    in.seekg(0);
    if (size > 0 && !in.read(data.data(), static_cast<std::streamsize>(size)))
    {
        data.clear();
        return UTTE_INITIALISATION_RESULT_INVALID_FILE;
    }
    return UTTE_INITIALISATION_RESULT_SUCCESS;
}

UTTE::InitialisationResult UTTE::Generator::loadFromString(const utte_string& str) noexcept
{
    data = str;
    return UTTE_INITIALISATION_RESULT_SUCCESS;
}

UTTE::Function& UTTE::Generator::pushVariable(const UTTE::Variable& var, const utte_string& name) noexcept
{
    return pushFunction(
        Function
        {
            .name = name,
            .function = [var](std::vector<Variable>&, Generator*) -> Variable
            {
                return var;
            },
        }
    );
}

bool UTTE::Generator::setVariable(const char* name, const UTTE::Variable& variable) noexcept
{
    return setFunction(name, [variable](std::vector<Variable>&, Generator*) -> Variable
    {
        return variable;
    });
}

bool UTTE::Generator::setFunction(const char* name, const std::function<Func>& event) noexcept
{
    const auto f = functions.find(utte_string{ name });
    if (f == functions.end())
        return false;

    // Registry elements are const to protect the hash key (the name); the callback is not part of the key, so
    // mutating it through a const_cast keeps the container's invariants intact
    const_cast<Function&>(*f).function = event;
    return true;
}

UTTE::Function& UTTE::Generator::pushFunction(const UTTE::Function& f) noexcept
{
    // On a name collision insert() keeps the existing entry, which preserves the old lookup semantics: with the
    // vector registry a duplicate push appended an entry that findFunction (first match wins) could never reach
    const auto [it, bInserted] = functions.insert(f);
    auto& stored = const_cast<Function&>(*it); // Safe: only non-key members are ever mutated through this reference
    if (bInserted)
        stored._internalOwner = this;
    return stored;
}

bool UTTE::Generator::renameFunction(UTTE::Function& f, const utte_string& newName) noexcept
{
    if (f.name == newName)
        return true;

    const auto it = functions.find(f.name);
    // The entry must be this registry's own element (not a copy of it or another generator's function), and the new
    // name must be free
    if (it == functions.end() || &*it != &f || functions.find(newName) != functions.end())
        return false;

    // The name is the hash key, so it cannot be mutated in place: extract the node, rename it and re-insert. A node
    // extraction/insertion never copies or moves the element itself, so its address — and therefore every existing
    // reference or C API handle to it — stays valid.
    auto node = functions.extract(it);
    node.value().name = newName;
    functions.insert(std::move(node));
    return true;
}

UTTE::ParseResult UTTE::Generator::parse() noexcept
{
    for (size_t i = data.find("{{"); i != utte_string::npos; i = data.find("{{", i))
    {
        ++i;
        ParseResultStatus status = parseFunction(*this, i, true).status;
        if (status != UTTE_PARSE_STATUS_SUCCESS)
            return ParseResult{ .status = status, .result = &data };
    }
    return ParseResult{ .status = UTTE_PARSE_STATUS_SUCCESS, .result = &data };
}

UTTE::FunctionRegistry& UTTE::Generator::getFunctionsRegistry() noexcept
{
    return functions;
}


utte_string UTTE::Generator::encodePointer(const std::intptr_t value) noexcept
{
    utte_string out;
    out.resize(sizeof value);
    std::memcpy(out.data(), &value, sizeof value);
    return out;
}

std::intptr_t UTTE::Generator::decodePointer(const utte_string& value) noexcept
{
    std::intptr_t out = 0;
    if (value.size() == sizeof out)
        std::memcpy(&out, value.data(), sizeof out);
    return out;
}

std::vector<utte_string>& UTTE::Generator::requestArrayWithGC() noexcept
{
    internalVectorsForList.emplace_back();
    return internalVectorsForList.back();
}

utte_map<utte_string, utte_string>& UTTE::Generator::requestMapWithGC() noexcept
{
    internalMapsForDict.emplace_back();
    return internalMapsForDict.back();
}

// The whitespace set that separates arguments inside an expression. '\r' is included so CRLF-authored templates
// don't glue a carriage return onto the last argument of a line.
static bool isSeparator(const char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\n' || c == '\r';
}

UTTE::ParseResult UTTE::Generator::parseFunction(UTTE::Generator& generator, size_t& i, const bool bRoot) noexcept
{
    auto& data = generator.data;

    UTTE::ParseResult result{}; // The result to return
    const size_t begin = i; // Get the initial position of i. This is needed so that we can split the strings correctly

    size_t beginCut = begin;
    bool bWasSpace = true;
    std::vector<Variable> args;

    for (; i < data.size(); i++)
    {
        // The previous character (data[i - 1]) is only ever read behind the index guards below, so a frame entered
        // at i == 0 cannot read out of bounds
        const auto& it = data[i];

        // Start function
        if (i >= 1 && it == '{' && data[i - 1] == '{')
        {
            // This is because we will be at the second bracket, but we want the first one
            const size_t locationBeforeAppend = i - 1;

            // Increment i to exit the brackets, otherwise we will be in an endless loop
            ++i;
            // if "i" is equal to the size of our string terminate since we have a malformed statement with no termination
            if (i == data.size())
                return { .status = UTTE_PARSE_STATUS_EXPECTED_TERMINATION };

            // Recursively parse the function
            auto res = parseFunction(generator, i, false);
            // Propagate a failure from the nested expression. This must test the *nested* call's status (res), not
            // this frame's still-default result, otherwise errors from inner expressions are silently swallowed.
            if (res.status != UTTE_PARSE_STATUS_SUCCESS)
                return res;

            // Replace all data, previously occupied by a function expression. Add 1 to also remove the last bracket
            // since we are doing "look back" iteration, and we haven't updated the index in the previous recursive call
            data.replace(locationBeforeAppend, i - locationBeforeAppend + 1, bRoot ? res._internalBuffer.value : "");

            // A comment will produce an empty result, which we don't want. In general, we do accept empty results, just
            // not ones generated by comments. The root frame is a document scanner, not a function call, so it never
            // collects arguments — for a large document that used to duplicate every word and every expression result
            // into a vector that nothing legitimately read
            if (!bRoot && !res._internalBuffer._internalBoolComment)
                args.push_back(res._internalBuffer);

            // This is done so that we don't break special functions. It's also more performant :)
            i = bRoot ? locationBeforeAppend + res._internalBuffer.value.length() : locationBeforeAppend;

            // After splicing out the nested expression, i points at the first character that followed it. The loop's
            // ++i is about to step over data[i], so decide where the next argument begins based on data[i] itself
            // (not data[i + 1] — that off-by-one is what used to glue a stray leading space onto the next argument,
            // e.g. the key "b" arriving as " b" and never matching a map entry):
            //  - no leftover char        -> nothing pending.
            //  - leftover is whitespace  -> it's a separator; skip it, next argument starts at i + 1.
            //  - leftover is a real char -> it's the first character of the next argument; keep it (beginCut = i).
            if (i >= data.size())
            {
                bWasSpace = true;
                beginCut = i;
            }
            else if (isSeparator(data[i]))
            {
                bWasSpace = true;
                beginCut = i + 1;
            }
            else
            {
                bWasSpace = false;
                beginCut = i;
            }
            continue;
        } // End function

        // Everything below implements the inside of a function expression: argument cutting, the `}}` terminator and
        // the special-function body capture. The root frame is plain document text, so none of it applies there — a
        // stray "}}" in top-level text is literal output (it used to invoke whatever function the previously collected
        // args[0] happened to name and could abort the whole parse), and a bare word like "raw" after a comment must
        // not trigger special body capture that swallows the rest of the document
        if (bRoot)
            continue;

        if (i >= 2 && it == '}' && data[i - 1] == '}')
        {
            // In case a string is like this: {{ func arg1 arg2}} instead of {{ func arg1 arg2 }} we do a final cut
            if (!isSeparator(data[i - 2]))
                args.push_back({ .value = data.substr(beginCut, i - 1 - beginCut), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL });

            // If it's an empty string return an empty result. If not find the correct function and call it.
            if (!args.empty())
            {
                if (const Function* f = generator.findFunction(args[0].value))
                {
                    result._internalBuffer = f->function(args, &generator);
                    result.status = result._internalBuffer.status;
                    return result;
                }
            }
            return result;
        } // Argument and most of the string cutting behaviour here

        if (!((i + 1) < data.size() && it == '}' && data[i + 1] == '}')
            && (isSeparator(it) || ((i + 1) < data.size() && it == '{' && data[i + 1] == '{') || (i + 1) == data.size()))
        {
            if (bWasSpace)
                ++beginCut;
            else
            {
                args.push_back({ .value = data.substr(beginCut, i - beginCut), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL });
                if (args.size() == 1)
                {
                    // Matched a special function
                    if (const Function* special = generator.findSpecialFunction(args[0].value))
                    {
                        // Go up by 1 index so that we don't start from the " "
                        i = (i + 1) == data.size() ? i : i + 1;
                        size_t depth = 0; // Expression depth level
                        const size_t initialPos = i;
                        bool bTerminated = false;
                        for (; i < data.size(); i++)
                        {
                            if (data[i] == '{' && data[i - 1] == '{')
                            {
                                // Count the pair and step past it, mirroring the ++i on the closing side. Without the
                                // skip, overlapping matches count a run like "{{{" as depth 2 (pairs at 0-1 and 1-2),
                                // and the body capture then eats the rest of the document looking for closers that
                                // don't exist
                                ++depth;
                                ++i;
                            }
                            else if (data[i] == '}' && data[i - 1] == '}')
                            {
                                if (depth == 0)
                                {
                                    bTerminated = true;
                                    break;
                                }
                                --depth;
                                ++i;
                            }
                        }
                        // Reaching EOF without a closing "}}" used to fall through and silently truncate the last
                        // character of the document; report it like any other unterminated expression
                        if (!bTerminated)
                            return { .status = UTTE_PARSE_STATUS_EXPECTED_TERMINATION };
                        args.push_back({ .value = data.substr(initialPos, i - initialPos - 1), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL });
                        result._internalBuffer = special->function(args, &generator);
                        result.status = result._internalBuffer.status;

                        return result;
                    }
                }
            }
            bWasSpace = true;
            beginCut = (i + 1) < data.size() ? i + 1 : i;
        }
        else
            bWasSpace = false;
    }
    return result;
}

UTTE::Variable UTTE::Generator::makeArray(const std::vector<utte_string>& arr) noexcept
{
    return { .value = encodePointer(reinterpret_cast<std::intptr_t>(&arr)), .type = UTTE_VARIABLE_TYPE_HINT_ARRAY };
}

UTTE::Variable UTTE::Generator::makeMap(const utte_map<utte_string, utte_string>& map) noexcept
{
    return { .value = encodePointer(reinterpret_cast<std::intptr_t>(&map)), .type = UTTE_VARIABLE_TYPE_HINT_MAP };
}

bool UTTE::Variable::operator==(const UTTE::Variable &variable) const noexcept
{
    return (this->value == variable.value && this->type == variable.type );
}
