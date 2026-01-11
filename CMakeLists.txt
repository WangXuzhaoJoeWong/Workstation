cmake_minimum_required(VERSION 3.16)

# Workstation service nodes (application layer)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Workstation LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

include(GNUInstallDirs)

find_package(Threads REQUIRED)

option(WXZ_WORKSTATION_USE_LOCAL_MOTIONCORE "(Discouraged) Use sibling MotionCore source tree (../MotionCore) instead of find_package(MotionCore)" OFF)


# Workstation is an independent project.
# Preferred dependency flow:
#   1) Build+install MotionCore to a prefix
#   2) cmake -S Workstation -B build -DCMAKE_PREFIX_PATH=<motioncore_prefix>
set(_wxz_motioncore_target "")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build: require MotionCore package (no implicit source-tree coupling).
    find_package(MotionCore CONFIG REQUIRED)
    if(TARGET MotionCore::MotionCore)
        set(_wxz_motioncore_target MotionCore::MotionCore)
    elseif(TARGET MotionCore)
        set(_wxz_motioncore_target MotionCore)
    endif()
elseif(TARGET MotionCore::MotionCore)
    # Subproject build (discouraged): accept existing target.
    set(_wxz_motioncore_target MotionCore::MotionCore)
elseif(TARGET MotionCore)
    set(_wxz_motioncore_target MotionCore)
