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

- **Arrays/maps** are real `std::vector`/`utte_map` objects whose pointer address is stringified by `Generator::makeArray`/`makeMap` and decoded by `CoreFuncs::getArray`/`getMap`. Their backing storage is owned by the Generator (`internalVectorsForList` / `internalMapsForDict`, handed out via `requestArrayWithGC` / `requestMapWithGC`) and freed only when the Generator is destroyed — so an encoded array/map must not outlive the Generator that made it.
- **FUNCTION-typed args are deferred (unevaluated) template bodies.** Control-flow functions (`if`, `switch`, `cond`, `for`) receive these and lazily evaluate the chosen branch by spinning up a child `Generator` (the private `Generator(const Generator* parent)` constructor) that *shares* the parent's registry via a `parent` pointer rather than copying it. Name lookups (`findFunction`) search the child's own registry first — which only holds overlays such as a `for` loop's iterator variables — then walk up the parent chain, so branches see the same variables/functions while avoiding a per-call deep copy of the whole registry. The parent must outlive the child, which the control-flow functions guarantee by keeping the child on the stack.

**Function calling convention:** a stdlib/custom function has signature `Variable(std::vector<Variable>& args, Generator*)`. `args[0]` is always the function's own name; real arguments start at `args[1]`. Arity is checked by hand inside each function.

**Error handling:** functions signal failure by returning a `Variable` with a non-success `status` (use the `UTTE_ERROR(status)` macro — it yields an empty value plus the status). `Generator::parse` aborts the whole document the instant any expression returns non-success (Generator.cpp:85-87); there is no partial output on error. Control-flow functions (`if`, `switch`, `cond`) that find no matching branch and were given no fallback return an empty value rather than erroring — the fallback branch is optional. `if` therefore takes either 3 args (no else branch → empty on false) or 4 (explicit else).

## File map

- `src/Common.h` — pure C header (compiles under `extern "C"`): the `UTTE_VariableTypeHint`, `UTTE_InitialisationResult`, `UTTE_ParseResultStatus` enums and the `MLS_PUBLIC_API` dllexport macro. Shared by the C and C++ sides.
- `src/Generator.hpp` / `Generator.cpp` — the C++ API. `Generator` owns the template `data`, the `functions` registry (the stdlib is registered in the default constructor in `Generator.cpp`), and the GC pools. Child generators are built with the private `Generator(const Generator*)` constructor and resolve names through the parent chain (`findFunction` / `findSpecialFunction`) instead of holding their own copy of the stdlib. The engine is the recursive `parseFunction` plus the top-level `parse`: a single-pass, look-back scanner that mutates `data` in place via `data.replace` as expressions are evaluated.
- `src/CoreFuncs.hpp` / `CoreFuncs.cpp` — the standard library: `if`, `switch`, `cond`, `for`, `at`, the boolean ops (`==`, `!=`, `!`, `&&`, `||`), `list`, `dict`, and the body-preserving `func` / `raw` / `comment`. Plus helpers `getBooleanV`, `getArray`, `getMap`.
- `src/C/CGenerator.h` / `CGenerator.cpp` — a thin C wrapper over the C++ API using opaque `void*` handles. C callers manage memory manually; the `bDeallocate` flag on `UTTE_CVariable`/`UTTE_CFunction` opts a value into automatic cleanup.

## Adding or changing a stdlib function

Three coordinated edits: implement the function in `CoreFuncs.cpp`, declare it in `CoreFuncs.hpp`, and register `{ .name = "...", .function = ... }` in the `functions` initializer list in the `Generator` default constructor in `Generator.cpp`.

**Special (body-preserving) functions** — `func`, `raw`, `comment` — are listed by index in `Generator::specialFunctions` (`{ 0, 1, 2 }`). The parser treats these specially: it captures their raw argument text by brace-depth counting *without* evaluating inner `{{ }}` expressions, which is what makes deferred branch bodies and raw blocks possible. A new function that must receive an unevaluated body has to be added to `specialFunctions` (and its registry index kept in sync).

## Pluggable types

`Common.h`/`CoreFuncs.hpp` let the host swap the underlying containers at compile time:

- `utte_string` defaults to `std::string`; override with `-DUTTE_CUSTOM_STRING=<type>` plus `-DUTTE_CUSTOM_STRING_INCLUDE=<"header">`.
- `utte_map` defaults to `std::map`; override with `-DUTTE_CUSTOM_MAP=<template>` plus `-DUTTE_CUSTOM_MAP_INCLUDE=<...>`. UVKBuildTool builds with `UTTE_CUSTOM_MAP=phmap::flat_hash_map`.

`MLS_EXPORT_LIBRARY` (+ `MLS_LIB_COMPILE` when compiling the lib itself) gate `__declspec(dllexport/dllimport)` on Windows; elsewhere the macro is empty.

Submodule: `ThirdParty/utfcpp` (UTF-8 handling).
