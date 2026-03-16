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
       OFF)
option(COREKIT_AUTO_FETCH_DEPS
       "Automatically download missing third-party sources when local/system deps are unavailable"
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

set(COREKIT_GLOG_URL "https://github.com/google/glog/archive/refs/tags/v0.7.1.zip")
set(COREKIT_MIMALLOC_URL "https://github.com/microsoft/mimalloc/archive/refs/tags/v2.1.7.zip")
set(COREKIT_ONETBB_URL "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.12.0.zip")

include(FetchContent)

function(_corekit_resolve_source_dep out_var dep_name local_dir dep_url)
  string(TOLOWER "${dep_name}" _fetch_suffix)
  if(EXISTS "${local_dir}/CMakeLists.txt")
    set(${out_var} "${local_dir}" PARENT_SCOPE)
    message(STATUS "[corekit] ${dep_name}: using local source ${local_dir}")
    return()
  endif()

  if(COREKIT_USE_LOCAL_THIRDPARTY_ONLY)
    message(FATAL_ERROR "[corekit] Missing local dependency: ${local_dir}")
  endif()

  if(NOT COREKIT_AUTO_FETCH_DEPS)
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()

  set(_fetch_name "corekit_${_fetch_suffix}")
  message(STATUS "[corekit] ${dep_name}: local source missing, fetching ${dep_url}")
  FetchContent_Declare(${_fetch_name}
    URL "${dep_url}"
  )
  FetchContent_GetProperties(${_fetch_name})
  if(NOT ${_fetch_name}_POPULATED)
    FetchContent_Populate(${_fetch_name})
  endif()
  set(${out_var} "${${_fetch_name}_SOURCE_DIR}" PARENT_SCOPE)
  message(STATUS "[corekit] ${dep_name}: fetched to ${${_fetch_name}_SOURCE_DIR}")
endfunction()

# ── zip 自动解压辅助函数 ───────────────────────────────────────────────────────
# 用法：_corekit_unzip_dep(<name> <zip_path> <dest_parent_dir>)
# 将 zip 解压到 <dest_parent_dir>/<name>。
# GitHub zip 通常只含一个顶级目录，自动 rename 为 <name>。
function(_corekit_unzip_dep name zip_path dest_parent)
  set(_tmp "${dest_parent}/${name}__tmp_extract")
  file(MAKE_DIRECTORY "${_tmp}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf "${zip_path}"
    WORKING_DIRECTORY "${_tmp}"
    RESULT_VARIABLE _rc
  )
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "[corekit] Failed to extract ${name} from ${zip_path}")
  endif()
  # 检测顶级条目
  file(GLOB _top_entries LIST_DIRECTORIES true "${_tmp}/*")
  list(LENGTH _top_entries _n)
  set(_final "${dest_parent}/${name}")
  if(_n EQUAL 1)
    # GitHub 风格：单一顶级目录，重命名
    file(RENAME "${_top_entries}" "${_final}")
    message(STATUS "[corekit] ${name}: extracted (single root dir) -> ${_final}")
  else()
    file(RENAME "${_tmp}" "${_final}")
    message(STATUS "[corekit] ${name}: extracted -> ${_final}")
  endif()
  if(EXISTS "${_tmp}")
    file(REMOVE_RECURSE "${_tmp}")
  endif()
endfunction()

