cmake_minimum_required(VERSION 3.24)
project(
  greeting_app
  VERSION 1.0.0
  LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Pull in Canopy from the adjacent directory. Set options before add_subdirectory so Canopy picks them up.
# ---------------------------------------------------------------------------
set(CANOPY_BUILD_COROUTINE
    ON
    CACHE BOOL "" FORCE)
set(CANOPY_BUILD_TEST
    OFF
    CACHE BOOL "" FORCE)
set(CANOPY_BUILD_DEMOS
    OFF
    CACHE BOOL "" FORCE)
set(CANOPY_BUILD_BENCHMARKING
    OFF
    CACHE BOOL "" FORCE)

add_subdirectory(../Canopy canopy_build)

# Output layout
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

# ---------------------------------------------------------------------------
# IDL code generation
# ---------------------------------------------------------------------------
add_subdirectory(idl)

# ---------------------------------------------------------------------------
# server executable
# ---------------------------------------------------------------------------
add_executable(server src/server.cpp)
target_compile_definitions(server PRIVATE ${CANOPY_DEFINES})
target_include_directories(server PRIVATE ${CANOPY_INCLUDES})
target_compile_options(server PRIVATE ${CANOPY_COMPILE_OPTIONS} ${CANOPY_WARN_OK})
target_link_options(server PRIVATE ${CANOPY_LINK_EXE_OPTIONS})
target_link_libraries(
  server
  PRIVATE greeting_idl
          transport_streaming
          streaming_tcp
          rpc
          canopy_network_config
          ${CANOPY_LIBRARIES})

# ---------------------------------------------------------------------------
# client executable
# ---------------------------------------------------------------------------
add_executable(client src/client.cpp)
target_compile_definitions(client PRIVATE ${CANOPY_DEFINES})
target_include_directories(client PRIVATE ${CANOPY_INCLUDES})
target_compile_options(client PRIVATE ${CANOPY_COMPILE_OPTIONS} ${CANOPY_WARN_OK})
target_link_options(client PRIVATE ${CANOPY_LINK_EXE_OPTIONS})
target_link_libraries(
  client
  PRIVATE greeting_idl
          transport_streaming
          streaming_tcp
          rpc
          canopy_network_config
          ${CANOPY_LIBRARIES})

message(STATUS "greeting_app configured")
message(STATUS "  server: listens on TCP 127.0.0.1:8080")
message(STATUS "  client: connects and calls greet/add/get_server_info")
