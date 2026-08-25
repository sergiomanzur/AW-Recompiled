find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(AW_TEST_ROM_PATH "${PROJECT_SOURCE_DIR}/rom/Advance Wars (USA) (Rev 1).gba")
set(AW_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(AW_GENERATED_HEADER ${AW_GENERATED_DIR}/aw/generated_blocks.hpp)
set(AW_GENERATED_SOURCE ${AW_GENERATED_DIR}/aw_generated_blocks.cpp)
set(AW_GENERATED_MANIFEST ${AW_GENERATED_DIR}/generated_manifest.txt)

add_custom_command(
  OUTPUT ${AW_GENERATED_HEADER} ${AW_GENERATED_SOURCE} ${AW_GENERATED_MANIFEST}
  COMMAND ${Python3_EXECUTABLE}
          ${PROJECT_SOURCE_DIR}/scripts/generate-blocks.py
          --rom "${AW_TEST_ROM_PATH}"
          --header ${AW_GENERATED_HEADER}
          --source ${AW_GENERATED_SOURCE}
          --manifest ${AW_GENERATED_MANIFEST}
  DEPENDS ${PROJECT_SOURCE_DIR}/scripts/generate-blocks.py ${AW_TEST_ROM_PATH}
  VERBATIM
)

add_library(aw_runtime STATIC
  src/arm_decode.cpp
  src/audio.cpp
  src/config_file.cpp
  src/cpu_interpreter.cpp
  src/cpu_state.cpp
  src/hardware.cpp
  src/hd_audio.cpp
  src/input_config.cpp
  src/input/source_win32.cpp
  src/input/viewport.cpp
  src/memory.cpp
  src/nav/nav_controller.cpp
  src/nav/pointer_nav.cpp
  src/order_stack.cpp
  src/map_sensor.cpp
  src/ppu.cpp
  src/probe/backend_mgba.cpp
  src/probe/context.cpp
  src/probe/cursor_probe.cpp
  src/probe/oam.cpp
  src/probe/oam_tracker.cpp
  src/render/hud_overlay.cpp
  src/render/pointer_overlay.cpp
  src/render/sidebar.cpp
  src/rewind.cpp
  src/rom.cpp
  src/tactical_intel.cpp
  src/window.cpp
  src/mgba_adapter.c
  ${AW_GENERATED_SOURCE}
)

if (WIN32)
  target_link_libraries(aw_runtime PUBLIC winmm xinput mgba)
else()
  target_link_libraries(aw_runtime PUBLIC mgba)
endif()

target_compile_definitions(aw_runtime PUBLIC
  BUILD_STATIC
  ENABLE_DEBUGGERS
  ENABLE_DIRECTORIES
  ENABLE_SCRIPTING
  ENABLE_VFS
  ENABLE_VFS_FD
  HAVE_SETLOCALE
  HAVE_STRDUP
  M_CORE_GB
  M_CORE_GBA
  USE_LZMA
)

target_include_directories(aw_runtime PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${AW_GENERATED_DIR}
  ${PROJECT_SOURCE_DIR}/third-party/mgba/include
  ${PROJECT_SOURCE_DIR}/third-party/mgba/src
  ${CMAKE_BINARY_DIR}/third-party/mgba/include
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_config.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/aw/test_config.hpp
  @ONLY
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/aw/config.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/aw/config.hpp
  @ONLY
)

add_executable(rom_metadata_tests
  tests/rom_metadata_tests.cpp
)

target_include_directories(rom_metadata_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(rom_metadata_tests PRIVATE aw_runtime)

add_test(NAME rom_metadata_tests COMMAND rom_metadata_tests)

add_executable(arm_decode_tests
  tests/arm_decode_tests.cpp
)

target_include_directories(arm_decode_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(arm_decode_tests PRIVATE aw_runtime)

add_test(NAME arm_decode_tests COMMAND arm_decode_tests)

add_executable(cpu_state_tests
  tests/cpu_state_tests.cpp
)

target_include_directories(cpu_state_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(cpu_state_tests PRIVATE aw_runtime)

add_test(NAME cpu_state_tests COMMAND cpu_state_tests)

add_executable(memory_tests
  tests/memory_tests.cpp
)

target_include_directories(memory_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(memory_tests PRIVATE aw_runtime)

add_test(NAME memory_tests COMMAND memory_tests)

add_executable(interpreter_tests
  tests/interpreter_tests.cpp
)

target_include_directories(interpreter_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(interpreter_tests PRIVATE aw_runtime)

add_test(NAME interpreter_tests COMMAND interpreter_tests)

add_executable(ppu_tests
  tests/ppu_tests.cpp
)

target_include_directories(ppu_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(ppu_tests PRIVATE aw_runtime)

add_test(NAME ppu_tests COMMAND ppu_tests)

add_executable(hardware_tests
  tests/hardware_tests.cpp
)

target_include_directories(hardware_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(hardware_tests PRIVATE aw_runtime)

add_test(NAME hardware_tests COMMAND hardware_tests)

add_executable(generated_block_tests
  tests/generated_block_tests.cpp
)

target_include_directories(generated_block_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${AW_GENERATED_DIR}
)

target_link_libraries(generated_block_tests PRIVATE aw_runtime)

add_test(NAME generated_block_tests COMMAND generated_block_tests)

add_executable(window_aspect_tests
  tests/window_aspect_tests.cpp
)

target_include_directories(window_aspect_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(window_aspect_tests PRIVATE aw_runtime)

add_test(NAME window_aspect_tests COMMAND window_aspect_tests)

add_executable(input_core_tests
  tests/input_core_tests.cpp
)

target_include_directories(input_core_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(input_core_tests PRIVATE aw_runtime)

add_test(NAME input_core_tests COMMAND input_core_tests)

add_executable(oam_tests
  tests/oam_tests.cpp
)

target_include_directories(oam_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(oam_tests PRIVATE aw_runtime)

add_test(NAME oam_tests COMMAND oam_tests)

add_executable(oam_tracker_tests
  tests/oam_tracker_tests.cpp
)

target_include_directories(oam_tracker_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(oam_tracker_tests PRIVATE aw_runtime)

add_test(NAME oam_tracker_tests COMMAND oam_tracker_tests)

add_executable(context_probe_tests
  tests/context_probe_tests.cpp
)

target_include_directories(context_probe_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(context_probe_tests PRIVATE aw_runtime)

add_test(NAME context_probe_tests COMMAND context_probe_tests)

add_executable(cursor_probe_tests
  tests/cursor_probe_tests.cpp
)

target_include_directories(cursor_probe_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(cursor_probe_tests PRIVATE aw_runtime)

add_test(NAME cursor_probe_tests COMMAND cursor_probe_tests)

add_executable(pointer_nav_tests
  tests/pointer_nav_tests.cpp
)

target_include_directories(pointer_nav_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(pointer_nav_tests PRIVATE aw_runtime)

add_test(NAME pointer_nav_tests COMMAND pointer_nav_tests)

add_executable(pointer_overlay_tests
  tests/pointer_overlay_tests.cpp
)

target_include_directories(pointer_overlay_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(pointer_overlay_tests PRIVATE aw_runtime)

add_test(NAME pointer_overlay_tests COMMAND pointer_overlay_tests)

add_executable(rewind_tests
  tests/rewind_tests.cpp
)

target_include_directories(rewind_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(rewind_tests PRIVATE aw_runtime)

add_test(NAME rewind_tests COMMAND rewind_tests)

add_executable(order_stack_tests
  tests/order_stack_tests.cpp
)

target_include_directories(order_stack_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(order_stack_tests PRIVATE aw_runtime)

add_test(NAME order_stack_tests COMMAND order_stack_tests)

add_executable(map_sensor_tests
  tests/map_sensor_tests.cpp
)

target_include_directories(map_sensor_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(map_sensor_tests PRIVATE aw_runtime)

add_test(NAME map_sensor_tests COMMAND map_sensor_tests)

add_executable(sidebar_tests
  tests/sidebar_tests.cpp
)

target_include_directories(sidebar_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(sidebar_tests PRIVATE aw_runtime)

add_test(NAME sidebar_tests COMMAND sidebar_tests)

add_executable(advance-wars-native
  src/main.cpp
)

target_include_directories(advance-wars-native PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(advance-wars-native PRIVATE aw_runtime)

add_test(
  NAME advance_wars_native_smoke
  COMMAND advance-wars-native "${AW_TEST_ROM_PATH}" --frames 120
)
set_tests_properties(advance_wars_native_smoke PROPERTIES
  TIMEOUT 120
  PASS_REGULAR_EXPRESSION "Ran 120 frames; audio samples emitted: [1-9][0-9]*"
)

add_test(
  NAME advance_wars_native_generated_trace
  COMMAND advance-wars-native "${AW_TEST_ROM_PATH}" --trace
)
set_tests_properties(advance_wars_native_generated_trace PROPERTIES
  PASS_REGULAR_EXPRESSION "Executing generated block 0x080000C0.*Executing generated Thumb block 0x080387FC.*Executing generated Thumb block 0x080386B4.*Executing generated Thumb block 0x080385CC.*Executing generated Thumb block 0x0800F1B0.*Executing generated Thumb block 0x0800F170.*Executing generated Thumb block 0x0800F1B6.*Executing generated Thumb block 0x0800F1CA.*Executing generated Thumb block 0x0801B820.*Executing generated Thumb block 0x080796C4.*Executing generated Thumb block 0x0801B840.*Executing generated Thumb block 0x08038604.*Executing generated Thumb block 0x08034C98.*Executing generated Thumb block 0x08034C74.*Executing generated Thumb block 0x08034BA8.*Executing generated Thumb block 0x080149E0.*Executing generated Thumb block 0x08014BF8.*Executing generated Thumb block 0x0807AF30.*Executing generated Thumb block 0x0807AED4.*Executing generated Thumb block 0x0807AF38.*Executing generated Thumb block 0x08014C14.*Executing generated Thumb block 0x0807B488.*Executing generated Thumb block 0x08014C20.*Stopped at unresolved target 0x08014C31"
)

add_test(
  NAME advance_wars_native_double_click_smoke
  COMMAND advance-wars-native --frames 60
)
set_tests_properties(advance_wars_native_double_click_smoke PROPERTIES
  ENVIRONMENT "AW_NATIVE_NO_PAUSE=1"
  TIMEOUT 120
  PASS_REGULAR_EXPRESSION "ROM: ADVANCEWARS AWRE01 Rev 1"
)

add_test(
  NAME advance_wars_native_rewind_smoke
  COMMAND advance-wars-native "${AW_TEST_ROM_PATH}" --frames 200 --rewind-smoke
)
set_tests_properties(advance_wars_native_rewind_smoke PROPERTIES
  ENVIRONMENT "AW_NATIVE_NO_PAUSE=1"
  TIMEOUT 120
  PASS_REGULAR_EXPRESSION "rewind smoke: restored 2/2 snapshots"
)

add_executable(aw-cursor-miner
  tools/cursor_miner.cpp
)

target_include_directories(aw-cursor-miner PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(aw-cursor-miner PRIVATE aw_runtime)

add_executable(aw-boot-probe
  tools/boot_probe.cpp
)

target_include_directories(aw-boot-probe PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(aw-boot-probe PRIVATE aw_runtime)
