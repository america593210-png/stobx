
cmake_minimum_required(VERSION 3.10)
project(Stobx)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)

find_path(Z3_INCLUDE_DIR z3++.h)
find_library(Z3_LIBRARY NAMES z3)

if(NOT Z3_INCLUDE_DIR OR NOT Z3_LIBRARY)
    message(FATAL_ERROR "Z3 development files not found")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(stobx_llvm
    src/main.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/CodeGen.cpp
    src/SP.cpp
    src/DiagnosticUI.cpp
    src/DiagnosticEngine.cpp
    src/CorrectionEngine.cpp
    src/DiagnosticCatalog.cpp
    src/Verification.cpp
)

llvm_map_components_to_libnames(llvm_libs core support native)

target_include_directories(stobx_llvm PRIVATE ${Z3_INCLUDE_DIR})

target_link_libraries(stobx_llvm
    ${llvm_libs}
    ${Z3_LIBRARY}
)

