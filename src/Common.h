#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef MLS_EXPORT_LIBRARY
    #ifdef _WIN32
        #ifdef MLS_LIB_COMPILE
            #define MLS_PUBLIC_API __declspec(dllexport)
        #else
            #define MLS_PUBLIC_API __declspec(dllimport)
        #endif
    #else
        #define MLS_PUBLIC_API
    #endif
#else
    #define MLS_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    /**
    * @brief A variable type hint is an enum, whose members can be used as a hint to what the given value might be
    * Keep in mind that there is only 1 type of value, a string. Anything else is encoded into a string.
    * For example, a function like `foreach` might take 3 arguments, one for the name of the iterator variable,
    * one for the array to iterate and one for a function that will represent the body. In this case the arguments will
    * be with the following hints: Normal, Array, Function.
    * @enum UTTE_VARIABLE_TYPE_HINT_NORMAL - A normal string
    * @enum UTTE_VARIABLE_TYPE_HINT_ARRAY - A string, encoded as an array. You can easily encode your string array
    * using the static `Generator::makeArray` function.
    * @enum UTTE_VARIABLE_TYPE_HINT_MAP - A string, encoded as a map. You can easily encode your string array using the
    * static `Generator::makeMap` function
    * @enum UTTE_VARIABLE_TYPE_HINT_FUNCTION - A string encoded as a function. Use the `function` function in code to
    * generate such a string. This string can be passed to the static `Generator::parseFunction` to run and get the
    * return value of it
    */
    typedef enum UTTE_VariableTypeHint
    {
        UTTE_VARIABLE_TYPE_HINT_NORMAL = 0,
        UTTE_VARIABLE_TYPE_HINT_ARRAY = 1,
        UTTE_VARIABLE_TYPE_HINT_MAP = 2,
        UTTE_VARIABLE_TYPE_HINT_FUNCTION = 3,
    } UTTE_VariableTypeHint;

    // Result after initialising the parser with a string or file. These, especially
    // UTTE_INITIALISATION_RESULT_INVALID_UTF8, do not affect the generator's internals in any way, so if a string has
    // an invalid UTF-8 string the internal string in the generator will still be initialised. This is due to invalid
    // character replacement strategies in the loadX members, part of the Generator class
    typedef enum UTTE_InitialisationResult
    {
        UTTE_INITIALISATION_RESULT_SUCCESS = 0,
        UTTE_INITIALISATION_RESULT_INVALID_FILE = 1,
    } UTTE_InitialisationResult;

    /**
    * @brief Result after initialising the parser with a string or file
    * @enum UTTE_PARSE_STATUS_SUCCESS - We parsed the file without any errors
    * @enum UTTE_PARSE_STATUS_OUT_OF_BOUNDS - An array or map was accessed with an index beyond its size
    * @enum UTTE_PARSE_STATUS_EXPECTED_TERMINATION - Parsing a function stated, but it was not terminated by a normal
    * function call termination character, instead an EOF or '\0' character was encountered that stopped iteration of
    * the input string/file
    */
    typedef enum UTTE_ParseResultStatus
    {
        UTTE_PARSE_STATUS_SUCCESS = 0,
        UTTE_PARSE_STATUS_OUT_OF_BOUNDS = 1,
        UTTE_PARSE_STATUS_EXPECTED_TERMINATION = 2,
        UTTE_PARSE_STATUS_INVALID_VALUE = 3,
        UTTE_PARSE_STATUS_INVALID_TYPE = 4,
    } UTTE_ParseResultStatus;
#ifdef __cplusplus
}
#endif