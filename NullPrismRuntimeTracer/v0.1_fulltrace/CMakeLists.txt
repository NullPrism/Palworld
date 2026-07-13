cmake_minimum_required(VERSION 3.22)

set(TARGET NullPrismRuntimeTracer)
project(${TARGET} LANGUAGES CXX)

add_library(${TARGET} SHARED
    src/dllmain.cpp
)

target_include_directories(${TARGET} PRIVATE .)
target_link_libraries(${TARGET} PRIVATE UE4SS)

target_compile_features(${TARGET} PRIVATE cxx_std_23)

set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "main"
    PREFIX ""
)