# ── tinyxml2 依赖（zip vendor）────────────────────────────────────────────────
set(_COREKIT_ZIP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(_COREKIT_TINYXML2_DIR "${_COREKIT_ZIP_DIR}/tinyxml2")
set(_COREKIT_TINYXML2_ZIP "${_COREKIT_ZIP_DIR}/tinyxml2.zip")

if(NOT EXISTS "${_COREKIT_TINYXML2_DIR}/tinyxml2.h")
  if(EXISTS "${_COREKIT_TINYXML2_ZIP}")
    _corekit_unzip_dep(tinyxml2 "${_COREKIT_TINYXML2_ZIP}" "${_COREKIT_ZIP_DIR}")
  else()
    message(FATAL_ERROR
      "[corekit] tinyxml2 not found.\n"
      "  提供方式 1（推荐）：third_party/tinyxml2.zip 已入库，无需额外操作\n"
      "  提供方式 2（联网更新）：bash scripts/vendor-deps.sh\n"
      "  当前搜索路径：${_COREKIT_TINYXML2_DIR}/tinyxml2.h")
  endif()
endif()
message(STATUS "[corekit] tinyxml2 ready: ${_COREKIT_TINYXML2_DIR}")

if (COREKIT_BUNDLE_DEPS_STATIC)
  if (NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR
            "COREKIT_BUNDLE_DEPS_STATIC=ON requires BUILD_SHARED_LIBS=ON for corekit")
  endif()
endif()

set(_corekit_have_local_glog OFF)
if (EXISTS "${COREKIT_GLOG_DIR}/CMakeLists.txt")
  set(_corekit_have_local_glog ON)
endif()
set(_corekit_glog_source_dir "")

if (_corekit_have_local_glog)
  # Prefer vendored glog when it is present.
  set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
  set(_corekit_prev_build_shared_libs ${BUILD_SHARED_LIBS})
  set(_corekit_prev_build_testing ${BUILD_TESTING})
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${COREKIT_GLOG_DIR}" "${CMAKE_BINARY_DIR}/3party/glog" EXCLUDE_FROM_ALL)
  set(BUILD_SHARED_LIBS ${_corekit_prev_build_shared_libs} CACHE BOOL "" FORCE)
  set(BUILD_TESTING ${_corekit_prev_build_testing} CACHE BOOL "" FORCE)

  if (NOT TARGET glog::glog AND TARGET glog)
    add_library(glog::glog ALIAS glog)
  endif()
elseif (COREKIT_USE_LOCAL_THIRDPARTY_ONLY)
  message(FATAL_ERROR "Missing local dependency: ${COREKIT_GLOG_DIR}")
else()
  find_package(glog CONFIG QUIET)
  if (NOT TARGET glog::glog)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
      pkg_check_modules(GLOG QUIET IMPORTED_TARGET libglog)
      if (NOT TARGET PkgConfig::GLOG)
        pkg_check_modules(GLOG QUIET IMPORTED_TARGET libglog glog)
      endif()
      if (TARGET PkgConfig::GLOG)
        add_library(glog::glog INTERFACE IMPORTED)
        target_link_libraries(glog::glog INTERFACE PkgConfig::GLOG)
      endif()
    endif()
  endif()
  if (NOT TARGET glog::glog)
    _corekit_resolve_source_dep(_corekit_glog_source_dir glog "${COREKIT_GLOG_DIR}" "${COREKIT_GLOG_URL}")
    if (_corekit_glog_source_dir)
      set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
      set(_corekit_prev_build_shared_libs ${BUILD_SHARED_LIBS})
      set(_corekit_prev_build_testing ${BUILD_TESTING})
      set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
      add_subdirectory("${_corekit_glog_source_dir}" "${CMAKE_BINARY_DIR}/3party/glog" EXCLUDE_FROM_ALL)
      set(BUILD_SHARED_LIBS ${_corekit_prev_build_shared_libs} CACHE BOOL "" FORCE)
      set(BUILD_TESTING ${_corekit_prev_build_testing} CACHE BOOL "" FORCE)
      if (NOT TARGET glog::glog AND TARGET glog)
        add_library(glog::glog ALIAS glog)
      endif()
    endif()
  endif()
  if (NOT TARGET glog::glog)
    message(FATAL_ERROR
            "glog was not found. Provide 3party/glog, install a system glog package, "
            "or leave COREKIT_AUTO_FETCH_DEPS=ON to auto-download missing sources.")
  endif()
endif()

find_package(Threads REQUIRED)

set(COREKIT_MEMORY_SOURCES
    src/memory/global_allocator.cpp
    src/memory/system_allocator.cpp
)
set(COREKIT_EXTRA_INCLUDE_DIRS "")
set(COREKIT_EXTRA_LIBS "")
set(COREKIT_EXTRA_DEFS "")

# mimalloc (optional, local source only)
if (COREKIT_ENABLE_MIMALLOC_BACKEND)
  _corekit_resolve_source_dep(_corekit_mimalloc_source_dir mimalloc "${COREKIT_MIMALLOC_DIR}" "${COREKIT_MIMALLOC_URL}")
  if (NOT _corekit_mimalloc_source_dir)
    message(FATAL_ERROR
      "COREKIT_ENABLE_MIMALLOC_BACKEND=ON but mimalloc was not found. "
      "Provide ${COREKIT_MIMALLOC_DIR}, or leave COREKIT_AUTO_FETCH_DEPS=ON.")
  endif()

  set(_corekit_mi_target "")
  set(MI_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_BENCH OFF CACHE BOOL "" FORCE)
  set(MI_USE_CXX OFF CACHE BOOL "" FORCE)
  add_subdirectory("${_corekit_mimalloc_source_dir}" "${CMAKE_BINARY_DIR}/3party/mimalloc" EXCLUDE_FROM_ALL)

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
  _corekit_resolve_source_dep(_corekit_onetbb_source_dir oneTBB "${COREKIT_ONETBB_DIR}" "${COREKIT_ONETBB_URL}")
  if (NOT _corekit_onetbb_source_dir)
    message(FATAL_ERROR
      "COREKIT_ENABLE_TBBMALLOC_BACKEND=ON but oneTBB was not found. "
      "Provide ${COREKIT_ONETBB_DIR}, or leave COREKIT_AUTO_FETCH_DEPS=ON.")
  endif()

  set(_corekit_tbbmalloc_target "")
  set(TBB_TEST OFF CACHE BOOL "" FORCE)
  set(TBB_STRICT OFF CACHE BOOL "" FORCE)
  set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(TBBMALLOC_BUILD ON CACHE BOOL "" FORCE)
  add_subdirectory("${_corekit_onetbb_source_dir}" "${CMAKE_BINARY_DIR}/3party/oneTBB" EXCLUDE_FROM_ALL)

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
    src/ipc/shared_memory_channel.cpp
    src/json/json_codec.cpp
    ${COREKIT_MEMORY_SOURCES}
    src/task/thread_pool_executor.cpp
    src/task/simple_task_graph.cpp
    src/api/c_api.cpp
    # XML module（tinyxml2 后端）
    src/xml/tinyxml2_doc_impl.cpp
    "${_COREKIT_TINYXML2_DIR}/tinyxml2.cpp"
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
        # tinyxml2 headers（PRIVATE：不暴露给消费者）
        ${_COREKIT_TINYXML2_DIR}
)
if (COREKIT_EXTRA_DEFS)
  target_compile_definitions(corekit PUBLIC ${COREKIT_EXTRA_DEFS})
