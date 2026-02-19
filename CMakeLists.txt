cmake_minimum_required(VERSION 3.16)
project(corekit LANGUAGES C CXX)

set(CMAKE_POLICY_VERSION_MINIMUM "3.5")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()
if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif()

set(COREKIT_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3party" CACHE PATH
    "Local third-party root directory")
option(COREKIT_USE_LOCAL_THIRDPARTY_ONLY
       "Require all dependencies from local 3party directory"
       ON)
option(COREKIT_BUNDLE_DEPS_STATIC
       "Build glog from source as static lib and link it into shared corekit"
       ON)
option(COREKIT_BUILD_EXAMPLES "Build examples" ON)
option(COREKIT_BUILD_TESTS "Build tests" ON)
option(COREKIT_ENABLE_MIMALLOC_BACKEND "Enable mimalloc backend support" OFF)
option(COREKIT_ENABLE_TBBMALLOC_BACKEND "Enable tbbmalloc backend support" OFF)

set(COREKIT_GLOG_DIR "${COREKIT_THIRDPARTY_DIR}/glog")
set(COREKIT_MIMALLOC_DIR "${COREKIT_THIRDPARTY_DIR}/mimalloc")
set(COREKIT_ONETBB_DIR "${COREKIT_THIRDPARTY_DIR}/oneTBB")

if (NOT EXISTS "${COREKIT_GLOG_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Missing local dependency: ${COREKIT_GLOG_DIR}")
endif()

if (COREKIT_BUNDLE_DEPS_STATIC)
  if (NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR
            "COREKIT_BUNDLE_DEPS_STATIC=ON requires BUILD_SHARED_LIBS=ON for corekit")
  endif()
endif()

