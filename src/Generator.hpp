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

    struct MLS_PUBLIC_API Variable
    {
        bool operator==(const Variable& variable) const noexcept;

        utte_string value{};
        VariableTypeHint type = UTTE_VARIABLE_TYPE_HINT_NORMAL;
        ParseResultStatus status = UTTE_PARSE_STATUS_SUCCESS;

        bool _internalBoolComment = false;
    };

    struct MLS_PUBLIC_API ParseResult
    {
        ParseResultStatus status = UTTE_PARSE_STATUS_SUCCESS;
        const utte_string* result = nullptr;

        Variable _internalBuffer;
    };

    using Func = Variable(std::vector<Variable>&, UTTE::Generator*);

    struct MLS_PUBLIC_API Function
    {
        utte_string name;
        std::function<Func> function = [](std::vector<Variable>&, UTTE::Generator*) -> Variable{ return {}; };
    };

    class MLS_PUBLIC_API Generator
    {
    public:
        // Constructs a root generator: installs the standard library into the functions registry.
        Generator() noexcept;

        InitialisationResult loadFromFile(const utte_string& location) noexcept;
        InitialisationResult loadFromString(const utte_string& str) noexcept;

        ParseResult parse() noexcept;

        Function& pushVariable(const Variable& var, const utte_string& name) noexcept;
        Function& pushFunction(const Function& f) noexcept;

        bool setVariable(const char* name, const Variable& variable) noexcept;
        bool setFunction(const char* name, const std::function<Func>& event) noexcept;

        static Variable makeArray(const std::vector<utte_string>& arr) noexcept;
        static Variable makeMap(const utte_map<utte_string, utte_string>& map) noexcept;

        // Returns a reference to an array that will be garbage-collected when the generator's destructor is called.
        // This is useful for custom functions that want to return arrays without managing their own registry
        std::vector<utte_string>& requestArrayWithGC() noexcept;
        // Returns a reference to a map that will be garbage-collected when the generator's destructor is called
        // This is useful for custom functions that want to return arrays without managing their own registry
        utte_map<utte_string, utte_string>& requestMapWithGC() noexcept;

        std::vector<Function>& getFunctionsRegistry() noexcept;
    private:
        friend class CoreFuncs;

        // Constructs a child generator. It owns no standard library of its own: name lookups fall through to the
        // parent's registry via the chain walked by findFunction/findSpecialFunction. This avoids deep-copying the
        // whole functions registry on every control-flow (if/switch/cond/for) invocation. The parent must outlive the
        // child, which the control-flow functions guarantee by keeping the child on the stack.
        explicit Generator(const Generator* parentGenerator) noexcept : parent(parentGenerator) {}

        static UTTE::ParseResult parseFunction(Generator& generator, size_t& i, bool bRoot = false) noexcept;

        // Resolves a function/variable by name, searching this generator's own registry first and then walking up the
        // parent chain. Returns nullptr if no match exists anywhere in the chain.
        [[nodiscard]] const Function* findFunction(const utte_string& name) const noexcept;
        // Resolves a body-preserving special function (func/raw/comment) by name. These always live in the root
        // generator's registry, so the lookup walks to the root and consults its specialFunctions indices.
        [[nodiscard]] const Function* findSpecialFunction(const utte_string& name) const noexcept;

        utte_string data;

        // The parent generator in a control-flow lookup chain, or nullptr for a root generator. Used by findFunction
        // to resolve names that this generator's own registry does not contain.
        const Generator* parent = nullptr;

        // For a root generator this holds the standard library plus any user-pushed variables/functions. For a child
        // generator it holds only the overlay (e.g. a for-loop's iterator variables); everything else resolves through
        // the parent chain.
        std::vector<Function> functions;

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