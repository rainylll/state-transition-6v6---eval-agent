cmake_minimum_required(VERSION 3.21)

project(SeaAirMacroEvalAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SEA_AIR_ENABLE_TACVIEW_SERVER_DEFAULT OFF)
set(SEA_AIR_ENABLE_WGUA_DEFAULT OFF)
set(SEA_AIR_ENABLE_JOYSTICK_DEFAULT OFF)
set(SEA_AIR_ENABLE_ACMI_FILE_DEFAULT ON)

if(WIN32)
  set(SEA_AIR_ENABLE_TACVIEW_SERVER_DEFAULT ON)
  set(SEA_AIR_ENABLE_WGUA_DEFAULT ON)
  set(SEA_AIR_ENABLE_JOYSTICK_DEFAULT ON)
endif()

option(SEA_AIR_ENABLE_TACVIEW_SERVER "Enable TacView real-time telemetry server." ${SEA_AIR_ENABLE_TACVIEW_SERVER_DEFAULT})
option(SEA_AIR_ENABLE_WGUA "Enable WGUA integration and runtime bridge." ${SEA_AIR_ENABLE_WGUA_DEFAULT})
option(SEA_AIR_ENABLE_JOYSTICK "Enable Windows joystick input path." ${SEA_AIR_ENABLE_JOYSTICK_DEFAULT})
option(SEA_AIR_ENABLE_ACMI_FILE "Enable ACMI replay file output." ${SEA_AIR_ENABLE_ACMI_FILE_DEFAULT})

if(NOT WIN32)
  foreach(_feature IN ITEMS SEA_AIR_ENABLE_TACVIEW_SERVER SEA_AIR_ENABLE_WGUA SEA_AIR_ENABLE_JOYSTICK)
    if(${_feature})
      message(WARNING "${_feature} is only supported on Windows; forcing it OFF for this build.")
      set(${_feature} OFF CACHE BOOL "" FORCE)
    endif()
  endforeach()
endif()

function(add_bool_compile_definition target name value)
  if(${value})
    target_compile_definitions(${target} PRIVATE ${name}=1)
  else()
    target_compile_definitions(${target} PRIVATE ${name}=0)
  endif()
endfunction()

function(ensure_nlohmann_json_target)
  find_package(nlohmann_json CONFIG QUIET)
  if(TARGET nlohmann_json::nlohmann_json)
    return()
  endif()

  set(_json_hints)
  if(DEFINED ENV{NLOHMANN_JSON_INCLUDE} AND NOT "$ENV{NLOHMANN_JSON_INCLUDE}" STREQUAL "")
    list(APPEND _json_hints "$ENV{NLOHMANN_JSON_INCLUDE}")
  endif()
  if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    list(APPEND _json_hints "$ENV{VCPKG_ROOT}/installed/include")
    if(DEFINED ENV{VCPKG_TARGET_TRIPLET} AND NOT "$ENV{VCPKG_TARGET_TRIPLET}" STREQUAL "")
      list(APPEND _json_hints "$ENV{VCPKG_ROOT}/installed/$ENV{VCPKG_TARGET_TRIPLET}/include")
    endif()
    list(APPEND _json_hints "$ENV{VCPKG_ROOT}/installed/x64-windows/include")
    list(APPEND _json_hints "$ENV{VCPKG_ROOT}/installed/x64-linux/include")
  endif()

  find_path(
    NLOHMANN_JSON_INCLUDE_DIR
    NAMES nlohmann/json.hpp
    HINTS ${_json_hints}
    PATH_SUFFIXES include
  )

  if(NLOHMANN_JSON_INCLUDE_DIR)
    add_library(nlohmann_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    target_include_directories(nlohmann_json INTERFACE "${NLOHMANN_JSON_INCLUDE_DIR}")
    return()
  endif()

  include(FetchContent)
  message(STATUS "nlohmann/json not found locally, fetching it with FetchContent")
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  )
  FetchContent_MakeAvailable(nlohmann_json)
endfunction()

ensure_nlohmann_json_target()

