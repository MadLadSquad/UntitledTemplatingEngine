#pragma once
#include <cinttypes>
#include <vector>
#include <deque>
#include <map>
#include <array>
#include <string_view>
#include <unordered_set>
#include <cstring>
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

        // The generator whose registry this function was pushed into (stamped by pushFunction). Internal: it exists
        // so the C API's UTTE_CGenerator_modify can rename a function through a bare handle — the name is the
        // registry's hash key, so a rename has to rehash the entry via the owning registry (see
        // Generator::renameFunction). May be stale for functions copied wholesale between registries (e.g. assigning
        // one getFunctionsRegistry() to another); renameFunction rejects such entries.
        Generator* _internalOwner = nullptr;
    };

    // Hash/equality for the functions registry. A Function's identity is its name only, and both functors are
    // transparent (is_transparent) so findFunction can look up by utte_string without constructing a temporary
    // Function (whose std::function member would allocate) on every expression. The name is hashed through
    // std::hash<std::string_view>, so any char-based custom utte_string works without its own std::hash
    // specialisation.
    struct MLS_PUBLIC_API FunctionHash
    {
        using is_transparent = void;

        size_t operator()(const utte_string& name) const noexcept
        {
            return std::hash<std::string_view>{}(std::string_view{ name.data(), name.size() });
        }

        size_t operator()(const Function& f) const noexcept
        {
            return operator()(f.name);
        }
    };

    struct MLS_PUBLIC_API FunctionEqual
    {
        using is_transparent = void;

        bool operator()(const Function& a, const Function& b) const noexcept { return a.name == b.name; }
        bool operator()(const Function& a, const utte_string& b) const noexcept { return a.name == b; }
        bool operator()(const utte_string& a, const Function& b) const noexcept { return a == b.name; }
    };

    // The functions registry: hashed by name so findFunction is O(1) instead of the linear scan the old
    // std::vector required on every expression. std::unordered_set is deliberately node-based: references/pointers
    // to elements (C API function handles, a for-loop's iterator variables) stay valid across later insertions,
    // which a vector or flat hash container would invalidate on growth.
    using FunctionRegistry = std::unordered_set<Function, FunctionHash, FunctionEqual>;

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

        // Renames a registered function while keeping its registry entry consistent: the name is the registry's hash
        // key, so it cannot be mutated in place — the entry is extracted, renamed and re-inserted, which preserves
        // the element's address so existing references and C API handles stay valid. `f` must be an element of THIS
        // generator's registry. Returns false if it is not, or if the new name is already taken.
        bool renameFunction(Function& f, const utte_string& newName) noexcept;

        static Variable makeArray(const std::vector<utte_string>& arr) noexcept;
        static Variable makeMap(const utte_map<utte_string, utte_string>& map) noexcept;

        // Encodes a pointer-sized integer into a string used purely as a byte buffer (a raw memcpy of the value's
        // bytes). The parameter is a std::intptr_t rather than a void* so the same routine round-trips either a real
        // pointer (reinterpret_cast it in) or a plain integer, letting callers interpret the payload as whichever they
        // need.
        static utte_string encodePointer(std::intptr_t value) noexcept;

        // Decodes a value written by encodePointer. Returns 0 when the payload is not exactly pointer-sized, which also
        // covers legacy/malformed values so callers still get a safe zero (reinterpret_cast<T*>(0) == nullptr) instead
        // of garbage. The result is a std::intptr_t so it can be used as an integer or reinterpret_cast back to a
        // pointer.
        static std::intptr_t decodePointer(const utte_string& value) noexcept;

        // Returns a reference to an array that will be garbage-collected when the generator's destructor is called.
        // This is useful for custom functions that want to return arrays without managing their own registry
        std::vector<utte_string>& requestArrayWithGC() noexcept;
        // Returns a reference to a map that will be garbage-collected when the generator's destructor is called
        // This is useful for custom functions that want to return arrays without managing their own registry
        utte_map<utte_string, utte_string>& requestMapWithGC() noexcept;

        FunctionRegistry& getFunctionsRegistry() noexcept;
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
        FunctionRegistry functions;

        // Names of the body-preserving special functions: func, raw and comment. The common thing about them is that
        // they preserve function expressions and don't execute them. For example a call like this:
        // {{ raw A b c {{ my-func }}
        // new line btw
        // }}
        // will not execute the {{ my-func }} call and will instead return a variable, whose content will be
        // "A b c {{ my-func }}
        // new line btw
        // "
        //
        // A new body-preserving function must have its name listed here (the registry is hashed by name, so there are
        // no stable indices to track any more). More information on how these functions are parsed can be found in
        // the if-branch, responsible for cutting arguments of function expressions
        static constexpr std::array<const char*, 3> specialFunctionNames{ "func", "raw", "comment" };

        // Deques (not vectors) of the arrays/maps that will be deallocated on the destruction of this class. These
        // back the "list"/"dict" functions' garbage collection. A deque is required because we hand out the *address*
        // of an element (encoded into a Variable) and keep using it later: a std::vector would reallocate on growth
        // and dangle every previously-encoded pointer, whereas a std::deque never relocates its existing elements.
        std::deque<std::vector<utte_string>> internalVectorsForList;
        std::deque<utte_map<utte_string, utte_string>> internalMapsForDict;
    };
}