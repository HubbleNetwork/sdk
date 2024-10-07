cmake_minimum_required(VERSION 3.20.0)
project(unit_test C)

set(CMAKE_BUILD_TYPE Debug)

# Flags needed for coverage reports
set(CMAKE_C_FLAGS "-fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS "-fprofile-arcs -ftest-coverage -coverage")

enable_testing()

# Function to create  unit tests
function(hubblenetwork_unit_test name src)
    add_executable(${name} ${src} ${ARGN})
    target_include_directories(${name} PRIVATE
        ../../include
        ../../src
    )
    add_test(NAME ${name} COMMAND "./${name}")
endfunction()

hubblenetwork_unit_test(test_bitarray
	../../src/utils.c
	test_bitarray.c)
