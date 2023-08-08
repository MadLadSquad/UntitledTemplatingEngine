#pragma once
#include "../Common.h"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct UTTE_CVariable UTTE_CVariable;
    typedef void UTTE_CGenerator;
    typedef void UTTE_CFunctionHandle;

    typedef UTTE_CVariable(*UTTE_CFunctionCallback)(UTTE_CVariable*, size_t, UTTE_CGenerator*);

    typedef struct UVK_PUBLIC_API UTTE_CVariable
    {
        const char* value;
        UTTE_VariableTypeHint type;
        bool bDeallocate;
        UTTE_ParseResultStatus status;
    } UTTE_CVariable;

    typedef struct UVK_PUBLIC_API UTTE_CParseResult
    {
        UTTE_ParseResultStatus status;
        const char* result;
    } UTTE_CParseResult;

    typedef struct UVK_PUBLIC_API UTTE_CFunction
    {
        const char* name;
        UTTE_CFunctionCallback function;
        bool bDeallocate;
    } UTTE_CFunction;

    typedef struct UVK_PUBLIC_API UTTE_CPair
    {
        char* key;
        char* val;
    } UTTE_CPair;

    // Free with UTTE_CGenerator_Free
    UVK_PUBLIC_API UTTE_CGenerator* UTTE_CGenerator_Allocate();

    UVK_PUBLIC_API UTTE_InitialisationResult UTTE_CGenerator_loadFromFile(UTTE_CGenerator* generator, const char* location);
    UVK_PUBLIC_API UTTE_InitialisationResult UTTE_CGenerator_loadFromString(UTTE_CGenerator* generator, const char* str);

    UVK_PUBLIC_API UTTE_CParseResult UTTE_CGenerator_parse(UTTE_CGenerator* generator);

    // If var->bDeallocate is set to true it will automatically deallocate the value after use
    UVK_PUBLIC_API UTTE_CFunctionHandle* UTTE_CGenerator_pushVariable(UTTE_CGenerator* generator, UTTE_CVariable var, const char* name);
    // If f->bDeallocate is set to true it will automatically deallocate the value after use
    UVK_PUBLIC_API UTTE_CFunctionHandle* UTTE_CGenerator_pushFunction(UTTE_CGenerator* generator, UTTE_CFunction f);

    // If var->bDeallocate is set to true it will automatically deallocate the value after use
    UVK_PUBLIC_API bool UTTE_CGenerator_setVariable(UTTE_CGenerator* generator, const char* name, const UTTE_CVariable* variable);
    UVK_PUBLIC_API bool UTTE_CGenerator_setFunction(UTTE_CGenerator* generator, const char* name, UTTE_CFunctionCallback event);

    // Data inside the return value is heap-allocated. Either call "UTTE_CGenerator_pushVariable" or deallocate it
    // yourself by calling "UTTE_CGenerator_tryFreeCVariable"
    UVK_PUBLIC_API UTTE_CVariable UTTE_CGenerator_makeArray(UTTE_CGenerator* generator, char** arr, size_t size);

    // Data inside the return value is heap-allocated. Either call "UTTE_CGenerator_pushVariable" or deallocate it
    // yourself by calling "UTTE_CGenerator_tryFreeCVariable"
    UVK_PUBLIC_API UTTE_CVariable UTTE_CGenerator_makeMap(UTTE_CGenerator* generator, UTTE_CPair* map, size_t size);

    UVK_PUBLIC_API void UTTE_CGenerator_Free(UTTE_CGenerator* generator);

    // Named "tryFreeCVariable" because it will not free the value if "UTTE_CVariable::bDeallocate" is not set to true
    UVK_PUBLIC_API void UTTE_CGenerator_tryFreeCVariable(const UTTE_CVariable* var);

    // Modifies a function from a handle
    // If function->bDeallocate is set to true it will automatically deallocate the value after use
    //
    // If you don't want to change the name of the function set function.name to an empty string
    UVK_PUBLIC_API void UTTE_CGenerator_modify(UTTE_CFunctionHandle* handle, UTTE_CFunction function);

    // Returns the name of a function from a function handle
    UVK_PUBLIC_API const char* UTTE_CGenerator_getName(UTTE_CFunctionHandle* handle);

    // Returns a bool given a boolean value as a string
    UVK_PUBLIC_API bool UTTE_CoreFuncs_getBooleanV(const char* str);

    // Should be explicitly freed using "UTTE_CoreFuncs_freeArray"
    UVK_PUBLIC_API char** UTTE_CoreFuncs_getArray(const UTTE_CVariable* variable, size_t* size);

    // Should be explicitly freed using "UTTE_CoreFuncs_freeMap"
    UVK_PUBLIC_API UTTE_CPair* UTTE_CoreFuncs_getMap(const UTTE_CVariable* variable, size_t* size);

    UVK_PUBLIC_API void UTTE_CoreFuncs_freeArray(char** array, size_t size);

    UVK_PUBLIC_API void UTTE_CoreFuncs_freeMap(UTTE_CPair* map, size_t size);

#ifdef __cplusplus
}
#endif