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

Both `server` and `client` register Canopy network arguments via
`canopy::network_config::add_network_args(parser)`.

### Virtual-address options

These options define the Canopy zone address allocated for the local process:

| Option | Meaning |
|---|---|
| `--va-name <identifier>` | Virtual address name. |
| `--va-type <local|ipv4|ipv6|ipv6_tun>` | Zone address type. |
| `--va-prefix <prefix>` | Routing prefix. If omitted, Canopy auto-detects it. |
| `--va-subnet-bits <n>` | Subnet field width. Default in Canopy is `64`. |
| `--va-subnet <value>` | Initial subnet value. Default in Canopy is `0`. |
| `--va-object-id-bits <n>` | Object-id field width. Default in Canopy is `64`. |
| `--va-object-id <value>` | Initial object-id value. Default in Canopy is `0`. |

### Physical endpoint options

These options define the TCP addresses used for listening and connecting:

| Option | Meaning |
|---|---|
| `--listen [va-name:]addr:port` | Server bind/listen endpoint. |
| `--connect [va-name:]addr:port` | Client connect endpoint. |
| `-h`, `--help` | Print usage and exit. |

Address family is inferred from the endpoint format:

- IPv4: `127.0.0.1:8080`
- IPv6: `[::1]:8080`
- Port-only listen form: `9090`

## Defaults in This Example

This repo adds defaults before parsing CLI arguments, so the programs run
without manually specifying Canopy network flags.

### Server defaults

If you do not pass any `--va-*` options, `server` injects:

```bash
--va-name=server
--va-type=ipv4
--va-prefix=127.0.0.1
--va-subnet-bits=32
--va-object-id-bits=32
--va-subnet=1
```

If you do not pass `--listen`, `server` injects:

```bash
--listen=server:127.0.0.1:8080
```

### Client defaults

If you do not pass any `--va-*` options, `client` injects:

```bash
--va-name=client
--va-type=ipv4
--va-prefix=127.0.0.1
--va-subnet-bits=32
--va-object-id-bits=32
--va-subnet=100
```

If you do not pass `--connect`, `client` injects:

```bash
--connect=client:127.0.0.1:8080
```

### Examples

Default (loopback, port 8080):
```bash
# terminal 1
./build_coroutine/output/server

# terminal 2
./build_coroutine/output/client
```

Custom port:
```bash
./build_coroutine/output/server --listen=server:127.0.0.1:9000
./build_coroutine/output/client --connect=client:127.0.0.1:9000
```

Custom virtual-address prefixes and endpoint port:
```bash
./build_coroutine/output/server \
  --va-name=server --va-type=ipv4 --va-prefix=192.168.1.0 \
  --va-subnet-bits=32 --va-object-id-bits=32 --va-subnet=1 \
  --listen=server:192.168.1.10:9000

./build_coroutine/output/client \
  --va-name=client --va-type=ipv4 --va-prefix=192.168.1.0 \
  --va-subnet-bits=32 --va-object-id-bits=32 --va-subnet=100 \
  --connect=client:192.168.1.10:9000
```

IPv6:
```bash
./build_coroutine/output/server \
  --va-name=server --va-type=ipv6 --va-prefix=2001:db8:: \
  --listen=server:[::1]:8080

./build_coroutine/output/client \
  --va-name=client --va-type=ipv6 --va-prefix=2001:db8:: \
  --connect=client:[::1]:8080
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
