#include "CGenerator.h"
#include "../Generator.hpp"

#define cast(x) ((UTTE::Generator*)x)

// Since the geniuses who standardise C think it's a good idea to add this in 2023
char* UTTE_strdup(const char* str)
{
    auto len = strlen(str) + 1;

    // + 1 for the null terminator
    auto buffer = (char*)malloc(len);
    if (buffer)
        memcpy(buffer, str, len);
    return buffer;
}

UTTE_CGenerator* UTTE_CGenerator_Allocate()
{
    return new UTTE::Generator;
}

UTTE_InitialisationResult UTTE_CGenerator_loadFromFile(UTTE_CGenerator* generator, const char* location)
{
    return cast(generator)->loadFromFile(location);
}

UTTE_InitialisationResult UTTE_CGenerator_loadFromString(UTTE_CGenerator* generator, const char* str)
{
    return cast(generator)->loadFromString(str);
}

UTTE_CParseResult UTTE_CGenerator_parse(UTTE_CGenerator* generator)
{
    auto tmp = cast(generator)->parse();

    // tmp.result->c_str() is safe to call since this is simply a pointer to the internal data. Our data is on the heap
    // in the C API anyway so no issues
    return { .status = tmp.status, .result = tmp.result->c_str() };
}

UTTE_CFunctionHandle* UTTE_CGenerator_pushVariable(UTTE_CGenerator* generator, const UTTE_CVariable var, const char* name)
{
    auto& func = cast(generator)->pushVariable({ .value = var.value, .type = var.type }, name);
    UTTE_CGenerator_tryFreeCVariable(&var);
    return &func;
}

UTTE_CFunctionHandle* UTTE_CGenerator_pushFunction(UTTE_CGenerator* generator, const UTTE_CFunction f)
{
    auto& func = cast(generator)->pushFunction({ .name = f.name, .function = [f](std::vector<UTTE::Variable>& args, UTTE::Generator* gen) -> UTTE::Variable {
        std::vector<UTTE_CVariable> cvars;
        cvars.reserve(args.size());

        for (auto& a : args)
            cvars.emplace_back(a.value.c_str(), a.type);
        auto result = f.function(cvars.data(), cvars.size(), (UTTE_CGenerator*)gen);

        UTTE::Variable ret{ .value = result.value, .type = result.type, .status = result.status };

        // Since non-string-literals will need heap allocation to be added to UTTE_CVariable safely, we can deallocate
        // them here if the user informs us using this boolean
        UTTE_CGenerator_tryFreeCVariable(&result);
        return ret;
    } });

    if (f.bDeallocate)
        free((void*)f.name);
    return &func;
}

bool UTTE_CGenerator_setVariable(UTTE_CGenerator* generator, const char* name, const UTTE_CVariable* variable)
{
    auto result = cast(generator)->setVariable(name, { .value = variable->value, .type = variable->type });
    UTTE_CGenerator_tryFreeCVariable(variable);
    return result;
}

bool UTTE_CGenerator_setFunction(UTTE_CGenerator* generator, const char* name, UTTE_CFunctionCallback event)
{
    return cast(generator)->setFunction(name, [event](std::vector<UTTE::Variable>& args, UTTE::Generator* gen) -> UTTE::Variable
    {
        std::vector<UTTE_CVariable> cvars;
        cvars.reserve(args.size());

        for (auto& a : args)
            cvars.emplace_back(a.value.c_str(), a.type);
        auto result = event(cvars.data(), cvars.size(), (UTTE_CGenerator*)gen);

        UTTE::Variable ret{ .value = result.value, .type = result.type, .status = result.status };

        // Since non-string-literals will need heap allocation to be added to UTTE_CVariable safely, we can deallocate
        // them here if the user informs us using this boolean
        UTTE_CGenerator_tryFreeCVariable(&result);
        return ret;
    });
}

