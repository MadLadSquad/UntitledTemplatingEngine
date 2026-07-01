#include "Generator.hpp"
#include <fstream>

UTTE::Generator::Generator() noexcept
    : functions
    {
        { .name = "func",       .function = CoreFuncs::funcFunc            },
        { .name = "raw",        .function = CoreFuncs::funcRaw             },
        { .name = "comment",    .function = CoreFuncs::funcComment         },
        { .name = "if",         .function = CoreFuncs::funcIf              },
        { .name = "switch",     .function = CoreFuncs::funcSwitch          },
        { .name = "at",         .function = CoreFuncs::funcAt              },
        { .name = "cond",       .function = CoreFuncs::funcCond            },
        { .name = "for",        .function = CoreFuncs::funcFor             },
        { .name = "==",         .function = CoreFuncs::funcBoolEqual       },
        { .name = "!=",         .function = CoreFuncs::funcBoolNotEqual    },
        { .name = "!",          .function = CoreFuncs::funcBoolNot         },
        { .name = "&&",         .function = CoreFuncs::funcBoolAnd         },
        { .name = "||",         .function = CoreFuncs::funcBoolOr          },
        { .name = "list",       .function = CoreFuncs::funcList            },
        { .name = "dict",       .function = CoreFuncs::funcDict            },
    }
{}

const UTTE::Function* UTTE::Generator::findFunction(const utte_string& name) const noexcept
{
    // Search this generator's own registry first, then fall through to the parent chain. This gives inner scopes
    // (e.g. a for-loop's iterator variable) precedence over outer ones while still resolving the standard library and
    // user-pushed values held by the root.
    for (auto g = this; g != nullptr; g = g->parent)
        for (const auto& f : g->functions)
            if (f.name == name)
                return &f;
    return nullptr;
}

const UTTE::Function* UTTE::Generator::findSpecialFunction(const utte_string& name) const noexcept
{
    // The body-preserving special functions live in the root generator's registry, so walk up to it before checking
    // the specialFunctions indices.
    auto root = this;
    while (root->parent != nullptr)
        root = root->parent;

    for (const auto a : root->specialFunctions)
        if (root->functions[a].name == name)
            return &root->functions[a];
    return nullptr;
}

UTTE::InitialisationResult UTTE::Generator::loadFromFile(const utte_string& location) noexcept
{
    std::ifstream in(location);
    if (!in)
        return UTTE_INITIALISATION_RESULT_INVALID_FILE;
    in.seekg(0, std::ios::end);

    const std::streamoff size = in.tellg();
    if (size < 0)
        return UTTE_INITIALISATION_RESULT_INVALID_FILE;
    data.resize(static_cast<size_t>(size));

    in.seekg(0);
    in.read(data.data(), static_cast<std::streamsize>(size));
    in.close();
    return UTTE_INITIALISATION_RESULT_SUCCESS;
}

UTTE::InitialisationResult UTTE::Generator::loadFromString(const utte_string& str) noexcept
{
    data = str;
    return UTTE_INITIALISATION_RESULT_SUCCESS;
}

UTTE::Function& UTTE::Generator::pushVariable(const UTTE::Variable& var, const utte_string& name) noexcept
{
    functions.emplace_back(
        Function
        {
            .name = name,
            .function = [var](std::vector<Variable>&, Generator*) -> Variable
            {
                return var;
            },
        }
    );
    return functions.back();
}

bool UTTE::Generator::setVariable(const char* name, const UTTE::Variable& variable) noexcept
{
    for (auto& a : functions)
    {
        if (a.name == name)
        {
            a.function = [variable](std::vector<Variable>&, Generator*) -> Variable
            {
                return variable;
            };
            return true;
        }
    }
    return false;
}

bool UTTE::Generator::setFunction(const char* name, const std::function<Func>& event) noexcept
{
    for (auto& a : functions)
    {
        if (a.name == name)
        {
            a.function = event;
            return true;
        }
    }
    return false;
}

UTTE::Function& UTTE::Generator::pushFunction(const UTTE::Function& f) noexcept
{
    functions.push_back(f);
    return functions.back();
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

std::vector<UTTE::Function>& UTTE::Generator::getFunctionsRegistry() noexcept
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
        const auto& it = data[i];
        const auto& pit = data[i - 1]; // pit = previous iterator

        // Start function
        if (i >= 1 && it == '{' && pit == '{')
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
            // not ones generated by comments
            if (!res._internalBuffer._internalBoolComment)
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
            else if (data[i] == ' ' || data[i] == '\t' || data[i] == '\v' || data[i] == '\n')
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

        if (i >= 2 && it == '}' && pit == '}')
        {
            // In case a string is like this: {{ func arg1 arg2}} instead of {{ func arg1 arg2 }} we do a final cut
            if (data[i - 2] != ' ' && data[i - 2] != '\t' && data[i - 2] != '\v' && data[i - 2] != '\n')
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
            && (it == ' ' || it == '\t' || it == '\v' || it == '\n' || ((i + 1) < data.size() && it == '{' && data[i + 1] == '{') || (i + 1) == data.size()))
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
                        for (; i < data.size(); i++)
                        {
                            if (data[i] == '{' && data[i - 1] == '{')
                                ++depth;
                            else if (data[i] == '}' && data[i - 1] == '}')
                            {
                                if (depth == 0)
                                    goto exit_special_fun_inner_block;
                                --depth;
                                ++i;
                            }
                        }
exit_special_fun_inner_block:
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