# glog: always build from local third-party source.
set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(_corekit_prev_build_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${COREKIT_GLOG_DIR}" "${CMAKE_BINARY_DIR}/3party/glog" EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS ${_corekit_prev_build_shared_libs} CACHE BOOL "" FORCE)

if (NOT TARGET glog::glog AND TARGET glog)
  add_library(glog::glog ALIAS glog)
endif()

set(COREKIT_MEMORY_SOURCES
    src/memory/global_allocator.cpp
    src/memory/system_allocator.cpp
)
set(COREKIT_EXTRA_INCLUDE_DIRS "")
set(COREKIT_EXTRA_LIBS "")
set(COREKIT_EXTRA_DEFS "")

# mimalloc (optional, local source only)
if (COREKIT_ENABLE_MIMALLOC_BACKEND)
  if (NOT EXISTS "${COREKIT_MIMALLOC_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "COREKIT_ENABLE_MIMALLOC_BACKEND=ON but missing ${COREKIT_MIMALLOC_DIR}")
  endif()

  set(_corekit_mi_target "")
  set(MI_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_BENCH OFF CACHE BOOL "" FORCE)
  set(MI_USE_CXX OFF CACHE BOOL "" FORCE)
  add_subdirectory("${COREKIT_MIMALLOC_DIR}" "${CMAKE_BINARY_DIR}/3party/mimalloc" EXCLUDE_FROM_ALL)

  if (TARGET mimalloc-static)
    set(_corekit_mi_target mimalloc-static)
  elseif(TARGET mimalloc)
    set(_corekit_mi_target mimalloc)
  endif()

  if (NOT _corekit_mi_target)
    message(FATAL_ERROR "COREKIT_ENABLE_MIMALLOC_BACKEND=ON but mimalloc target not available")
  endif()

  list(APPEND COREKIT_MEMORY_SOURCES src/memory/mimalloc_allocator.cpp)
  list(APPEND COREKIT_EXTRA_LIBS ${_corekit_mi_target})
  list(APPEND COREKIT_EXTRA_DEFS COREKIT_ENABLE_MIMALLOC_BACKEND=1)
endif()

# tbbmalloc (optional, local source only)
if (COREKIT_ENABLE_TBBMALLOC_BACKEND)
  if (NOT EXISTS "${COREKIT_ONETBB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "COREKIT_ENABLE_TBBMALLOC_BACKEND=ON but missing ${COREKIT_ONETBB_DIR}")
  endif()

  set(_corekit_tbbmalloc_target "")
  set(TBB_TEST OFF CACHE BOOL "" FORCE)
  set(TBB_STRICT OFF CACHE BOOL "" FORCE)
  set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(TBBMALLOC_BUILD ON CACHE BOOL "" FORCE)
  add_subdirectory("${COREKIT_ONETBB_DIR}" "${CMAKE_BINARY_DIR}/3party/oneTBB" EXCLUDE_FROM_ALL)

  if (TARGET TBB::tbbmalloc)
    set(_corekit_tbbmalloc_target TBB::tbbmalloc)
  elseif(TARGET tbbmalloc)
    set(_corekit_tbbmalloc_target tbbmalloc)
  elseif(TARGET tbbmalloc_static)
    set(_corekit_tbbmalloc_target tbbmalloc_static)
  endif()

  if (NOT _corekit_tbbmalloc_target)
    message(FATAL_ERROR "COREKIT_ENABLE_TBBMALLOC_BACKEND=ON but tbbmalloc target not available")
  endif()

  list(APPEND COREKIT_MEMORY_SOURCES src/memory/tbb_allocator.cpp)
  list(APPEND COREKIT_EXTRA_LIBS ${_corekit_tbbmalloc_target})
  if (TARGET TBB::tbb)
    list(APPEND COREKIT_EXTRA_LIBS TBB::tbb)
  elseif(TARGET tbb)
    list(APPEND COREKIT_EXTRA_LIBS tbb)
  endif()
  list(APPEND COREKIT_EXTRA_DEFS COREKIT_ENABLE_TBBMALLOC_BACKEND=1)
endif()

add_library(corekit
    src/api/status.cpp
    src/log_manager.cpp
    src/log/log_manager_adapter.cpp
    src/ipc/shared_memory_channel.cpp
    src/json/json_codec.cpp
    ${COREKIT_MEMORY_SOURCES}
    src/task/thread_pool_executor.cpp
    src/task/simple_task_graph.cpp
    src/api/c_api.cpp
)
if (WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(corekit PRIVATE COREKIT_BUILD_DLL)
endif()
target_include_directories(corekit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COREKIT_EXTRA_INCLUDE_DIRS}
)
if (COREKIT_EXTRA_DEFS)
  target_compile_definitions(corekit PRIVATE ${COREKIT_EXTRA_DEFS})
endif()
target_link_libraries(corekit
    PRIVATE
        glog::glog
        ${COREKIT_EXTRA_LIBS}
)

add_library(corekit::corekit ALIAS corekit)

if (COREKIT_BUILD_EXAMPLES)
  add_executable(corekit_example examples/main.cpp)
  target_link_libraries(corekit_example PRIVATE corekit)

  add_executable(containers_demo examples/containers_demo.cpp)
  target_link_libraries(containers_demo PRIVATE corekit)

  add_executable(ipc_server_example examples/ipc_server.cpp)
  target_link_libraries(ipc_server_example PRIVATE corekit)

  add_executable(ipc_client_example examples/ipc_client.cpp)
  target_link_libraries(ipc_client_example PRIVATE corekit)
endif()

include(GNUInstallDirs)
install(TARGETS corekit
    EXPORT corekitTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT corekitTargets
    FILE corekitTargets.cmake
    NAMESPACE corekit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/corekit
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/corekitConfigVersion.cmake
    VERSION 0.2.0
    COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/corekitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/corekitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/corekit
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/corekitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/corekitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/corekit
)

if (COREKIT_BUILD_TESTS)
  enable_testing()
  add_executable(corekit_legacy_tests tests/corekit_legacy_tests.cpp)
  target_link_libraries(corekit_legacy_tests PRIVATE corekit)
  add_test(NAME corekit_legacy_tests COMMAND corekit_legacy_tests)

  add_executable(interface_tests tests/interface_tests.cpp)
  target_link_libraries(interface_tests PRIVATE corekit)
  add_test(NAME interface_tests COMMAND interface_tests)

  add_executable(ipc_tests tests/ipc_tests.cpp)
  target_link_libraries(ipc_tests PRIVATE corekit)
  add_test(NAME ipc_tests COMMAND ipc_tests)

  add_executable(memory_perf_compare tests/memory_perf_compare.cpp)
  target_link_libraries(memory_perf_compare PRIVATE corekit)
endif()

if (COREKIT_BUILD_EXAMPLES)
  configure_file(config/logging.conf ${CMAKE_CURRENT_BINARY_DIR}/config/logging.conf COPYONLY)
  configure_file(config/corekit.json ${CMAKE_CURRENT_BINARY_DIR}/config/corekit.json COPYONLY)
endif()


