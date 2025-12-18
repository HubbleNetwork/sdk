# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)


find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

target_include_directories(testbinary PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src
)

target_sources(testbinary PRIVATE
  main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/utils/bitarray.c
)
