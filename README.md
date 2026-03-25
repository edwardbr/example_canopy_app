# greeting_app — Canopy TCP example

A minimal two-process example using the Canopy RPC library. A `server` exposes a
`i_greeter` interface over TCP; a `client` connects, calls three methods, and exits.

## Prerequisites

- Canopy checked out at `../Canopy` (adjacent to this directory)
- clang / clang++ (default; see `CMakePresets.json` to override)
- Ninja
- OpenSSL, liburing (same requirements as Canopy coroutine builds)

## Build

```bash
cmake --preset Coroutine
cmake --build build_coroutine --target server client
```

Binaries land in `build_coroutine/output/`.

## Running

Start the server in one terminal, then run the client in another.

### Server

```
./build_coroutine/output/server [options]
```

Press **Ctrl+C** to shut down.

### Client

```
./build_coroutine/output/client [options]
```

The client connects, calls `greet`, `add`, and `get_server_info`, prints the
results, then exits cleanly.

## Command-line options

Both `server` and `client` accept the same network options (registered by
`canopy::network_config::add_network_args`):

| Option | Default | Description |
|---|---|---|
| `--host <addr>` | auto-detected | IP address to bind (server) or connect to (client). Dotted-decimal for IPv4, colon-hex for IPv6. |
| `--port <n>` | 7777 | TCP port. |
| `--routing-prefix <addr>` | auto-detected | Network routing prefix used to build the Canopy zone address. Needed when auto-detection picks the wrong interface. |
| `-4` / `--ipv4` | — | Treat `--routing-prefix` as an IPv4 address. |
| `-6` / `--ipv6` | — | Treat `--routing-prefix` as an IPv6 address. |
| `--object-offset <n>` | 64 | Bit offset of the object-id field within the zone address local payload. Rarely needs changing. |
| `-h` / `--help` | — | Print usage and exit. |

### Examples

Default (loopback, port 7777):
```bash
# terminal 1
./build_coroutine/output/server

# terminal 2
./build_coroutine/output/client
```

Custom port:
```bash
./build_coroutine/output/server --port 9000
./build_coroutine/output/client --port 9000
```

Explicit host / routing prefix (useful on multi-homed machines):
```bash
./build_coroutine/output/server --host 192.168.1.10 --routing-prefix 192.168.1.0 --port 9000
./build_coroutine/output/client --host 192.168.1.10 --routing-prefix 192.168.1.0 --port 9000
```

IPv6:
```bash
./build_coroutine/output/server -6 --routing-prefix 2001:db8:: --host 2001:db8::1
./build_coroutine/output/client -6 --routing-prefix 2001:db8:: --host 2001:db8::1
```

## IDL interface

Defined in `idl/greeting/greeting.idl`:

```
namespace greeting_app
{
    interface i_greeter
    {
        int greet([in] const std::string& name, [out] std::string& response);
        int add(int a, int b, [out] int& result);
        int get_server_info([out] std::string& info);
    };
}
```

Generated code appears in `build_coroutine/generated/` and is never written
back into the source tree.

## Project layout

```
test_app/
├── CMakeLists.txt              root build file; pulls in Canopy via add_subdirectory
├── CMakePresets.json           Coroutine (Debug) and Release_Coroutine presets
├── idl/
│   ├── CMakeLists.txt          CanopyGenerate() call
│   └── greeting/
│       └── greeting.idl        i_greeter interface definition
└── src/
    ├── server.cpp              greeter_impl + TCP listener; runs until Ctrl+C
    └── client.cpp              connects, calls all three methods, exits
```
