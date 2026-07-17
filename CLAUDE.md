# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

UntitledTemplatingEngine (UTTE) — a C and C++ templating engine driven by a small Lisp-like language. Templates are plain text containing `{{ function arg arg ... }}` expressions that the engine evaluates in a single pass. Its original consumer is [UVKBuildTool](https://github.com/MadLadSquad/UVKBuildTool) (this checkout lives inside it as `utg/`), but the engine is a standalone library. The canonical language reference is the project [wiki](https://github.com/MadLadSquad/UntitledTemplatingEngine/wiki/).

## Build / test

**This repo has no build system, no tests, and no linter of its own.** It is consumed as source: a host project adds `src/*.cpp` to its target and `src/` to its include path (e.g. UVKBuildTool does `file(GLOB_RECURSE UTG_SRC "utg/src/*.cpp")` and compiles at C++23). There is no library artifact built here.

To exercise the engine locally, drop in a driver and a CMake file — note `.gitignore` deliberately ignores `main.cpp`, `main.c`, `CMakeLists.txt`, and `build/`, which is exactly the throwaway-driver workflow:

```bash
# write your own main.cpp that includes "src/Generator.hpp", then:
cmake -B build -DCMAKE_CXX_STANDARD=23 && cmake --build build && ./build/<your-driver>
```

The only checked-in CI is `.github/workflows/release.yaml`, which just tarballs the tree on `v*` tags — it does not compile anything.

## Core model (read before editing)

**Everything is a string.** `Variable` (Generator.hpp) is the only value type: a `value` string, a `VariableTypeHint` (`NORMAL` / `ARRAY` / `MAP` / `FUNCTION`), and a `status`. Non-string data is encoded *into* the string:

- **Arrays/maps** are real `std::vector`/`utte_map` objects whose pointer is encoded into the `Variable`'s string by `Generator::makeArray`/`makeMap` and decoded by `CoreFuncs::getArray`/`getMap`. The encoding is a raw byte copy: `Generator::encodePointer`/`decodePointer` take/return a `std::intptr_t` and `memcpy` its `sizeof(std::intptr_t)` (== `sizeof(void*)`) bytes to/from the string (no decimal stringification / `istringstream` round-trip); `makeArray`/`makeMap` `reinterpret_cast` the pointer to `intptr_t` on the way in and `getArray`/`getMap` cast it back out. The parameter/return is a plain integer type (not `void*`) so the same pair can round-trip an integer as well as a pointer. `decodePointer` returns `0` unless the payload is exactly pointer-sized, so malformed/legacy values decode safely (and `reinterpret_cast<T*>(0)` is `nullptr`). Their backing storage is owned by the Generator (`internalVectorsForList` / `internalMapsForDict`, handed out via `requestArrayWithGC` / `requestMapWithGC`) and freed only when the Generator is destroyed — so an encoded array/map must not outlive the Generator that made it. **These pools are `std::deque`, not `std::vector`, on purpose:** we hand out the address of a pool element and keep using it, so the pool must never relocate its existing elements on growth. Empty `{{ list }}` / `{{ dict }}` still allocate a real (empty) pool entry rather than encoding a null, so `for`/`at` see a valid empty collection.
- **FUNCTION-typed args are deferred (unevaluated) template bodies.** Control-flow functions (`if`, `switch`, `cond`, `for`) receive these and lazily evaluate the chosen branch by spinning up a child `Generator` (the private `Generator(const Generator* parent)` constructor) that *shares* the parent's registry via a `parent` pointer rather than copying it. Name lookups (`findFunction`) search the child's own registry first — which only holds overlays such as a `for` loop's iterator variables — then walk up the parent chain, so branches see the same variables/functions while avoiding a per-call deep copy of the whole registry. The parent must outlive the child, which the control-flow functions guarantee by keeping the child on the stack.

**Function calling convention:** a stdlib/custom function has signature `Variable(std::vector<Variable>& args, Generator*)`. `args[0]` is always the function's own name; real arguments start at `args[1]`. Arity is checked by hand inside each function.

**Error handling:** functions signal failure by returning a `Variable` with a non-success `status` (use the `UTTE_ERROR(status)` macro — it yields an empty value plus the status). `Generator::parse` aborts the whole document the instant any expression returns non-success (Generator.cpp:132-134); there is no partial output on error. Nested-expression errors propagate too — `parseFunction` checks the recursive call's status (`res.status`) before continuing. Control-flow functions (`if`, `switch`, `cond`) that find no matching branch and were given no fallback return an empty value rather than erroring — the fallback branch is optional. `if` therefore takes either 3 args (no else branch → empty on false) or 4 (explicit else). Unlike `switch`/`cond` — whose value/branch pairs are distinguished purely by type, so their branches *must* be FUNCTION-typed — `if` only ever inspects the single branch it selects and accepts it in either form: a deferred `{{ func ... }}` body (FUNCTION, re-parsed lazily so the untaken branch is never rendered) or a plain already-evaluated value (any other type — the parser rendered it eagerly while cutting arguments, e.g. a bare `{{ if cond {{ some_var }} }}`), which is returned verbatim.

## File map

- `src/Common.h` — pure C header (compiles under `extern "C"`): the `UTTE_VariableTypeHint`, `UTTE_InitialisationResult`, `UTTE_ParseResultStatus` enums and the `MLS_PUBLIC_API` dllexport macro. Shared by the C and C++ sides.
- `src/Generator.hpp` / `Generator.cpp` — the C++ API. `Generator` owns the template `data`, the `functions` registry (a `FunctionRegistry` — an `std::unordered_set<Function>` hashed/compared by name via the transparent `FunctionHash`/`FunctionEqual` functors, so `findFunction` is O(1); it is node-based on purpose, so references/handles to registered functions survive later pushes. The stdlib is registered in the default constructor in `Generator.cpp`, and renames must go through `Generator::renameFunction`, which rehashes the entry without moving it), and the GC pools. Child generators are built with the private `Generator(const Generator*)` constructor and resolve names through the parent chain (`findFunction` / `findSpecialFunction`) instead of holding their own copy of the stdlib. The engine is the recursive `parseFunction` plus the top-level `parse`: a single-pass, look-back scanner that mutates `data` in place via `data.replace` as expressions are evaluated.
- `src/CoreFuncs.hpp` / `CoreFuncs.cpp` — the standard library: `if`, `switch`, `cond`, `for`, `at`, the boolean ops (`==`, `!=`, `!`, `&&`, `||`), `list`, `dict`, and the body-preserving `func` / `raw` / `comment`. Plus helpers `getBooleanV`, `getArray`, `getMap`.
- `src/C/CGenerator.h` / `CGenerator.cpp` — a thin C wrapper over the C++ API using opaque `void*` handles. C callers manage memory manually; the `bDeallocate` flag on `UTTE_CVariable`/`UTTE_CFunction` opts a value into automatic cleanup. **Encoded array/map values are binary, not C strings:** an `ARRAY`/`MAP` `UTTE_CVariable.value` holds a raw `sizeof(intptr_t)`-byte encoded pointer that can contain embedded `\0`, so the wrapper never `strlen`/`strdup`s it — it copies by length (`UTTE_dupValueBytes`) and rebuilds C++ strings length-aware (`UTTE_toVariable`). C code that plugs in its own functions encodes/decodes these with `UTTE_CGenerator_encodePointer(intptr_t)` / `UTTE_CoreFuncs_decodePointer(const char*) -> intptr_t` (the C counterparts of `Generator::encodePointer`/`decodePointer` — cast a pointer to/from `intptr_t`, and `decodePointer` yields `0` on a NULL/short value); never call `strlen` on such a value.

## Adding or changing a stdlib function

Three coordinated edits: implement the function in `CoreFuncs.cpp`, declare it in `CoreFuncs.hpp`, and register `{ .name = "...", .function = ... }` in the `functions` initializer list in the `Generator` default constructor in `Generator.cpp`.

**Special (body-preserving) functions** — `func`, `raw`, `comment` — are listed by name in `Generator::specialFunctionNames`. The parser treats these specially: it captures their raw argument text by brace-depth counting *without* evaluating inner `{{ }}` expressions, which is what makes deferred branch bodies and raw blocks possible. A new function that must receive an unevaluated body has to have its name added to `specialFunctionNames`.

The captured body is preserved verbatim: only the single separator after the function name is consumed, so the trailing whitespace before the closing `}}` is kept. `{{ raw foo }}` therefore yields `"foo "` (note the trailing space), not `"foo"`. This is intentional — `raw` (and the other body-preserving functions) must not mangle whitespace — so comparisons like `{{ == {{ raw foo }} foo }}` will *not* match a bare token; trim on the consumer side if you need an exact match.

## Pluggable types

`Common.h`/`CoreFuncs.hpp` let the host swap the underlying containers at compile time:

- `utte_string` defaults to `std::string`; override with `-DUTTE_CUSTOM_STRING=<type>` plus `-DUTTE_CUSTOM_STRING_INCLUDE=<"header">`.
- `utte_map` defaults to `std::map`; override with `-DUTTE_CUSTOM_MAP=<template>` plus `-DUTTE_CUSTOM_MAP_INCLUDE=<...>`. UVKBuildTool builds with `UTTE_CUSTOM_MAP=phmap::flat_hash_map`.

`MLS_EXPORT_LIBRARY` (+ `MLS_LIB_COMPILE` when compiling the lib itself) gate `__declspec(dllexport/dllimport)` on Windows; elsewhere the macro is empty.

Submodule: `ThirdParty/utfcpp` (UTF-8 handling).
