# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Big picture

Apache NiFi MiNiFi C++ is a lightweight native-C++ data-collection agent, complementing the Java MiNiFi/NiFi ecosystem. It runs at or near data sources, is managed centrally over the C2 protocol, and exchanges data with NiFi over Site-to-Site. Functionality is split into a small core plus ~35 opt-in extensions loaded as dynamic libraries at runtime.

Top-level layout:

- `minifi-api/` — the stable C ABI (`minifi-api.h`) that extensions must build against. This is the only surface with binary-compatibility guarantees across releases.
- `core-framework/` — shared C++ core types built on top of the C API.
- `libminifi/` — main agent library: `FlowController`, scheduling agents (`Timer/EventDriven/CronDrivenSchedulingAgent`), C2 (`libminifi/include/c2`), repositories, expression language, Site-to-Site, parameter providers. Tests live in `libminifi/test/{unit,integration,flow-tests,persistence-tests,keyvalue-tests,schema-tests}`.
- `extension-framework/` — C++ helpers for authoring extensions (the `REGISTER_RESOURCE` macro, serialization glue, etc.).
- `extensions/` — one directory per extension (aws, azure, kafka, mqtt, sql, python, `standard-processors`, `rocksdb-repos`, `civetweb`, …). Each has its own `CMakeLists.txt`, gated by a `-DENABLE_<NAME>` flag, and its own `tests/` (typically containing a `features/` subdirectory for Behave integration tests).
- `minifi_main/` — the `minifi` executable and `MainHelper`.
- `controller/` — the `minificontroller` CLI that talks to a running agent's controller socket.
- `encrypt-config/` — the `encrypt-config` tool for encrypting flow config and sensitive properties.
- `bootstrap/` — Python-based dependency + CMake bootstrap.
- Also: `cmake/` (custom modules, including bundled-thirdparty and `Extensions.cmake`), `docker/`, `conf/`, `docs/`, `examples/`, `thirdparty/`.

## Build

Recommended: `./bootstrap/py_bootstrap.sh` (or `.\bootstrap\py_bootstrap.bat` on Windows) — spins up a venv and drives CMake interactively.

Manual:

```
mkdir build && cd build
cmake ..            # add -DENABLE_<NAME>=ON per extension, or -DENABLE_ALL=TRUE
make -j$(nproc)
```

- Extensions default to off unless listed as default-on; see the README table for the full `-DENABLE_*` flag list.
- `-DDOCKER_BUILD_ONLY=ON` short-circuits the local build and only exposes the `make docker*` targets, useful when your host toolchain is too old.
- Conan v2 builds are documented in `CONAN.md`.
- Docker: `make docker` (uses the current cmake extension selection) or `make docker-minimal` (image published to Docker Hub — standard processors + aws/azure/kafka).

## Test

C++ tests are driven by CTest from the build directory:

```
ctest --output-on-failure          # everything
ctest -R <regex> -j$(nproc)         # subset in parallel
ctest --verbose -L performance      # perf tests
```

System integration tests are Behave-based and live under each extension's `tests/features/`. Run them all in a docker matrix with `make docker-verify-modular`, or one extension at a time:

```
python -m venv .venv && source .venv/bin/activate
pip install -e behave_framework
cd extensions/<name>/tests && behavex
```

## Lint & static analysis

- `make linter` — cpplint against the Google style (with the repo's exceptions; config in `CPPLINT.cfg`).
- `make shellcheck` — shell scripts.
- `./run_flake8.sh` — PEP8 for Python.
- `./run_clang_tidy.sh <path/to/file.cpp>` — clang-tidy-20 against `build/` compile_commands, per-file; the script excludes some Windows-only dirs.

## Style conventions

Follow the Google C++ Style Guide with these deliberate, load-bearing deviations from `CONTRIBUTING.md`:

- `.cpp` implementations; `lowerCamelCase` functions/accessors; `UPPER_SNAKE_CASE` constants; enums either `UPPER_SNAKE_CASE` or `UpperCamelCase`.
- Filenames and most classes are `UpperCamelCase`; classes that imitate STL/boost use `lower_snake_case`.
- `#pragma once` over include guards; forward declarations OK.
- Exceptions, RTTI, rvalue refs, template metaprogramming, and `auto` are all allowed. `using namespace` only for user-defined-literal namespaces.
- No abseil. Use `gsl::narrow` / `gsl::narrow_cast` alongside standard casts.
- Line length isn't fixed; linter warns above 200 chars — a rare `// NOLINT` is fine when it's more readable.
- Function comments in `/** Javadoc style */`. Every new file needs the Apache License header.

## Extension / plug-in model

Extensions are shared libraries loaded at runtime, discovered via the `nifi.extension.path` property in `minifi.properties` (comma-separated glob patterns; `**` for any depth, `!` prefix to exclude). There are two ways to author one — details in `Extensions.md`:

- **C ABI** — link against only `minifi-api`, export `minifi_api_version` (equal to `MINIFI_API_VERSION`) and a `minifi_init_extension` function that calls `minifi_register_extension` / `minifi_register_processor` / etc. This is the only path with cross-version binary compatibility.
- **C++ helper** — link against `minifi-api` + `extension-framework` and register user-facing classes with

  ```cpp
  REGISTER_RESOURCE(ClassName, Processor);            // or ControllerService
  REGISTER_RESOURCE(HelperName, InternalResource);    // or DescriptionOnly
  ```

  from a `.cpp` (never a header). Extensions that need setup work (e.g. `python`) also define `minifi_init_cpp_extension`.

Extension `CMakeLists.txt` files gate themselves on their `-DENABLE_<NAME>` flag and must end with `register_extension(<target> "<label>" <CATEGORY> "<description>" [<test_dir>])` so `Extensions.cmake` picks them up.

## Runtime layout

- `conf/minifi.properties` plus any `conf/minifi.properties.d/*.properties` (applied lexicographically after the defaults) drive configuration. C2-received settings land in `conf/minifi.properties.d/90_c2.properties`.
- `conf/config.yml` is the flow definition; if C2 is enabled and no flow exists, it's fetched from the C2 server on first start.
- Run/stop/install via `./bin/minifi.sh {start|stop|install|flowStatus "…"}` on Unix; `flowstatus-minifi.bat` on Windows.
- `MINIFI_HOME` points to the install dir; set `MINIFI_INSTALLATION_TYPE=FHS` for the rpm-style layout.

## Doc pointers

- Processors and controller services: `PROCESSORS.md`, `CONTROLLERS.md`.
- Configuration keys and encryption: `CONFIGURE.md`.
- C2 protocol, response nodes, triggers: `C2.md`.
- Expression Language subset: `EXPRESSIONS.md`.
- Extension internals, loading rules: `Extensions.md`.
- Parameter providers / contexts: `PARAMETER_PROVIDERS.md`.
- Metrics: `METRICS.md`; operational commands and `flowStatus` grammar: `OPS.md`.
- Site-to-Site: `SITE_TO_SITE.md`.
- Conan-based builds: `CONAN.md`. Windows specifics: `Windows.md`.
- Contribution / style: `CONTRIBUTING.md`. Third-party handling: `ThirdParties.md`.
- Python processors: `extensions/python/PYTHON.md`.