set(ENCOUNTER_ENGINE_SOURCES
  Source/chuan_pid/chuan_gen_zhong.cpp
  Source/chuan_pid/chuan_pid.cpp
  Source/CombatSimulation/ExtendConstants.cpp
  Source/CombatSimulation/UnitDefine.cpp
  Source/Control/flyPID.cpp
  Source/FlyTac/aircraft.cpp
  Source/FlyTac/missile.cpp
  Source/Sensor/missile_radar.cpp
  Source/Sensor/radar.cpp
  Source/TacView/TacViewDefine.cpp
  Source/TacView/TacViewFile_T.cpp
  Source/TacView/TacViewOutput.cpp
  Source/Tools/BlueCommander.cpp
  Source/Tools/coordinate.cpp
  Source/Tools/RedCommander.cpp
  Source/Tools/extern.cpp
  Source/Tools/interface.cpp
  Source/Tools/sea_skimming.cpp
  Source/Tools/tool_function.cpp
  Source/Tools/WaypointControl.cpp
  Source/Tools/ZHANSHU.cpp
)

if(WIN32 AND SEA_AIR_ENABLE_TACVIEW_SERVER)
  list(APPEND ENCOUNTER_ENGINE_SOURCES Source/TacView/TacViewServer_T.cpp)
endif()

if(WIN32 AND SEA_AIR_ENABLE_JOYSTICK)
  list(APPEND ENCOUNTER_ENGINE_SOURCES
    Source/Tools/JoySticks.cpp
    Source/Tools/joystick_gamepad.cpp
  )
endif()

function(add_encounter_runner_target target_name runner_source tactic_source)
  add_executable(
    ${target_name}
    ${runner_source}
    ${tactic_source}
    ${ENCOUNTER_ENGINE_SOURCES}
  )

  target_compile_definitions(
    ${target_name}
    PRIVATE
      _CRT_SECURE_NO_WARNINGS
      _USE_MATH_DEFINES
      NOMINMAX
  )

  add_bool_compile_definition(${target_name} SEA_AIR_ENABLE_TACVIEW_SERVER SEA_AIR_ENABLE_TACVIEW_SERVER)
  add_bool_compile_definition(${target_name} SEA_AIR_ENABLE_WGUA SEA_AIR_ENABLE_WGUA)
  add_bool_compile_definition(${target_name} SEA_AIR_ENABLE_JOYSTICK SEA_AIR_ENABLE_JOYSTICK)
  add_bool_compile_definition(${target_name} SEA_AIR_ENABLE_ACMI_FILE SEA_AIR_ENABLE_ACMI_FILE)

  if(MSVC)
    target_compile_options(${target_name} PRIVATE /utf-8)
  endif()

  target_include_directories(
    ${target_name}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/Source
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/chuan_pid
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/CombatSimulation
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/Control
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/FlyTac
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/Sensor
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/TacView
      ${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools
  )

  target_link_libraries(
    ${target_name}
    PRIVATE
      nlohmann_json::nlohmann_json
  )

  if(WIN32 AND SEA_AIR_ENABLE_TACVIEW_SERVER)
    target_link_libraries(${target_name} PRIVATE ws2_32)
  endif()

  if(WIN32 AND SEA_AIR_ENABLE_WGUA)
    target_link_libraries(
      ${target_name}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/libs/wguazj.lib
    )
  endif()

  set_target_properties(
    ${target_name}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )

  if(WIN32 AND SEA_AIR_ENABLE_WGUA)
    add_custom_command(
      TARGET ${target_name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_CURRENT_SOURCE_DIR}/Source/libs/wguazj.dll"
              "$<TARGET_FILE_DIR:${target_name}>/wguazj.dll"
    )
  endif()
endfunction()

add_encounter_runner_target(
  EncounterBatchRunner
  agent_mvp/cpp_sandbox_template/EncounterBatchRunner.cpp
  agent_mvp/cpp_sandbox_template/EncounterBatchTactics.cpp
)

add_encounter_runner_target(
  EncounterBatchRunnerTemplate
  agent_mvp/cpp_sandbox_template/EncounterBatchRunner_Template.cpp
  agent_mvp/cpp_sandbox_template/EncounterBatchTactics_Template.cpp
)
