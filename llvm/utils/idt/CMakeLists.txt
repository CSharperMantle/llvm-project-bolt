cmake_minimum_required(VERSION 3.20)
project(idt LANGUAGES C CXX)

# This is a vendored copy of the Interface Definition Scanner (idt) from
# https://github.com/compnerd/ids. It is built as a standalone CMake project
# against a pre-installed LLVM/Clang (LLVM_DIR / Clang_DIR), not as part of
# the main LLVM build. See README.md for context.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_INLINES_HIDDEN yes)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(LLVM REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")

find_package(Clang REQUIRED)
message(STATUS "Found Clang ${Clang_PACKAGE_VERSION}")
message(STATUS "Using ClangConfig.cmake in ${Clang_DIR}")

add_executable(idt idt.cc)

target_compile_definitions(idt PRIVATE
  ${LLVM_DEFINITIONS})

target_compile_options(idt PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/EHsc- /GR->
  $<$<CXX_COMPILER_ID:AppleClang>:-fno-exceptions -fno-rtti>
  $<$<CXX_COMPILER_ID:Clang>:-fno-exceptions -fno-rtti>
  $<$<CXX_COMPILER_ID:GNU>:-fno-exceptions -fno-rtti>)

target_include_directories(idt PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS})

target_link_libraries(idt PRIVATE
  clangRewriteFrontend
  clangTooling)
