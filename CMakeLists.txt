cmake_minimum_required(VERSION 3.21)

project(cyrilcc VERSION 1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SRC_DIR "src")
set(PARSER_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# set(FLEX_EXECUTABLE "/opt/homebrew/opt/flex/bin/flex")
find_package(FLEX REQUIRED)
# set(BISON_EXECUTABLE "/opt/homebrew/opt/bison/bin/bison")
find_package(BISON  REQUIRED)

set(LEXER_OUT "${PARSER_DIR}/lexer.cpp")
set(PARSER_OUT "${PARSER_DIR}/parser.cpp")

flex_target(LEXER "${SRC_DIR}/lexer.l" "${LEXER_OUT}" DEFINES_FILE "${PARSER_DIR}/lexer.h")
bison_target(PARSER "${SRC_DIR}/parser.y" "${PARSER_OUT}" DEFINES_FILE "${PARSER_DIR}/parser.h")
add_flex_bison_dependency(LEXER PARSER)

add_executable(cyrilcc "${SRC_DIR}/main.cpp" "${LEXER_OUT}" "${PARSER_OUT}" "${SRC_DIR}/symbol.cpp" "${SRC_DIR}/ast.cpp")
target_include_directories(cyrilcc PRIVATE "${PARSER_DIR}" "${SRC_DIR}")


# Homebrew deploys libc++ along with the llvm package. We must point to it
# otherwise clang will use the header files of the package and link to
# system libc++. See llvm/llvm-project#155531 and Homebrew/homebrew-core#235411
# target_link_directories(cyrilcc PUBLIC /opt/homebrew/opt/llvm/lib/c++)


# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Add debug flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
