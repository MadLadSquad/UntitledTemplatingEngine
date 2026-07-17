#include "CGenerator.h"
#include "../Generator.hpp"

#define cast(x) ((UTTE::Generator*)(x))

// Since the geniuses who standardise C think it's a good idea to add this in 2023
char* UTTE_strdup(const char* str)
{
    const auto len = strlen(str) + 1;

    // + 1 for the null terminator
    const auto buffer = static_cast<char*>(malloc(len));
    if (buffer)
        memcpy(buffer, str, len);
    return buffer;
}

// Copies a value into a fresh heap buffer of exactly its byte length. Unlike UTTE_strdup this does not stop at the
// first '\0', so it is safe for the raw-byte pointer payloads that back arrays/maps (whose bytes commonly contain
// embedded zeros).
static char* UTTE_dupValueBytes(const utte_string& value)
{
    const auto size = value.size();
    const auto buffer = static_cast<char*>(malloc(size));
    if (buffer)
        memcpy(buffer, value.data(), size);
    return buffer;
}

// Reconstructs a C++ Variable from a UTTE_CVariable crossing the C boundary. ARRAY/MAP values are a raw
// sizeof(intptr_t)-byte encoded pointer that may contain embedded '\0', so they are taken by explicit length; every
// other value is an ordinary NUL-terminated C string. This is what stops encoded array/map pointers from being silently
// truncated by the implicit std::string(const char*) constructor.
static UTTE::Variable UTTE_toVariable(const UTTE_CVariable& var)
{
    UTTE::Variable out;
    out.type = var.type;
    out.status = var.status;
    if (var.value != nullptr)
    {
        if (var.type == UTTE_VARIABLE_TYPE_HINT_ARRAY || var.type == UTTE_VARIABLE_TYPE_HINT_MAP)
            out.value.assign(var.value, sizeof(intptr_t));
        else
            out.value.assign(var.value);
    }
    return out;
}

UTTE_CGenerator* UTTE_CGenerator_allocate()
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
    const auto tmp = cast(generator)->parse();

    // tmp.result->c_str() is safe to call since this is simply a pointer to the internal data. Our data is on the heap
    // in the C API anyway so no issues
    return { .status = tmp.status, .result = tmp.result->c_str() };
}

UTTE_CFunctionHandle* UTTE_CGenerator_pushVariable(UTTE_CGenerator* generator, const UTTE_CVariable var, const char* name)
{
    // A NULL name cannot become a registry key. Still honour bDeallocate so the mistake doesn't also leak the value
    if (name == nullptr)
    {
        UTTE_CGenerator_tryFreeCVariable(&var);
        return nullptr;
    }

    auto& func = cast(generator)->pushVariable(UTTE_toVariable(var), name);
    UTTE_CGenerator_tryFreeCVariable(&var);
    return &func;
}

UTTE_CFunctionHandle* UTTE_CGenerator_pushFunction(UTTE_CGenerator* generator, const UTTE_CFunction f)
{
    // A NULL name cannot become a registry key (and there is nothing to deallocate for it)
    if (f.name == nullptr)
        return nullptr;

    auto& func = cast(generator)->pushFunction({ .name = f.name, .function = [f](const std::vector<UTTE::Variable>& args, UTTE::Generator* gen) -> UTTE::Variable {
        std::vector<UTTE_CVariable> cvars;
        cvars.reserve(args.size());

        for (auto& a : args)
            cvars.push_back({ .value = a.value.c_str(), .type = a.type });
        const auto result = f.function(cvars.data(), cvars.size(), static_cast<UTTE_CGenerator*>(gen));

        UTTE::Variable ret = UTTE_toVariable(result);

        // Since non-string-literals will need heap allocation to be added to UTTE_CVariable safely, we can deallocate
        // them here if the user informs us using this boolean
        UTTE_CGenerator_tryFreeCVariable(&result);
        return ret;
    } });

    if (f.bDeallocate)
        free(const_cast<char*>(f.name));
    return &func;
}

bool UTTE_CGenerator_setVariable(UTTE_CGenerator* generator, const char* name, const UTTE_CVariable* variable)
{
    // A NULL name can never match a registry entry. Still honour bDeallocate so the mistake doesn't also leak the value
    if (name == nullptr)
    {
        UTTE_CGenerator_tryFreeCVariable(variable);
        return false;
    }

    const auto result = cast(generator)->setVariable(name, UTTE_toVariable(*variable));
    UTTE_CGenerator_tryFreeCVariable(variable);
    return result;
}

