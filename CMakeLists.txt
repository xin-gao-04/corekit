cmake_minimum_required(VERSION 3.16)
project(corekit LANGUAGES CXX)

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

option(COREKIT_FETCH_DEPS "Fetch glog if not found" ON)
option(COREKIT_BUNDLE_DEPS_STATIC
       "Build glog from source as static lib and link it into shared corekit"
       ON)
option(COREKIT_BUILD_EXAMPLES "Build examples" ON)
option(COREKIT_BUILD_TESTS "Build tests" ON)
option(COREKIT_ENABLE_MIMALLOC_BACKEND "Enable mimalloc backend support" OFF)
option(COREKIT_ENABLE_TBBMALLOC_BACKEND "Enable tbbmalloc backend support" OFF)

if (COREKIT_BUNDLE_DEPS_STATIC)
  if (NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR
            "COREKIT_BUNDLE_DEPS_STATIC=ON requires BUILD_SHARED_LIBS=ON for corekit")
  endif()
  include(FetchContent)
  FetchContent_Declare(
    glog
    GIT_REPOSITORY https://github.com/google/glog.git
    GIT_TAG v0.6.0
  )
  set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
  set(_corekit_prev_build_testing ${BUILD_TESTING})
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(_corekit_prev_build_shared_libs ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glog)
  set(BUILD_SHARED_LIBS ${_corekit_prev_build_shared_libs} CACHE BOOL "" FORCE)
  set(BUILD_TESTING ${_corekit_prev_build_testing} CACHE BOOL "" FORCE)
else()
  find_package(glog QUIET)

  if (NOT glog_FOUND)
    if (NOT COREKIT_FETCH_DEPS)
      message(FATAL_ERROR "glog not found and COREKIT_FETCH_DEPS=OFF")
    endif()
    include(FetchContent)
    FetchContent_Declare(
      glog
      GIT_REPOSITORY https://github.com/google/glog.git
      GIT_TAG v0.6.0
    )
    set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
    set(_corekit_prev_build_testing ${BUILD_TESTING})
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glog)
    set(BUILD_TESTING ${_corekit_prev_build_testing} CACHE BOOL "" FORCE)
  endif()
endif()

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

if (COREKIT_ENABLE_MIMALLOC_BACKEND)
  find_path(COREKIT_MIMALLOC_INCLUDE_DIR mimalloc.h)
  find_library(COREKIT_MIMALLOC_LIBRARY NAMES mimalloc mimalloc-static)
  if (NOT COREKIT_MIMALLOC_INCLUDE_DIR OR NOT COREKIT_MIMALLOC_LIBRARY)
    message(FATAL_ERROR "COREKIT_ENABLE_MIMALLOC_BACKEND=ON but mimalloc not found")
  endif()
  list(APPEND COREKIT_MEMORY_SOURCES src/memory/mimalloc_allocator.cpp)
  list(APPEND COREKIT_EXTRA_INCLUDE_DIRS ${COREKIT_MIMALLOC_INCLUDE_DIR})
  list(APPEND COREKIT_EXTRA_LIBS ${COREKIT_MIMALLOC_LIBRARY})
  list(APPEND COREKIT_EXTRA_DEFS COREKIT_ENABLE_MIMALLOC_BACKEND=1)
endif()

if (COREKIT_ENABLE_TBBMALLOC_BACKEND)
  find_path(COREKIT_TBBMALLOC_INCLUDE_DIR tbb/scalable_allocator.h)
  find_library(COREKIT_TBBMALLOC_LIBRARY NAMES tbbmalloc)
  if (NOT COREKIT_TBBMALLOC_INCLUDE_DIR OR NOT COREKIT_TBBMALLOC_LIBRARY)
    message(FATAL_ERROR "COREKIT_ENABLE_TBBMALLOC_BACKEND=ON but tbbmalloc not found")
  endif()
  list(APPEND COREKIT_MEMORY_SOURCES src/memory/tbb_allocator.cpp)
  list(APPEND COREKIT_EXTRA_INCLUDE_DIRS ${COREKIT_TBBMALLOC_INCLUDE_DIR})
  list(APPEND COREKIT_EXTRA_LIBS ${COREKIT_TBBMALLOC_LIBRARY})
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
endif()

if (COREKIT_BUILD_EXAMPLES)
  configure_file(config/logging.conf ${CMAKE_CURRENT_BINARY_DIR}/config/logging.conf COPYONLY)
  configure_file(config/corekit.json ${CMAKE_CURRENT_BINARY_DIR}/config/corekit.json COPYONLY)
endif()
