cmake_minimum_required(VERSION 3.16)
project(sim_toolkit LANGUAGES CXX)

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

option(LOGKIT_FETCH_DEPS "Fetch glog if not found" ON)
option(LOGKIT_BUNDLE_DEPS_STATIC
       "Build glog from source as static lib and link it into shared logkit"
       ON)
option(SIM_TOOLKIT_BUILD_EXAMPLES "Build examples" ON)
option(SIM_TOOLKIT_BUILD_TESTS "Build tests" ON)

if (LOGKIT_BUNDLE_DEPS_STATIC)
  if (NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR
            "LOGKIT_BUNDLE_DEPS_STATIC=ON requires BUILD_SHARED_LIBS=ON for logkit")
  endif()
  include(FetchContent)
  FetchContent_Declare(
    glog
    GIT_REPOSITORY https://github.com/google/glog.git
    GIT_TAG v0.6.0
  )
  set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
  set(_logkit_prev_build_testing ${BUILD_TESTING})
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(_logkit_prev_build_shared_libs ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glog)
  set(BUILD_SHARED_LIBS ${_logkit_prev_build_shared_libs} CACHE BOOL "" FORCE)
  set(BUILD_TESTING ${_logkit_prev_build_testing} CACHE BOOL "" FORCE)
else()
  find_package(glog QUIET)

  if (NOT glog_FOUND)
    if (NOT LOGKIT_FETCH_DEPS)
      message(FATAL_ERROR "glog not found and LOGKIT_FETCH_DEPS=OFF")
    endif()
    include(FetchContent)
    FetchContent_Declare(
      glog
      GIT_REPOSITORY https://github.com/google/glog.git
      GIT_TAG v0.6.0
    )
    set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
    set(_logkit_prev_build_testing ${BUILD_TESTING})
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glog)
    set(BUILD_TESTING ${_logkit_prev_build_testing} CACHE BOOL "" FORCE)
  endif()
endif()

if (NOT TARGET glog::glog AND TARGET glog)
  add_library(glog::glog ALIAS glog)
endif()

add_library(sim_toolkit
    src/log_manager.cpp
    src/log/log_manager_adapter.cpp
    src/ipc/shared_memory_channel.cpp
    src/memory/system_allocator.cpp
    src/task/thread_pool_executor.cpp
    src/task/simple_task_graph.cpp
    src/api/c_api.cpp
)
if (WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sim_toolkit PRIVATE LOGKIT_BUILD_DLL)
endif()
target_include_directories(sim_toolkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(sim_toolkit
    PRIVATE
        glog::glog
)

add_library(sim_toolkit::sim_toolkit ALIAS sim_toolkit)
add_library(liblogkit::liblogkit ALIAS sim_toolkit)

if (SIM_TOOLKIT_BUILD_EXAMPLES)
  add_executable(sim_toolkit_example examples/main.cpp)
  target_link_libraries(sim_toolkit_example PRIVATE sim_toolkit)

  add_executable(ipc_server_example examples/ipc_server.cpp)
  target_link_libraries(ipc_server_example PRIVATE sim_toolkit)

  add_executable(ipc_client_example examples/ipc_client.cpp)
  target_link_libraries(ipc_client_example PRIVATE sim_toolkit)
endif()

include(GNUInstallDirs)
install(TARGETS sim_toolkit
    EXPORT sim_toolkitTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT sim_toolkitTargets
    FILE sim_toolkitTargets.cmake
    NAMESPACE sim_toolkit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sim_toolkit
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/sim_toolkitConfigVersion.cmake
    VERSION 0.2.0
    COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/sim_toolkitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/sim_toolkitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sim_toolkit
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/sim_toolkitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/sim_toolkitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sim_toolkit
)

if (SIM_TOOLKIT_BUILD_TESTS)
  enable_testing()
  add_executable(logkit_tests tests/logkit_tests.cpp)
  target_link_libraries(logkit_tests PRIVATE sim_toolkit)
  add_test(NAME logkit_tests COMMAND logkit_tests)

  add_executable(interface_tests tests/interface_tests.cpp)
  target_link_libraries(interface_tests PRIVATE sim_toolkit)
  add_test(NAME interface_tests COMMAND interface_tests)
endif()

if (SIM_TOOLKIT_BUILD_EXAMPLES)
  configure_file(config/logging.conf ${CMAKE_CURRENT_BINARY_DIR}/config/logging.conf COPYONLY)
endif()