bool UTTE_CGenerator_setFunction(UTTE_CGenerator* generator, const char* name, UTTE_CFunctionCallback event)
{
    // A NULL name can never match a registry entry
    if (name == nullptr)
        return false;

    return cast(generator)->setFunction(name, [event](std::vector<UTTE::Variable>& args, UTTE::Generator* gen) -> UTTE::Variable
    {
        std::vector<UTTE_CVariable> cvars;
        cvars.reserve(args.size());

        for (auto& a : args)
            cvars.push_back({ .value = a.value.c_str(), .type = a.type });
        const auto result = event(cvars.data(), cvars.size(), static_cast<UTTE_CGenerator*>(gen));

        UTTE::Variable ret = UTTE_toVariable(result);

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

    const auto variable = UTTE::Generator::makeArray(vector);
    // The encoded pointer is raw bytes (may contain '\0'), so copy by length — UTTE_strdup would truncate it.
    return { .value = UTTE_dupValueBytes(variable.value), .type = variable.type, .bDeallocate = true };
}


UTTE_CVariable UTTE_CGenerator_makeMap(UTTE_CGenerator* generator, UTTE_CPair* map, size_t size)
{
    auto& dict = cast(generator)->requestMapWithGC();

    for (size_t i = 0; i < size; i++)
        dict.insert({ map[i].key, map[i].val });

    const auto variable = UTTE::Generator::makeMap(dict);
    // The encoded pointer is raw bytes (may contain '\0'), so copy by length — UTTE_strdup would truncate it.
    return { .value = UTTE_dupValueBytes(variable.value), .type = variable.type, .bDeallocate = true };
}

char* UTTE_CGenerator_encodePointer(intptr_t value)
{
    // Mirrors UTTE::Generator::encodePointer, then copies the raw bytes into a caller-owned heap buffer following the
    // same ownership convention as makeArray/makeMap (free() it, or hand it to a UTTE_CVariable with bDeallocate=true).
    return UTTE_dupValueBytes(UTTE::Generator::encodePointer(value));
}

void UTTE_CGenerator_free(UTTE_CGenerator* generator)
{
    delete static_cast<UTTE::Generator*>(generator);
}

void UTTE_CGenerator_tryFreeCVariable(const UTTE_CVariable* var)
{
    if (var->bDeallocate)
        free(const_cast<char*>(var->value));
}

void UTTE_CGenerator_modify(UTTE_CFunctionHandle* handle, UTTE_CFunction function)
{
    auto* f = static_cast<UTTE::Function*>(handle);
    f->function = [function](const std::vector<UTTE::Variable>& args, UTTE::Generator* gen) -> UTTE::Variable
    {
        std::vector<UTTE_CVariable> cvars;
        cvars.reserve(args.size());

        for (auto& a : args)
            cvars.push_back({ .value = a.value.c_str(), .type = a.type });
        const auto result = function.function(cvars.data(), cvars.size(), static_cast<UTTE_CGenerator*>(gen));

        UTTE::Variable ret = UTTE_toVariable(result);

        // Since non-string-literals will need heap allocation to be added to UTTE_CVariable safely, we can deallocate
        // them here if the user informs us using this boolean
        UTTE_CGenerator_tryFreeCVariable(&result);
        return ret;
    };
    // If given a NULL or empty name, don't change the name. A rename must go through the owning generator's
    // registry — the name is the registry's hash key, so it cannot be assigned in place. renameFunction rehashes the
    // entry without moving it, so this handle stays valid afterwards. A function that was never pushed through a
    // generator (no owner) cannot be renamed.
    if (function.name != nullptr && function.name[0] != '\0' && f->_internalOwner != nullptr)
        f->_internalOwner->renameFunction(*f, function.name);

    // Deallocate the name if needed
    if (function.bDeallocate)
        free(const_cast<char*>(function.name));
}

const char* UTTE_CGenerator_getName(UTTE_CFunctionHandle* handle)
{
    return static_cast<UTTE::Function*>(handle)->name.c_str();
}

bool UTTE_CoreFuncs_getBooleanV(const char* str)
{
    return UTTE::CoreFuncs::getBooleanV(str);
}

intptr_t UTTE_CoreFuncs_decodePointer(const char* value)
{
    if (value == nullptr)
        return 0;
    // The C boundary carries no length, but an encoded value is always exactly sizeof(intptr_t) bytes, so rebuild a
    // length-correct string and defer to the C++ decoder (which also range-checks the size and returns 0 on mismatch).
    return UTTE::Generator::decodePointer(utte_string(value, sizeof(intptr_t)));
}

char** UTTE_CoreFuncs_getArray(const UTTE_CVariable* variable, size_t* size)
{
    const auto* arr = UTTE::CoreFuncs::getArray(UTTE_toVariable(*variable));
    if (arr == nullptr)
        return nullptr;

    // + 1 so an empty array still yields a non-NULL buffer — malloc(0) may legally return NULL, which callers could
    // not tell apart from the error return
    const auto result = static_cast<char**>(malloc((arr->size() + 1) * sizeof(char*)));
    if (result == nullptr)
        return nullptr;

    for (size_t i = 0; i < arr->size(); i++)
    {
        result[i] = UTTE_strdup((*arr)[i].c_str());
        if (result[i] == nullptr)
        {
            // Roll back the elements duplicated so far so an allocation failure doesn't leak
            UTTE_CoreFuncs_freeArray(result, i);
            return nullptr;
        }
    }
    *size = arr->size();
    return result;
}

UTTE_CPair* UTTE_CoreFuncs_getMap(const UTTE_CVariable* variable, size_t* size)
{
    auto* map = UTTE::CoreFuncs::getMap(UTTE_toVariable(*variable));
    if (map == nullptr)
        return nullptr;

    // + 1 so an empty map still yields a non-NULL buffer — malloc(0) may legally return NULL, which callers could
    // not tell apart from the error return
    const auto result = static_cast<UTTE_CPair*>(malloc((map->size() + 1) * sizeof(UTTE_CPair)));
    if (result == nullptr)
        return nullptr;

    size_t i = 0;
    for (auto& a : *map)
    {
        result[i].key = UTTE_strdup(a.first.c_str());
        result[i].val = UTTE_strdup(a.second.c_str());
        if (result[i].key == nullptr || result[i].val == nullptr)
        {
            // Free the partially-filled pair (free(NULL) is a no-op), then roll back the fully duplicated ones so an
            // allocation failure doesn't leak
            free(result[i].key);
            free(result[i].val);
            UTTE_CoreFuncs_freeMap(result, i);
            return nullptr;
        }
        ++i;
    }
    *size = map->size();
    return result;
}

void UTTE_CoreFuncs_freeArray(char** array, const size_t size)
{
    for (size_t i = 0; i < size; i++)
        free(array[i]);
    free(array);
}

void UTTE_CoreFuncs_freeMap(UTTE_CPair* map, const size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        free(map[i].val);
        free(map[i].key);
    }
    free(map);
}