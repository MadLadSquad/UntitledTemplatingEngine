#include "CoreFuncs.hpp"
#include "Generator.hpp"
#include <sstream>


UTTE::Variable UTTE::CoreFuncs::funcIf(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    // This is because this is a binary function + 1 for the boolean expression and 1 for the name of the function
    if (args.size() != 4)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    if (args[2].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION || args[3].type != UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);

    UTTE::Generator gen{};
    gen.functions = generator->functions;

    uint8_t index = 2;
    if (!getBooleanV(args[1].value))
        index = 3;

    gen.loadFromString(args[index].value);
    auto result = gen.parse();
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
        auto map = getMap(args[1]);
        if (map == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        for (auto& a : *map)
            if (args[2].value == a.first)
                return { .value = a.second, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    }
    else if (args[1].type == UTTE_VARIABLE_TYPE_HINT_ARRAY)
    {
        auto array = getArray(args[1]);
        if (array == nullptr)
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE);

        size_t index;
        std::istringstream(args[2].value) >> index;

        return (array->size() <= index) ? UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_VALUE)
                                        : Variable{ .value = (*array)[index], .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    }
    else
    {
        size_t index;
        std::istringstream(args[2].value) >> index;

        return (args[1].value.length() <= index) ? Variable{ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }
                                                 : Variable{ .value = (utte_string() + args[1].value[index]), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    }
}

UTTE::Variable UTTE::CoreFuncs::funcSwitch(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    
    Variable result;
    
    Generator gen{};
    gen.functions = generator->functions;
    
    for (size_t i = 2; i < args.size(); i++)
    {
        if ((i + 1) < args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i + 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        {
            if (args[1] == args[i])
            {
                gen.loadFromString(args[i + 1].value);
                auto r = gen.parse();
                if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                    return UTTE_ERROR(r.status);
                return { .value = *r.result, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
            }
            ++i;
        } // This will be called if the last function is also one that matches a value. The default fallback function which returns an empty value will be called
        else if ((i + 1) == args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i - 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
            return { .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
        else if ((i + 1) == args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)// Last argument is function
        {
            gen.loadFromString(args[i].value);
            auto r = gen.parse();
            if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                return UTTE_ERROR(r.status);
            return { .value = *r.result, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
        }
        else // Last element is not a function, therefore return an invalid type
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);
    }
    return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
}

UTTE::Variable UTTE::CoreFuncs::funcCond(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    Variable result;
    Generator gen{};
    gen.functions = generator->functions;

    for (size_t i = 1; i < args.size(); i++)
    {
        if ((i + 1) < args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i + 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
        {
            if (getBooleanV(args[i].value))
            {
                gen.loadFromString(args[i + 1].value);
                auto r = gen.parse();
                if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                    return UTTE_ERROR(r.status);
                return { .value = *r.result, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
            }
            ++i;
        } // This will be called if the last function is also one that matches a value. The default fallback function which returns an empty value will be called
        else if ((i + 1) == args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_NORMAL && args[i - 1].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)
            return { .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
        else if ((i + 1) == args.size() && args[i].type == UTTE_VARIABLE_TYPE_HINT_FUNCTION)// Last argument is function
        {
            gen.loadFromString(args[i].value);
            auto r = gen.parse();
            if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                return UTTE_ERROR(r.status);
            return { .value = *r.result, .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
        }
        else // Last element is not a function, therefore return an invalid type
            return UTTE_ERROR(UTTE_PARSE_STATUS_INVALID_TYPE);
    }
    return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
}

UTTE::Variable UTTE::CoreFuncs::funcFor(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() < 4 || args.size() > 5)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    Variable result;
    // This will interpret the body of the for loop
    Generator gen{};
    gen.functions = generator->functions;

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

            auto r = gen.parse();
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

        // Push these variables then use the reference to append new values in the loop.
        auto& key = gen.pushVariable({ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }, args[1].value);
        auto& val = gen.pushVariable({ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL }, args[2].value);
        for (auto& a : *map)
        {
            UTTE_VARIABLE_SET_NEW_VAL(key, a, a.first, UTTE_VARIABLE_TYPE_HINT_NORMAL);
            UTTE_VARIABLE_SET_NEW_VAL(val, a, a.second, UTTE_VARIABLE_TYPE_HINT_NORMAL);

            gen.loadFromString(args[4].value);
            auto r = gen.parse();
            if (r.status != UTTE_PARSE_STATUS_SUCCESS)
                return UTTE_ERROR(r.status);
            result.value += *r.result;
        }
    }
    return result;
}

UTTE::Variable UTTE::CoreFuncs::funcBoolEqual(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    Variable* variable = nullptr;
    bool result = true;
    for (size_t i = 2; i < args.size(); i++)
    {
        if (i == 2)
            variable = &args[1];
        if (*variable != args[i])
        {
            result = false;
            break;
        }
    }
    return { .value = std::to_string(result), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcBoolNotEqual(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    Variable* variable = nullptr;
    bool result = true;
    for (size_t i = 2; i < args.size(); i++)
    {
        if (i == 2)
            variable = &args[1];

        if (*variable == args[i])
            return { .value = std::to_string(false), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
    }
    return { .value = std::to_string(result), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcBoolNot(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() < 2)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);

    return { .value = std::to_string(!getBooleanV(args[1].value)), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcBoolAnd(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() < 3)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    Variable& comparator = args[1];
    bool result = true;

    for (size_t i = 2; i < args.size(); i++)
    {
        if (!(getBooleanV(comparator.value) && getBooleanV(args[i].value)))
        {
            result = false;
            break;
        }
    }
    return { .value = std::to_string(result), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
}

UTTE::Variable UTTE::CoreFuncs::funcBoolOr(std::vector<Variable>& args, UTTE::Generator*) noexcept
{
    if (args.size() < 3)
        return UTTE_ERROR(UTTE_PARSE_STATUS_OUT_OF_BOUNDS);
    Variable& comparator = args[1];
    if (getBooleanV(comparator.value))
        return { .value = std::to_string(true), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };

    bool result = false;
    for (size_t i = 2; i < args.size(); i++)
    {
        if (getBooleanV(args[i].value))
        {
            result = true;
            break;
        }
    }
    return { .value = std::to_string(result), .type = UTTE_VARIABLE_TYPE_HINT_NORMAL };
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
        return { .value = std::to_string((intptr_t)nullptr), .type = UTTE_VARIABLE_TYPE_HINT_ARRAY };

    auto& arr = generator->requestArrayWithGC();
    for (size_t i = 1; i < args.size(); i++)
        arr.push_back(args[i].value);

    return UTTE::Generator::makeArray(arr);
}

bool UTTE::CoreFuncs::getBooleanV(const utte_string& str) noexcept
{
    // Description: This function generates a boolean from a boolean value represented as a keyword or as a number.
    // If a string has the value "true", "b1" will be set to "true" since that is valid C++ syntax for booleans.
    // However, it's also valid to have the value to evaluate to true using an integer. We use the integer "b2" to
    // represent integer values. If any of these returns "true" we return "true"
    bool b1;
    int b2;

    std::istringstream(str) >> std::boolalpha >> b1;
    std::istringstream(str) >> b2;

    return b1 || b2;
}

std::vector<utte_string>* UTTE::CoreFuncs::getArray(const UTTE::Variable& variable) noexcept
{
    if (variable.type != UTTE_VARIABLE_TYPE_HINT_ARRAY)
        return nullptr;

    // Get memory address of array. Arrays and maps encode their pointers as strings
    auto addr = (intptr_t)nullptr;
    std::istringstream(variable.value) >> addr;

    return (addr == (intptr_t)nullptr) ? nullptr : (std::vector<utte_string>*)addr;
}

utte_map<utte_string, utte_string>* UTTE::CoreFuncs::getMap(const UTTE::Variable& variable) noexcept
{
    if (variable.type != UTTE_VARIABLE_TYPE_HINT_MAP)
        return nullptr;

    // Get memory address of map. Arrays and maps encode their pointers as strings
    auto addr = (intptr_t)nullptr;
    std::istringstream(variable.value) >> addr;

    return (addr == (intptr_t)nullptr) ? nullptr : (utte_map<utte_string, utte_string>*)addr;
}

UTTE::Variable UTTE::CoreFuncs::funcDict(std::vector<Variable>& args, UTTE::Generator* generator) noexcept
{
    if (args.size() == 1)
        return { .value = std::to_string((intptr_t)nullptr), .type = UTTE_VARIABLE_TYPE_HINT_MAP };

    auto& map = generator->requestMapWithGC();
    for (size_t i = 1; i < args.size(); i++)
        if ((i % 2) == 0)
            map.insert({ args[i - 1].value, args[i].value });

    // This will only be called if we have odd arguments. The check is for even because the function name adds 1
    if (args.size() % 2 == 0)
        map.insert({ args.back().value, "" });

    return Generator::makeMap(map);
}