endif()
target_link_libraries(corekit
    PRIVATE
        glog::glog
        Threads::Threads
        ${COREKIT_EXTRA_LIBS}
)

add_library(corekit::corekit ALIAS corekit)

if (COREKIT_BUILD_EXAMPLES)
  add_executable(logging_demo examples/logging_demo.cpp)
  target_link_libraries(logging_demo PRIVATE corekit)

  add_executable(memory_demo examples/memory_demo.cpp)
  target_link_libraries(memory_demo PRIVATE corekit)

  add_executable(task_demo examples/task_demo.cpp)
  target_link_libraries(task_demo PRIVATE corekit)

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

  add_executable(interface_tests tests/interface_tests.cpp)
  target_link_libraries(interface_tests PRIVATE corekit)
  add_test(NAME interface_tests COMMAND interface_tests)

  add_executable(ipc_tests tests/ipc_tests.cpp)
  target_link_libraries(ipc_tests PRIVATE corekit)
  add_test(NAME ipc_tests COMMAND ipc_tests)

  add_executable(memory_perf_compare tests/memory_perf_compare.cpp)
  target_link_libraries(memory_perf_compare PRIVATE corekit)

  add_executable(xml_tests tests/xml_tests.cpp)
  target_link_libraries(xml_tests PRIVATE corekit)
endif()

if (COREKIT_BUILD_EXAMPLES)
  configure_file(config/logging.conf ${CMAKE_CURRENT_BINARY_DIR}/config/logging.conf COPYONLY)
  configure_file(config/corekit.json ${CMAKE_CURRENT_BINARY_DIR}/config/corekit.json COPYONLY)
endif()