UTTE_CVariable UTTE_CGenerator_makeArray(UTTE_CGenerator* generator, char** arr, size_t size)
{
    auto& vector = cast(generator)->requestArrayWithGC();
    vector.reserve(size);

    for (size_t i = 0; i < size; i++)
        vector.emplace_back(arr[i]);

    auto variable = UTTE::Generator::makeArray(vector);
    return { .value = UTTE_strdup(variable.value.c_str()), .type = variable.type, .bDeallocate = true };
}


UTTE_CVariable UTTE_CGenerator_makeMap(UTTE_CGenerator* generator, UTTE_CPair* map, size_t size)
{
    auto& dict = cast(generator)->requestMapWithGC();

    for (size_t i = 0; i < size; i++)
        dict.insert({ map[i].key, map[i].val });

    auto variable = UTTE::Generator::makeMap(dict);
    return { .value = UTTE_strdup(variable.value.c_str()), .type = variable.type, .bDeallocate = true };
}

void UTTE_CGenerator_Free(UTTE_CGenerator* generator)
{
    delete (UTTE::Generator*)generator;
}

void UTTE_CGenerator_tryFreeCVariable(const UTTE_CVariable* var)
{
    if (var->bDeallocate)
        free((void*)var->value);
}

void UTTE_CGenerator_modify(UTTE_CFunctionHandle* handle, UTTE_CFunction function)
{
    auto* f = (UTTE::Function*)handle;
    f->function = [function](std::vector<UTTE::Variable>& args, UTTE::Generator* gen) -> UTTE::Variable
    {
        std::vector<UTTE_CVariable> cvars;
        cvars.reserve(args.size());

        for (auto& a : args)
            cvars.emplace_back(a.value.c_str(), a.type);
        auto result = function.function(cvars.data(), cvars.size(), (UTTE_CGenerator*)gen);

        UTTE::Variable ret{ .value = result.value, .type = result.type };

        // Since non-string-literals will need heap allocation to be added to UTTE_CVariable safely, we can deallocate
        // them here if the user informs us using this boolean
        UTTE_CGenerator_tryFreeCVariable(&result);
        return ret;
    };
    // If given an empty string, don't change the name
    if (strlen(function.name) > 0)
        f->name = function.name;

    // Deallocate the name if needed
    if (function.bDeallocate)
        free((void*)function.name);
}

const char* UTTE_CGenerator_getName(UTTE_CFunctionHandle* handle)
{
    return ((UTTE::Function*)handle)->name.c_str();
}

bool UTTE_CoreFuncs_getBooleanV(const char* str)
{
    return UTTE::CoreFuncs::getBooleanV(str);
}

char** UTTE_CoreFuncs_getArray(const UTTE_CVariable* variable, size_t* size)
{
    auto* arr = UTTE::CoreFuncs::getArray({ .value = variable->value, .type = variable->type });
    if (arr == nullptr)
        return nullptr;

    *size = arr->size();
    auto result = (char**)malloc(arr->size() * sizeof(char*));

    for (size_t i = 0; i < arr->size(); i++)
        result[i] = UTTE_strdup((*arr)[i].c_str());
    return result;
}

UTTE_CPair* UTTE_CoreFuncs_getMap(const UTTE_CVariable* variable, size_t* size)
{
    auto* map = UTTE::CoreFuncs::getMap({ .value = variable->value, .type = variable->type });
    if (map == nullptr)
        return nullptr;
    *size = map->size();
    auto result = (UTTE_CPair*)malloc(map->size() * sizeof(UTTE_CPair));

    size_t i = 0;
    for (auto& a : *map)
    {
        if (i >= map->size())
            break;

        result[i].key = UTTE_strdup(a.first.c_str());
        result[i].val = UTTE_strdup(a.second.c_str());
        ++i;
    }

    return result;
}

void UTTE_CoreFuncs_freeArray(char** array, size_t size)
{
    for (size_t i = 0; i < size; i++)
        free((void*)array[i]);
    free((void*)array);
}

void UTTE_CoreFuncs_freeMap(UTTE_CPair* map, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        free((void*)map[i].val);
        free((void*)map[i].key);
    }
    free((void*)map);
}