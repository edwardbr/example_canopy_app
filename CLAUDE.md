# test_app — Canopy TCP example

## What this project is

A minimal two-process example that uses Canopy via `add_subdirectory`. It defines
an IDL interface (`i_greeter`), generates proxy/stub code with `CanopyGenerate()`,
and connects a `server` and `client` over TCP using Canopy's coroutine streaming
transport.

## Canopy dependency

Canopy lives at `../Canopy` (adjacent directory). This project pulls it in with:

```cmake
add_subdirectory(../Canopy canopy_build)
```

No system install of Canopy is needed. All Canopy targets, headers, and generated
files appear inside this project's own binary directory.

## Where to find Canopy patterns

If you need to understand or modify anything related to Canopy:

- **`../Canopy/documents/external-project-guide.md`** — authoritative guide for
  the `add_subdirectory` pattern used here: CMake layout, IDL syntax, TCP
  server/client coroutine code, and CMake pitfalls.
- **`../Canopy/AGENTS.md`** — Canopy working practices and build commands.
- **`../Canopy/llms.txt`** — overview of the whole Canopy library.
- **`../Canopy/demos/comprehensive/`** — richer examples covering all transports.

## Build

```bash
cmake --preset Coroutine
cmake --build build_coroutine --target server client
```

Binaries land in `build_coroutine/output/`.

## Key facts

- Requires `CANOPY_BUILD_COROUTINE=ON` (set in CMakeLists.txt before add_subdirectory).
- `LANGUAGES C CXX` is required in `project()` — Canopy submodules need C.
- IDL is in `idl/greeting/greeting.idl`; generated headers go to
  `build_coroutine/generated/include/greeting/greeting.h`.
- Include generated headers as `<greeting/greeting.h>`, not `_stub.h` or `_proxy.h`.
- `CanopyGenerate(greeting …)` produces the `greeting_idl` CMake target.
- Both executables also need: `transport_streaming streaming_tcp rpc canopy_network_config ${CANOPY_LIBRARIES}`.
- Default port is 8080; both executables accept `--host` / `--port` / `--routing-prefix`.
- `rpc_log(int, const char*, size_t)` must be defined in each translation unit that
  uses Canopy logging macros when telemetry is disabled.