elseif(WXZ_WORKSTATION_USE_LOCAL_MOTIONCORE AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../MotionCore/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../MotionCore" "${CMAKE_BINARY_DIR}/_motioncore")
    set(_wxz_motioncore_target MotionCore)
else()
    find_package(MotionCore CONFIG REQUIRED)
    if(TARGET MotionCore::MotionCore)
        set(_wxz_motioncore_target MotionCore::MotionCore)
    elseif(TARGET MotionCore)
        set(_wxz_motioncore_target MotionCore)
    endif()
endif()

if("${_wxz_motioncore_target}" STREQUAL "")
    message(FATAL_ERROR "MotionCore target not found (expected MotionCore or MotionCore::MotionCore)")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Third-party dependency root
#
# In the wxz_robot workspace layout, the heavy binary dependencies live in the workspace root
# (../depends) and are intentionally NOT part of the Workstation git repository.
#
# You can override this path when building Workstation as a standalone repo:
#   cmake -S Workstation -B build -DWXZ_WORKSTATION_DEPS_ROOT=/abs/path/to/depends
set(_wxz_deps_root_default "")
foreach(_cand IN ITEMS
    "${CMAKE_CURRENT_LIST_DIR}/../depends"
    "${CMAKE_CURRENT_LIST_DIR}/depends"
)
    if(EXISTS "${_cand}")
        set(_wxz_deps_root_default "${_cand}")
        break()
    endif()
endforeach()

set(WXZ_WORKSTATION_DEPS_ROOT "${_wxz_deps_root_default}" CACHE PATH "Workstation third-party dependency root (contains CGXi_Robot_SDK, Groot, BehaviorTree.CPP, etc.)")
if(NOT WXZ_WORKSTATION_DEPS_ROOT OR NOT EXISTS "${WXZ_WORKSTATION_DEPS_ROOT}")
    message(FATAL_ERROR "WXZ_WORKSTATION_DEPS_ROOT not found. Current value: '${WXZ_WORKSTATION_DEPS_ROOT}'. Expected a directory that contains 'CGXi_Robot_SDK'.")
endif()

option(WXZ_WORKSTATION_ENABLE_BT "Build Workstation BehaviorTree services" OFF)
set(_wxz_bt_root_default "")
if(EXISTS "${WXZ_WORKSTATION_DEPS_ROOT}/BehaviorTree.CPP/CMakeLists.txt")
    set(_wxz_bt_root_default "${WXZ_WORKSTATION_DEPS_ROOT}/BehaviorTree.CPP")
endif()
set(WXZ_WORKSTATION_BT_ROOT "${_wxz_bt_root_default}" CACHE PATH "Optional path to a local BehaviorTree.CPP v3 checkout")
option(WXZ_WORKSTATION_BT_FETCHCONTENT "Fetch BehaviorTree.CPP v3 via FetchContent when not found locally" OFF)

set(WXZ_WORKSTATION_SDK_ROOT "${WXZ_WORKSTATION_DEPS_ROOT}/CGXi_Robot_SDK")
set(WXZ_WORKSTATION_SDK_INCLUDE "${WXZ_WORKSTATION_SDK_ROOT}/include")
set(WXZ_WORKSTATION_SDK_LIBDIR "${WXZ_WORKSTATION_SDK_ROOT}/libs")

set(_wxz_unpack_dir "${CMAKE_CURRENT_BINARY_DIR}/sdk_unpacked")
set(_wxz_unpack_script "${CMAKE_CURRENT_LIST_DIR}/cmake/unpack_padded_elf.py")

function(_wxz_is_elf_at_start path out_var)
    if(NOT EXISTS "${path}")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    file(READ "${path}" _wxz_magic_hex OFFSET 0 LIMIT 4 HEX)
    if(_wxz_magic_hex STREQUAL "7f454c46")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(wxz_workstation_pick_or_unpack in_lib out_var)
    get_filename_component(_name "${in_lib}" NAME)
    _wxz_is_elf_at_start("${in_lib}" _is_elf)
    if(_is_elf)
        set(${out_var} "${in_lib}" PARENT_SCOPE)
        return()
    endif()

    set(_out "${_wxz_unpack_dir}/${_name}")
    add_custom_command(
        OUTPUT "${_out}"
        COMMAND "${Python3_EXECUTABLE}" "${_wxz_unpack_script}" "${in_lib}" "${_out}"
        DEPENDS "${in_lib}" "${_wxz_unpack_script}"
        COMMENT "Unpacking padded ELF: ${_name}"
        VERBATIM
    )
    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

add_executable(workstation_arm_control_service
    services/arm_control/src/main.cpp
    services/arm_control/src/app.cpp
    services/arm_control/src/domain/arm_control_domain.cpp
    services/arm_control/src/adapters/arm_control_dds_adapter.cpp
    services/arm_control/src/arm_control_internal.cpp
    services/arm_control/src/arm_command_handler.cpp
    services/arm_control/src/arm_cmd_demo.cpp
    services/arm_control/src/arm_cmd_robot_mode.cpp
)

target_include_directories(workstation_arm_control_service PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/services/arm_control/include
    ${WXZ_WORKSTATION_SDK_INCLUDE}
)

target_link_libraries(workstation_arm_control_service PRIVATE
    ${_wxz_motioncore_target}
    Threads::Threads
)

# Production constraint: in Release builds, compile out the simulation code path for arm_control.
if(CMAKE_CONFIGURATION_TYPES)
    target_compile_definitions(workstation_arm_control_service PRIVATE $<$<CONFIG:Release>:WXZ_ARM_DISABLE_SIM=1>)
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(workstation_arm_control_service PRIVATE WXZ_ARM_DISABLE_SIM=1)
    endif()
endif()

option(WXZ_ARM_LINK_SDK "Link CGXi SDK libs directly (required; dlopen disabled)" ON)
if(WXZ_ARM_LINK_SDK)
    target_compile_definitions(workstation_arm_control_service PRIVATE WXZ_ARM_LINK_SDK=1)

    set(_wxz_arm_sdk_inputs
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libcr_sdk.so
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libprotobuf-rpc.so
        # Prefer a known-good ELF variant if present; otherwise unpack the padded one.
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libprotobuf-c.so.1.0.0
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libCGXIZip.so
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libRobotConfigDll.so
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libLogStorageDll.so
    )

    set(_wxz_arm_sdk_runtime_libs)
    set(_wxz_arm_sdk_unpacked_outputs)
    foreach(_in IN LISTS _wxz_arm_sdk_inputs)
        if(NOT EXISTS "${_in}")
            message(FATAL_ERROR "Workstation SDK library not found: ${_in}")
        endif()
        wxz_workstation_pick_or_unpack("${_in}" _picked)
        if(NOT _picked OR NOT EXISTS "${_picked}")
            message(FATAL_ERROR "Workstation SDK library prepare failed (pick/unpack returned an invalid path). Input: ${_in}")
        endif()
        list(APPEND _wxz_arm_sdk_runtime_libs "${_picked}")
        if(_picked MATCHES "^${_wxz_unpack_dir}/")
            list(APPEND _wxz_arm_sdk_unpacked_outputs "${_picked}")
        endif()
    endforeach()

    if(_wxz_arm_sdk_unpacked_outputs)
        add_custom_target(workstation_sdk_unpacked DEPENDS ${_wxz_arm_sdk_unpacked_outputs})
        add_dependencies(workstation_arm_control_service workstation_sdk_unpacked)
    endif()

    target_link_libraries(workstation_arm_control_service PRIVATE ${_wxz_arm_sdk_runtime_libs})

    set_target_properties(workstation_arm_control_service PROPERTIES
        BUILD_RPATH "${WXZ_WORKSTATION_SDK_LIBDIR};${_wxz_unpack_dir}"
    )
endif()

function(wxz_workstation_add_simple_service target service_dir)
    add_executable(${target}
        services/${service_dir}/src/main.cpp
        services/${service_dir}/src/app.cpp
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/services/${service_dir}/include
        ${CMAKE_CURRENT_LIST_DIR}/services/${service_dir}/src
    )

    target_link_libraries(${target} PRIVATE
        ${_wxz_motioncore_target}
        Threads::Threads
    )
endfunction()

if(WXZ_WORKSTATION_ENABLE_BT)
    add_executable(workstation_bt_service
        services/bt_service/src/main.cpp
        services/bt_service/src/app.cpp
        services/bt_service/src/app_config.cpp
        services/bt_service/src/dds_channels.cpp
        services/bt_service/src/arm_status_cache.cpp
        services/bt_service/src/arm_wiring.cpp
        services/bt_service/src/bt_runtime_wiring.cpp
        services/bt_service/src/bt_tree_runner.cpp
        services/bt_service/src/main_loop.cpp
        services/bt_service/src/node_wiring.cpp
        services/bt_service/src/arm_types.cpp
        services/bt_service/src/arm_nodes.cpp
    )

    target_include_directories(workstation_bt_service PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/services/bt_service/include
    )

    target_link_libraries(workstation_bt_service PRIVATE
        ${_wxz_motioncore_target}
        Threads::Threads
    )

    # BehaviorTree.CPP v3 dependency (Workstation-only)
    set(_wxz_bt_targets)

    if(WXZ_WORKSTATION_BT_ROOT)
        if(EXISTS "${WXZ_WORKSTATION_BT_ROOT}/CMakeLists.txt")
            set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
            set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
            set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            add_subdirectory("${WXZ_WORKSTATION_BT_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/behaviortree_cpp")
        else()
            message(FATAL_ERROR "WXZ_WORKSTATION_BT_ROOT does not look like a BehaviorTree.CPP checkout: ${WXZ_WORKSTATION_BT_ROOT}")
        endif()
    elseif(WXZ_WORKSTATION_BT_FETCHCONTENT)
        include(FetchContent)

        set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            behaviortree_cpp_v3
            GIT_REPOSITORY https://github.com/BehaviorTree/BehaviorTree.CPP.git
            GIT_TAG 3.8.6
        )
        FetchContent_MakeAvailable(behaviortree_cpp_v3)
    else()
        find_package(behaviortree_cpp_v3 QUIET)
        find_package(BehaviorTree.CPP QUIET)
    endif()

    if(TARGET behaviortree_cpp_v3)
        set(_wxz_bt_targets behaviortree_cpp_v3)
    elseif(TARGET BT::behaviortree_cpp_v3)
        set(_wxz_bt_targets BT::behaviortree_cpp_v3)
    elseif(TARGET behaviortree_cpp)
        set(_wxz_bt_targets behaviortree_cpp)
    elseif(TARGET BT::behaviortree_cpp)
        set(_wxz_bt_targets BT::behaviortree_cpp)
    endif()

    if(NOT _wxz_bt_targets)
        message(FATAL_ERROR "BehaviorTree.CPP v3 not found. Set -DWXZ_WORKSTATION_BT_ROOT=/path/to/BehaviorTree.CPP or configure with -DWXZ_WORKSTATION_BT_FETCHCONTENT=ON")
    endif()

    target_link_libraries(workstation_bt_service PRIVATE ${_wxz_bt_targets})
