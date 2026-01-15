cmake_minimum_required(VERSION 3.16)

# Workstation service nodes (application layer)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Workstation LANGUAGES C CXX)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

include(GNUInstallDirs)

# Local developer workflow: install into the build directory by default.
# This avoids creating an extra "_install" folder and keeps MotionCore/Workstation independent.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Install prefix (default: build directory)" FORCE)
endif()

set(WXZ_WORKSTATION_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Pin Fast-DDS/Fast-CDR to the workspace prefix by default.
# This avoids accidentally picking up system installs (e.g. /usr/local) and makes the build reproducible.
get_filename_component(WXZ_REPO_ROOT "${WXZ_WORKSTATION_DIR}" DIRECTORY)

set(WXZ_FASTDDS_VERSION "2.14.4")
set(WXZ_FASTCDR_VERSION "2.2.0")
set(_wxz_pinned_fastdds_prefix "${WXZ_REPO_ROOT}/depends/eprosima_fastdds/v${WXZ_FASTDDS_VERSION}/_install")

if(NOT DEFINED WXZ_FASTDDS_PREFIX OR "${WXZ_FASTDDS_PREFIX}" STREQUAL "")
    if(EXISTS "${_wxz_pinned_fastdds_prefix}")
        set(WXZ_FASTDDS_PREFIX "${_wxz_pinned_fastdds_prefix}" CACHE PATH "Pinned Fast-DDS prefix (v${WXZ_FASTDDS_VERSION})" FORCE)
    else()
        set(WXZ_FASTDDS_PREFIX "" CACHE PATH "Pinned Fast-DDS prefix (v${WXZ_FASTDDS_VERSION})" FORCE)
    endif()
endif()

if(WXZ_FASTDDS_PREFIX)
    if(NOT EXISTS "${WXZ_FASTDDS_PREFIX}")
        message(FATAL_ERROR
            "WXZ_FASTDDS_PREFIX does not exist: ${WXZ_FASTDDS_PREFIX}\n"
            "Expected: ${_wxz_pinned_fastdds_prefix}\n"
            "Please ensure Fast-DDS v${WXZ_FASTDDS_VERSION} (and Fast-CDR v${WXZ_FASTCDR_VERSION}) is installed into that directory.")
    endif()

    # Help downstream find_dependency() in MotionCoreConfig.cmake by setting *_DIR explicitly.
    set(_wxz_fastcdr_hint_paths
        "${WXZ_FASTDDS_PREFIX}/lib/cmake/fastcdr"
        "${WXZ_FASTDDS_PREFIX}/lib64/cmake/fastcdr"
        "${WXZ_FASTDDS_PREFIX}/lib/cmake"
        "${WXZ_FASTDDS_PREFIX}/lib64/cmake"
        "${WXZ_FASTDDS_PREFIX}"
    )
    set(_wxz_fastrtps_hint_paths
        "${WXZ_FASTDDS_PREFIX}/share/fastrtps/cmake"
        "${WXZ_FASTDDS_PREFIX}/lib/cmake/fastrtps"
        "${WXZ_FASTDDS_PREFIX}/lib64/cmake/fastrtps"
        "${WXZ_FASTDDS_PREFIX}/lib/cmake"
        "${WXZ_FASTDDS_PREFIX}/lib64/cmake"
        "${WXZ_FASTDDS_PREFIX}"
    )

    if(NOT DEFINED fastcdr_DIR OR "${fastcdr_DIR}" STREQUAL "")
        foreach(_cand IN LISTS _wxz_fastcdr_hint_paths)
            if(EXISTS "${_cand}/fastcdrConfig.cmake" OR EXISTS "${_cand}/fastcdr-config.cmake")
                set(fastcdr_DIR "${_cand}")
                break()
            endif()
        endforeach()
    endif()
    if(NOT DEFINED fastrtps_DIR OR "${fastrtps_DIR}" STREQUAL "")
        foreach(_cand IN LISTS _wxz_fastrtps_hint_paths)
            if(EXISTS "${_cand}/fastrtpsConfig.cmake" OR EXISTS "${_cand}/fastrtps-config.cmake")
                set(fastrtps_DIR "${_cand}")
                break()
            endif()
        endforeach()
    endif()

    if(
        NOT DEFINED fastcdr_DIR OR "${fastcdr_DIR}" STREQUAL "" OR
        (NOT EXISTS "${fastcdr_DIR}/fastcdrConfig.cmake" AND NOT EXISTS "${fastcdr_DIR}/fastcdr-config.cmake")
    )
        message(FATAL_ERROR "Pinned Fast-CDR v${WXZ_FASTCDR_VERSION} not found under WXZ_FASTDDS_PREFIX=${WXZ_FASTDDS_PREFIX}. Searched: ${_wxz_fastcdr_hint_paths}")
    endif()
    if(
        NOT DEFINED fastrtps_DIR OR "${fastrtps_DIR}" STREQUAL "" OR
        (NOT EXISTS "${fastrtps_DIR}/fastrtpsConfig.cmake" AND NOT EXISTS "${fastrtps_DIR}/fastrtps-config.cmake")
    )
        message(FATAL_ERROR "Pinned Fast-RTPS v${WXZ_FASTDDS_VERSION} not found under WXZ_FASTDDS_PREFIX=${WXZ_FASTDDS_PREFIX}. Searched: ${_wxz_fastrtps_hint_paths}")
    endif()

    if(DEFINED fastcdr_DIR AND NOT "${fastcdr_DIR}" MATCHES "^${WXZ_FASTDDS_PREFIX}(/|$)")
        message(FATAL_ERROR "fastcdr_DIR must be under WXZ_FASTDDS_PREFIX. fastcdr_DIR=${fastcdr_DIR} WXZ_FASTDDS_PREFIX=${WXZ_FASTDDS_PREFIX}")
    endif()
    if(DEFINED fastrtps_DIR AND NOT "${fastrtps_DIR}" MATCHES "^${WXZ_FASTDDS_PREFIX}(/|$)")
        message(FATAL_ERROR "fastrtps_DIR must be under WXZ_FASTDDS_PREFIX. fastrtps_DIR=${fastrtps_DIR} WXZ_FASTDDS_PREFIX=${WXZ_FASTDDS_PREFIX}")
    endif()

    # Make pinned prefix available to other find_package() calls (if any).
    set(_wxz_prefix_path "${CMAKE_PREFIX_PATH}")
    list(PREPEND _wxz_prefix_path "${WXZ_FASTDDS_PREFIX}")
    list(REMOVE_DUPLICATES _wxz_prefix_path)
    set(CMAKE_PREFIX_PATH "${_wxz_prefix_path}")
endif()

# Generate a convenience installer script in the build directory.
configure_file(
    ${WXZ_WORKSTATION_DIR}/cmake/install.sh.in
    ${CMAKE_BINARY_DIR}/install.sh
    @ONLY
)

if(UNIX)
    execute_process(COMMAND chmod +x "${CMAKE_BINARY_DIR}/install.sh")
endif()

find_package(Threads REQUIRED)

option(WXZ_WORKSTATION_USE_LOCAL_MOTIONCORE "(Discouraged) Use sibling MotionCore source tree (../MotionCore) instead of find_package(MotionCore)" OFF)


# Workstation is an independent project.
# Preferred dependency flow:
#   1) Build+install MotionCore into its build directory (default) OR a custom prefix
#   2) cmake -S Workstation -B build -DCMAKE_PREFIX_PATH=<motioncore_build_or_prefix>
set(_wxz_motioncore_target "")

# In the wxz_robot workspace layout, help CMake find MotionCore without extra flags
# by probing a few common build/install locations and appending them to CMAKE_PREFIX_PATH.
set(_wxz_motioncore_prefix_candidates
    "${WXZ_REPO_ROOT}/MotionCore/build_fastdds_2_14_4"
    "${WXZ_REPO_ROOT}/MotionCore/build"
    "${WXZ_REPO_ROOT}/MotionCore/build_fastdds_${WXZ_FASTDDS_VERSION}"
    "${WXZ_REPO_ROOT}/MotionCore/build_fastdds_${WXZ_FASTDDS_VERSION}/_install"
)
if(NOT DEFINED MotionCore_DIR OR "${MotionCore_DIR}" STREQUAL "")
    set(_wxz_prefix_path "${CMAKE_PREFIX_PATH}")
    foreach(_cand IN LISTS _wxz_motioncore_prefix_candidates)
        if(EXISTS "${_cand}")
            # Append (not prepend) so user-provided CMAKE_PREFIX_PATH keeps precedence.
            list(APPEND _wxz_prefix_path "${_cand}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _wxz_prefix_path)
    set(CMAKE_PREFIX_PATH "${_wxz_prefix_path}")
endif()

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

option(WXZ_WORKSTATION_ENABLE_BT "Build Workstation BehaviorTree services" ON)

set(_wxz_bt_root_default "")
if(EXISTS "${WXZ_WORKSTATION_DEPS_ROOT}/BehaviorTree.CPP/CMakeLists.txt")
    set(_wxz_bt_root_default "${WXZ_WORKSTATION_DEPS_ROOT}/BehaviorTree.CPP")
endif()
set(WXZ_WORKSTATION_BT_ROOT "${_wxz_bt_root_default}" CACHE PATH "Optional path to a local BehaviorTree.CPP v3 checkout")

# For standalone Workstation repos (no ../depends/BehaviorTree.CPP), default to FetchContent so
# bt_service still builds out-of-the-box.
set(_wxz_bt_fetchcontent_default OFF)
if(NOT WXZ_WORKSTATION_BT_ROOT)
    set(_wxz_bt_fetchcontent_default ON)
endif()
option(WXZ_WORKSTATION_BT_FETCHCONTENT "Fetch BehaviorTree.CPP v3 via FetchContent when not found locally" ${_wxz_bt_fetchcontent_default})

set(WXZ_WORKSTATION_SDK_ROOT "${WXZ_WORKSTATION_DEPS_ROOT}/CGXi_Robot_SDK")
set(WXZ_WORKSTATION_SDK_INCLUDE "${WXZ_WORKSTATION_SDK_ROOT}/include")
set(WXZ_WORKSTATION_SDK_LIBDIR "${WXZ_WORKSTATION_SDK_ROOT}/libs")

add_executable(workstation_arm_control_service
    services/arm_control/src/main.cpp
    services/arm_control/src/app.cpp
    services/arm_control/src/arm_control_config.cpp
    services/arm_control/src/arm_control_loop.cpp
    services/arm_control/src/rpc_control_plane.cpp
    services/arm_control/src/rpc_kv_codec.cpp
    services/arm_control/src/arm_rpc_handlers.cpp
    services/arm_control/src/arm_rpc_service_config.cpp
    services/arm_control/src/arm_command_processor.cpp
    services/arm_control/src/arm_control_internal.cpp
    services/arm_control/src/arm_command_handler.cpp
)


target_include_directories(workstation_arm_control_service PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
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
        # SDK libs are expected to be standard ELF shared objects.
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libprotobuf-c.so.1.0.0
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libCGXIZip.so
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libRobotConfigDll.so
        ${WXZ_WORKSTATION_SDK_LIBDIR}/libLogStorageDll.so
    )

    foreach(_in IN LISTS _wxz_arm_sdk_inputs)
        if(NOT EXISTS "${_in}")
            message(FATAL_ERROR "Workstation SDK library not found: ${_in}")
        endif()
    endforeach()

    target_link_libraries(workstation_arm_control_service PRIVATE ${_wxz_arm_sdk_inputs})

    # Used by install() to optionally bundle the SDK runtime libs.
    set(_wxz_arm_sdk_runtime_libs ${_wxz_arm_sdk_inputs})

    set_target_properties(workstation_arm_control_service PROPERTIES
        BUILD_RPATH "${WXZ_WORKSTATION_SDK_LIBDIR}"
    )

    # Important: the SDK core lib (libcr_sdk.so.1) depends on other SDK libs
    # (libprotobuf-rpc.so.1, libprotobuf-c.so.1, ...). With DT_RUNPATH, the
    # loader will NOT use the executable's runpath for these transitive deps.
    # Using DT_RPATH avoids the "...: not found" runtime error without requiring
    # users to export LD_LIBRARY_PATH.
    target_link_options(workstation_arm_control_service PRIVATE "-Wl,--disable-new-dtags")

    # When installed, prefer bundled libs under <prefix>/lib.
    set_target_properties(workstation_arm_control_service PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
endif()

function(wxz_workstation_add_simple_service target service_dir)
    add_executable(${target}
        services/${service_dir}/src/main.cpp
        services/${service_dir}/src/app.cpp
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
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
        services/bt_service/src/rpc_control_plane.cpp
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

    # bt_service always loads ./bt.xml (relative to current working directory).
    # For developer convenience, create a build-tree symlink so running from
    # Workstation/build works without setting any env vars.
    set(_wxz_bt_xml_source "${CMAKE_CURRENT_LIST_DIR}/resources/bt.xml")
    if(NOT EXISTS "${_wxz_bt_xml_source}")
        set(_wxz_bt_xml_source "${CMAKE_CURRENT_LIST_DIR}/resources/bt.xml.sample")
    endif()
    add_custom_command(TARGET workstation_bt_service POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:workstation_bt_service>/bt.xml"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${_wxz_bt_xml_source}" "$<TARGET_FILE_DIR:workstation_bt_service>/bt.xml"
        VERBATIM
    )

    target_include_directories(workstation_bt_service PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
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

    install(FILES
        docs/RPC调用示例（arm_control与bt_service）.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/workstation/docs
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
