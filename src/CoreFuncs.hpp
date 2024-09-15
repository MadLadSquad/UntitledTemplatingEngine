#pragma once
#include <vector>

#ifdef UTTE_CUSTOM_STRING
    #ifdef UTTE_CUSTOM_STRING_INCLUDE
		#include UTTE_CUSTOM_STRING_INCLUDE
		typedef UTTE_CUSTOM_STRING utte_string;
	#else
		#error UTTE_CUSTOM_STRING defined but UTTE_CUSTOM_STRING_INCLUDE not defined, it is needed to include the necessary headers for the string, and should contain the name of the header wrapped in ""
	#endif
#else
    #include <string>
    typedef std::string utte_string;
#endif

#ifdef UTTE_CUSTOM_MAP
    #ifdef UTTE_CUSTOM_MAP_INCLUDE
		#include UTTE_CUSTOM_MAP_INCLUDE
        template<typename T, typename T2>
        using utte_map = UTTE_CUSTOM_MAP<T, T2>;
	#else
		#error UTTE_CUSTOM_MAP defined but UTTE_CUSTOM_MAP_INCLUDE not defined, it is needed to include the necessary headers for the string, and should contain the name of the header wrapped in ""
	#endif
#else
    #include <map>
    template<typename T, typename T2>
    using utte_map = std::map<T, T2>;
#endif

#include "Common.h"

/**
 * @brief A utility macro to set a new value to a variable from a Function&
 * @param x - Function& in question
 * @param y - Value or container that need so be passed to the capture of the internal lambda function
 * @param z - Value that the value of the new variable will be set to. Can use the value or container passed to the
 * capture as "y"
 * @param t - The type of the variable in case you want to change it
 */
#define UTTE_VARIABLE_SET_NEW_VAL(x, y, z, t) x.function = [y](std::vector<UTTE::Variable>&, UTTE::Generator*) -> UTTE::Variable { return { .value = (z), .type = (t) }; }

/**
 * @brief A utility macro to return an empty value with an error type
 * @param x - The error type as specified in the "ParseResultStatus" enum
 */
#define UTTE_ERROR(x) UTTE::Variable{ .value = "", .type = UTTE_VARIABLE_TYPE_HINT_NORMAL, .status = (x) }

namespace UTTE
{
    struct Variable;
    struct Function;
    class Generator;

    class MLS_PUBLIC_API CoreFuncs
    {
    public:
        static Variable funcIf(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcSwitch(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcAt(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcCond(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcFor(std::vector<Variable>& args, Generator* generator) noexcept;

        static Variable funcBoolEqual(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcBoolNotEqual(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcBoolNot(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcBoolAnd(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcBoolOr(std::vector<Variable>& args, Generator* generator) noexcept;

        static Variable funcFunc(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcRaw(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcComment(std::vector<Variable>& args, Generator* generator) noexcept;

        static Variable funcList(std::vector<Variable>& args, Generator* generator) noexcept;
        static Variable funcDict(std::vector<Variable>& args, Generator* generator) noexcept;

        /**
         * @brief Given a const reference to a variable, converts it to an array
         * @param variable - The reference in question
         * @return A pointer to an std::vector<std::string>. If the type does not match or the address is nullptr will
         * return nullptr. Make sure to check for it.
         */
        static std::vector<std::string>* getArray(const Variable& variable) noexcept;

        /**
         * @brief Given a const reference to a variable, converts it to a map
         * @param variable - The reference in question
         * @return A pointer to an utte_map<std::string, std::string>. If the type does not match or the address is
         * nullptr will return nullptr. Make sure to check for it.
         */
        static utte_map<std::string, std::string>* getMap(const Variable& variable) noexcept;

        // Returns a bool given a boolean value as a string
        static bool getBooleanV(const std::string& str) noexcept;
    };
}