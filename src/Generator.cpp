#include "Generator.hpp"
#include <fstream>

UTTE::InitialisationResult UTTE::Generator::loadFromFile(const utte_string& location, bool bReplaceInvalidCharacters, char32_t replaceInvalid) noexcept
{
    bReplaceValidCharactersI = bReplaceInvalidCharacters;
    replaceInvalidI = replaceInvalid;

    std::ifstream in(location);
    if (!in)
        return UTTE_INITIALISATION_RESULT_INVALID_FILE;
    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    data.resize(size);

    in.seekg(0);
    in.read(&data[0], static_cast<std::streamsize>(size));
    in.close();
    if (!utf8::is_valid(data))
    {
        if (bReplaceInvalidCharacters)
            utf8::replace_invalid(data, replaceInvalid);
        return UTTE_INITIALISATION_RESULT_INVALID_UTF8;
    }
    return UTTE_INITIALISATION_RESULT_SUCCESS;
}

UTTE::InitialisationResult UTTE::Generator::loadFromString(const utte_string& str, bool bReplaceInvalidCharacters, char32_t replaceInvalid) noexcept
{
    bReplaceValidCharactersI = bReplaceInvalidCharacters;
    replaceInvalidI = replaceInvalid;

    data = str;
    if (!utf8::is_valid(data))
    {
        if (bReplaceInvalidCharacters)
            utf8::replace_invalid(data, replaceInvalid);
        return UTTE_INITIALISATION_RESULT_INVALID_UTF8;
    }
    return UTTE_INITIALISATION_RESULT_SUCCESS;
}

UTTE::Function& UTTE::Generator::pushVariable(const UTTE::Variable& var, const utte_string& name) noexcept
{
    functions.push_back(Function
    {
        .name = name,
        .function = [var](std::vector<Variable>&, Generator*) -> Variable
        {
            return var;
        },
    });
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

UTTE::Variable UTTE::Generator::makeArray(const std::vector<utte_string>& arr) noexcept
{
    return { .value = std::to_string((intptr_t)(&arr)), .type = UTTE_VARIABLE_TYPE_HINT_ARRAY };
}

UTTE::Variable UTTE::Generator::makeMap(const utte_map<utte_string, utte_string>& map) noexcept
{
    return { .value = std::to_string(((intptr_t)&map)), .type = UTTE_VARIABLE_TYPE_HINT_MAP };
}

UTTE::ParseResult UTTE::Generator::parse() noexcept
{
    size_t i = data.find_first_of("{{");

    for (; i != utte_string::npos; i = data.find("{{", i))
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

UTTE::ParseResult UTTE::Generator::parseFunction(UTTE::Generator& generator, size_t& i, bool bRoot) noexcept
{
    auto& data = generator.data;

    UTTE::ParseResult result{}; // The result to return
    size_t begin = i; // Get the initial position of i. This is needed so that we can split the strings correctly

    size_t beginCut = begin;
    bool bWasSpace = true;
    //bool bPreviousWasSpace = true;
    //size_t beginCut = begin;
    std::vector<Variable> args;

    for (; i < data.size(); i++)
    {
        auto& it = data[i];
        auto& pit = data[i - 1]; // pit = previous iterator

        // Start function
        if ((i - 1) >= 0 && it == '{' && pit == '{')
        {
            // This is because we will be at the second bracket, but we want the first one
            size_t locationBeforeAppend = i - 1;

            // Increment i to exit the brackets, otherwise we will be in an endless loop
            ++i;
            // if "i" is equal to the size of our string terminate since we have a malformed statement with no termination
            if (i == data.size())
                return { .status = UTTE_PARSE_STATUS_EXPECTED_TERMINATION };

            // Recursively parse the function
            auto res = parseFunction(generator, i, false);
            if (result.status != UTTE_PARSE_STATUS_SUCCESS)
                return result;

            // Replace all data, previously occupied by a function expression. Add 1 to also remove the last bracket
            // since we are doing "look back" iteration, and we haven't updated the index in the previous recursive call
            data.replace(locationBeforeAppend, i - locationBeforeAppend + 1, bRoot ? res._internalBuffer.value : "");

            // A comment will produce an empty result, which we don't want. In general, we do accept empty results, just
            // not ones generated by comments
            if (!res._internalBuffer._internalBoolComment)
                args.push_back(res._internalBuffer);

            // This is done so that we don't break special functions. It's also more performant :)
            i = bRoot ? locationBeforeAppend + res._internalBuffer.value.length() : locationBeforeAppend;

            bWasSpace = true;
            //beginCut = i + 1;
            beginCut = (i + 1) < data.size() && (data[i + 1] == ' ' ||
                                                 data[i + 1] == '\t' ||
                                                 data[i + 1] == '\v' ||
                                                 data[i + 1] == '\n') ? i + 1 : i;
            continue;
        } // End function
        else if ((i - 2) >= 0 && it == '}' && pit == '}')
        {
            // In case a string is like this: {{ func arg1 arg2}} instead of {{ func arg1 arg2 }} we do a final cut
            if (data[i - 2] != ' ' && data[i - 2] != '\t' && data[i - 2] != '\v' && data[i - 2] != '\n')
                args.push_back({ .value = data.substr(beginCut, i - 1 - beginCut), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL });

            // If it's an empty string return an empty result. If not find the correct function and call it.
            if (!args.empty())
            {
                for (auto& a : generator.functions)
                {
                    if (a.name == args[0].value)
                    {
                        result._internalBuffer = a.function(args, &generator);
                        result.status = result._internalBuffer.status;
                        return result;
                    }
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
                    for (auto a : generator.specialFunctions)
                    {
                        // Matched a special function
                        if (generator.functions[a].name == args[0].value)
                        {
                            // Go up by 1 index so that we don't start from the " "
                            i = (i + 1) == data.size() ? i : i + 1;
                            size_t depth = 0; // Expression depth level
                            size_t initialPos = i;
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
                            args.emplace_back(data.substr(initialPos, i - initialPos - 1), UTTE_VARIABLE_TYPE_HINT_NORMAL);
                            result._internalBuffer = generator.functions[a].function(args, &generator);
                            result.status = result._internalBuffer.status;

                            return result;
                        }
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

bool UTTE::Variable::operator==(const UTTE::Variable &variable) const noexcept
{
    return (this->value == variable.value && this->type == variable.type );
}