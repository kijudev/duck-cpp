cmake_minimum_required(VERSION 4.0)

project(
  MyProject
  VERSION 0.1.0
  DESCRIPTION "A minimal C++23 project"
  LANGUAGES C CXX
)

# Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Common interface library
add_library(common INTERFACE)


# Executables
add_executable(example_test examples/test.cpp)
target_link_libraries(example_test PRIVATE common)

add_executable(example_simple examples/simple.cpp)
target_link_libraries(example_simple PRIVATE common)

# Common interface library options
target_include_directories(common INTERFACE include)

target_compile_options(common INTERFACE
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wcast-align
  -Wcast-qual
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wold-style-cast
  -Wunused
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
  -Wimplicit-fallthrough
  -Wmisleading-indentation
)



# Disable C++ module scanning to prevent clang-scan-deps from failing in Nix
set_target_properties(common PROPERTIES CXX_SCAN_FOR_MODULES OFF)

# LDD linker
target_link_options(common INTERFACE -fuse-ld=lld)

# Sanitizer options
option(ENABLE_ASAN  "Enable AddressSanitizer"           OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN  "Enable MemorySanitizer"            OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer"            OFF)

# ==========================================================================================
# Sanitizer checks
# ==========================================================================================

if(ENABLE_ASAN AND ENABLE_MSAN)
  message(FATAL_ERROR "ASan and MSan are mutually exclusive.")
endif()

if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_MSAN))
  message(FATAL_ERROR "TSan is mutually exclusive with ASan and MSan.")
endif()

# ==========================================================================================
# Sanitizer toggles
# ==========================================================================================

if(ENABLE_ASAN)
  message(STATUS "AddressSanitizer enabled")
target_compile_options(common INTERFACE
    -fsanitize=address
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
  )
  target_link_options(common INTERFACE -fsanitize=address)
endif()

if(ENABLE_UBSAN)
  message(STATUS "UndefinedBehaviorSanitizer enabled")
  target_compile_options(common INTERFACE 
    -fsanitize=undefined
    -fno-omit-frame-pointer
  )
  target_link_options(common INTERFACE -fsanitize=undefined)
endif()

if(ENABLE_MSAN)
  message(STATUS "MemorySanitizer enabled")
  target_compile_options(common INTERFACE 
    -fsanitize=memory
    -fsanitize-memory-track-origins=2
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
  )
  target_link_options(common INTERFACE -fsanitize=memory)
endif()

if(ENABLE_TSAN)
  message(STATUS "ThreadSanitizer enabled")
  target_compile_options(common INTERFACE 
    -fsanitize=thread
    -fno-omit-frame-pointer
  )
  target_link_options(common INTERFACE -fsanitize=thread)
endif()
