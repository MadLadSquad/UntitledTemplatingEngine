#pragma once
#include <cinttypes>
#include <vector>
#include <map>
#include <functional>
#include "Common.h"
#include "CoreFuncs.hpp"
#include "C/CGenerator.h"

namespace UTTE
{
    typedef UTTE_VariableTypeHint VariableTypeHint;
    typedef UTTE_InitialisationResult InitialisationResult;
    typedef UTTE_ParseResultStatus ParseResultStatus;

    struct UVK_PUBLIC_API Variable
    {
        bool operator==(const Variable& variable) const noexcept;

        utte_string value{};
        VariableTypeHint type = UTTE_VARIABLE_TYPE_HINT_NORMAL;
        ParseResultStatus status = UTTE_PARSE_STATUS_SUCCESS;

        bool _internalBoolComment = false;
    };

    struct UVK_PUBLIC_API ParseResult
    {
        ParseResultStatus status = UTTE_PARSE_STATUS_SUCCESS;
        const utte_string* result = nullptr;

        Variable _internalBuffer;
    };

    using Func = Variable(std::vector<Variable>&, UTTE::Generator*);

    struct UVK_PUBLIC_API Function
    {
        utte_string name;
        std::function<Func> function = [](std::vector<Variable>&, UTTE::Generator*) -> Variable{ return {}; };
    };

    class UVK_PUBLIC_API Generator
    {
    public:
        Generator() = default;

        InitialisationResult loadFromFile(const utte_string& location, bool bReplaceInvalidCharacters = false, char32_t replaceInvalid = U'?') noexcept;
        InitialisationResult loadFromString(const utte_string& str, bool bReplaceInvalidCharacters = false, char32_t replaceInvalid = U'?') noexcept;

        ParseResult parse() noexcept;

        Function& pushVariable(const Variable& var, const utte_string& name) noexcept;
        Function& pushFunction(const Function& f) noexcept;

        bool setVariable(const char* name, const Variable& variable) noexcept;
        bool setFunction(const char* name, const std::function<Func>& event) noexcept;

        static Variable makeArray(const std::vector<utte_string>& arr) noexcept;
        static Variable makeMap(const std::map<utte_string, utte_string>& map) noexcept;

        // Returns a reference to an array that will be garbage-collected when the generator's destructor is called.
        // This is useful for custom functions that want to return arrays without managing their own registry
        std::vector<utte_string>& requestArrayWithGC() noexcept;
        // Returns a reference to a map that will be garbage-collected when the generator's destructor is called
        // This is useful for custom functions that want to return arrays without managing their own registry
        utte_map<utte_string, utte_string>& requestMapWithGC() noexcept;

        std::vector<Function>& getFunctionsRegistry() noexcept;
    private:
        friend class CoreFuncs;

        static UTTE::ParseResult parseFunction(Generator& generator, size_t& i, bool bRoot = false) noexcept;

        bool bReplaceValidCharactersI = false;
        char32_t replaceInvalidI = U'?';

        utte_string data;
        std::vector<Function> functions =
        {
            {
                .name = "func",
                .function = UTTE::CoreFuncs::funcFunc,
            },
            {
                .name = "raw",
                .function = UTTE::CoreFuncs::funcRaw
            },
            {
                .name = "comment",
                .function = UTTE::CoreFuncs::funcComment
            },
            {
                .name = "if",
                .function = UTTE::CoreFuncs::funcIf,
            },
            {
                .name = "switch",
                .function = UTTE::CoreFuncs::funcSwitch,
            },
            {
                .name = "at",
                .function = UTTE::CoreFuncs::funcAt,
            },
            {
                .name = "cond",
                .function = UTTE::CoreFuncs::funcCond,
            },
            {
                .name = "for",
                .function = UTTE::CoreFuncs::funcFor,
            },
            {
                .name = "==",
                .function = UTTE::CoreFuncs::funcBoolEqual,
            },
            {
                .name = "!=",
                .function = UTTE::CoreFuncs::funcBoolNotEqual,
            },
            {
                .name = "!",
                .function = UTTE::CoreFuncs::funcBoolNot,
            },
            {
                .name = "&&",
                .function = UTTE::CoreFuncs::funcBoolAnd,
            },
            {
                .name = "||",
                .function = UTTE::CoreFuncs::funcBoolOr,
            },
            {
                .name = "list",
                .function = UTTE::CoreFuncs::funcList
            },
            {
                .name = "dict",
                .function = UTTE::CoreFuncs::funcDict
            }
        };

        // This array has pointers to the following functions: func, raw, comment. The common thing about them is that
        // they preserve function expressions and don't execute them. For example a call like this:
        // {{ raw A b c {{ my-func }}
        // new line btw
        // }}
        // will not execute the {{ my-func }} call and will instead return a variable, whose content will be
        // "A b c {{ my-func }}
        // new line btw
        // "
        //
        // More information on how these functions are parsed can be found in the if-branch, responsible for cutting
        // arguments of function expressions
        std::vector<size_t> specialFunctions{ 0, 1, 2 };

        // This is a vector containing vectors of strings that will be deallocated on the destruction of this class.
        // This is here specifically for the "list" function to be able to garbage collect lists.
        std::vector<std::vector<utte_string>> internalVectorsForList;

        // This is a vector containing dictionaries that will be deallocated on the destruction of this class.
        // This is here specifically for the "dict" function to be able to garbage collect maps.
        std::vector<utte_map<utte_string, utte_string>> internalMapsForDict;
    };
}