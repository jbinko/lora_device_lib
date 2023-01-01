cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(lorawan_rfm95w C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(lorawan_rfm95w
  main.c
  chip.c
  ../../../src/ldl_aes.c
  ../../../src/ldl_cmac.c
  ../../../src/ldl_ctr.c
  ../../../src/ldl_frame.c
  ../../../src/ldl_mac.c
  ../../../src/ldl_mac_commands.c
  ../../../src/ldl_ops.c
  ../../../src/ldl_radio.c
  ../../../src/ldl_region.c
  ../../../src/ldl_sm.c
  ../../../src/ldl_stream.c
  ../../../src/ldl_sx126x.c
  ../../../src/ldl_sx127x.c
)

target_compile_definitions(lorawan_rfm95w PRIVATE
  LDL_ENABLE_EU_863_870
  LDL_ENABLE_SX1276
  LDL_LL_VERSION_1_0_4
)

target_include_directories(lorawan_rfm95w PRIVATE
  ../../../include
)

target_link_libraries(lorawan_rfm95w
  pico_stdlib
  hardware_spi
)

pico_add_extra_outputs(lorawan_rfm95w)
