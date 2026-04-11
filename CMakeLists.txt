cmake_minimum_required(VERSION 3.20)
project(echo VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Werror
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O0 -g3)
  add_link_options(-fsanitize=address,undefined)
  add_compile_options(-fsanitize=address,undefined)
endif()

include_directories(include)

add_library(echo_core
  source/core/chunker.c
  source/core/manifest.c
  source/core/util.c
  source/crypto/crypto_stub.c
  source/stego/stego.c
  source/providers/provider_localfs.c
  source/app/upload_file.c
  source/app/download_file.c
  source/app/verify_file.c
)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
endif()

if(TARGET PkgConfig::SODIUM)
  target_link_libraries(echo_core PUBLIC PkgConfig::SODIUM)
else()
  find_path(SODIUM_INCLUDE_DIR sodium.h)
  find_library(SODIUM_LIBRARY NAMES sodium libsodium)

  if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "libsodium not found. Install it (e.g. `brew install libsodium`) and re-run CMake.")
  endif()

  target_include_directories(echo_core PUBLIC ${SODIUM_INCLUDE_DIR})
  target_link_libraries(echo_core PUBLIC ${SODIUM_LIBRARY})
endif()

add_executable(echo
  source/cli/main.c
)

target_link_libraries(echo echo_core)

enable_testing()
add_subdirectory(tests)