endif()

# Direct-link to SDK is mandatory; no runtime dlopen fallback is supported.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_wxz_workstation_install_default ON)
else()
    set(_wxz_workstation_install_default OFF)
endif()
option(WXZ_INSTALL_WORKSTATION "Install workstation service executables (and optional bundled SDK libs)" ${_wxz_workstation_install_default})

if(WXZ_INSTALL_WORKSTATION)
    install(TARGETS
        workstation_arm_control_service
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT runtime-workstation
    )

    install(FILES
        resources/bt.xml.sample
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/workstation
        COMPONENT runtime-workstation
    )

    if(WXZ_WORKSTATION_ENABLE_BT)
        install(TARGETS workstation_bt_service
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT runtime-workstation
        )
    endif()

    if(WXZ_ARM_LINK_SDK)
        set(_wxz_arm_sdk_runtime_libs_to_install ${_wxz_arm_sdk_runtime_libs})
        if(_wxz_arm_sdk_runtime_libs_to_install)
            foreach(_wxz_sdk_lib IN LISTS _wxz_arm_sdk_runtime_libs_to_install)
                string(STRIP "${_wxz_sdk_lib}" _wxz_sdk_lib_stripped)
                if("${_wxz_sdk_lib_stripped}" STREQUAL "")
                    continue()
                endif()
                install(FILES "${_wxz_sdk_lib_stripped}"
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    COMPONENT runtime-workstation
                )
            endforeach()
        else()
            message(WARNING "WXZ_ARM_LINK_SDK=ON but no SDK runtime libs were collected; skip installing SDK runtime libs")
        endif()
    endif()
endif()
