cmake_minimum_required(VERSION 3.20.2)
project(MemSim VERSION 1.0)
if (CMAKE_BUILD_TYPE MATCHES "Release")
    set(COMPILE_OPTIONS
        -O3
        -fno-strict-aliasing
        -Wno-deprecated
        -Wno-unknown-warming-option
        -Wno-self-assign
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
        -Wunused)
else()
    set(COMPILE_OPTIONS
        -g0
        -Wall)
endif()

set(CMAKE_CXX_STANDARD 17)

set(SIM_FILES
    src/dram.cpp
    src/drambank.cpp
    src/dramchannel.cpp
    src/mcache.cpp
    src/mcore.cpp
    src/memsys.cpp
    src/os.cpp
    src/sim.cpp
)

# memsim executable
add_executable(sim ${SIM_FILES})
target_compile_options(sim PRIVATE ${COMPILE_OPTIONS})
target_include_directories(sim PRIVATE "src")

find_package(ZLIB)
target_link_libraries(sim PRIVATE ZLIB::ZLIB)

# compressibility executable
add_executable(cbility other/compressibility.cpp)
target_compile_options(cbility PRIVATE ${COMPILE_OPTIONS})
target_link_libraries(cbility PRIVATE ZLIB::ZLIB)

add_executable(compress_stats other/compress_stats.cpp)
target_compile_options(compress_stats PRIVATE ${COMPILE_OPTIONS})
target_link_libraries(compress_stats PRIVATE ZLIB::ZLIB)

