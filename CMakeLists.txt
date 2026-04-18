cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(SimpleHttpTemplateHs CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── httpserver (the rewritten HTTP library replacing Photon) ─────────
# Sibling project at ../httpserver. Build the library as a subdirectory
# target so we don't require a system install.
if(NOT TARGET httpserver::httpserver)
    set(HTTPSERVER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../httpserver
                     ${CMAKE_BINARY_DIR}/httpserver_build)
endif()

# ─── Code-gen toolchain (migrated to httpserver API) ──────────────────
find_program(CRDL_COMPILER NAMES crdl_compiler
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../route_hs/build)
find_program(CRDL_FUNC NAMES crdl_func
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../route_hs/build)
find_program(WEAVEPP_EXECUTABLE NAMES weave++
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../weave++_hs/build)
if(NOT CRDL_COMPILER)
    message(FATAL_ERROR "crdl_compiler not found — build /home/yelin/code/route_hs first.")
endif()
if(NOT CRDL_FUNC)
    message(FATAL_ERROR "crdl_func not found — build /home/yelin/code/route_hs first.")
endif()
if(NOT WEAVEPP_EXECUTABLE)
    message(FATAL_ERROR "weave++ not found — build /home/yelin/code/weave++_hs first.")
endif()
message(STATUS "crdl_compiler: ${CRDL_COMPILER}")
message(STATUS "weave++:       ${WEAVEPP_EXECUTABLE}")

# ─── Third-party deps (unchanged) ─────────────────────────────────────
find_package(gflags REQUIRED)

# yxmysql
set(YXMYSQL_ROOT "" CACHE PATH "yxmysql install root (optional)")
find_path(YXMYSQL_INCLUDE_DIR NAMES yxmysql/yxmysql.h
    HINTS ${YXMYSQL_ROOT} PATHS /usr/local/include /usr/include
    PATH_SUFFIXES include include/yxmysql)
find_library(YXMYSQL_LIBRARY NAMES yxmysql yxmysql_static
    HINTS ${YXMYSQL_ROOT} PATHS /usr/local/lib /usr/lib /usr/lib64
    PATH_SUFFIXES lib lib64)
if(NOT YXMYSQL_INCLUDE_DIR OR NOT YXMYSQL_LIBRARY)
    message(FATAL_ERROR "yxmysql not found.")
endif()

# jsoncpp
find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h
    PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp /usr/include /usr/local/include
    PATH_SUFFIXES include jsoncpp)
find_library(JSONCPP_LIBRARY NAMES jsoncpp
    PATHS /usr/local/lib /usr/lib /usr/lib64 PATH_SUFFIXES lib lib64)
if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
    message(FATAL_ERROR "jsoncpp not found.")
endif()

find_library(MYSQLCLIENT_LIBRARY NAMES mysqlclient
    PATHS /usr/local/lib /usr/lib /usr/lib64 PATH_SUFFIXES lib lib64)
if(NOT MYSQLCLIENT_LIBRARY)
    message(FATAL_ERROR "mysqlclient not found.")
endif()

# ─── Code generation: routes.crdl → handlers.h / router.generated.* ───
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/router.generated.cpp
           ${CMAKE_CURRENT_BINARY_DIR}/router.generated.h
           ${CMAKE_CURRENT_BINARY_DIR}/handlers.h
           ${CMAKE_CURRENT_BINARY_DIR}/handlers.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/routes.crdl
    COMMAND ${CRDL_COMPILER} -o ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/routes.crdl
    COMMENT "crdl_compiler: routes.crdl → router.generated.*"
)
add_custom_target(generate_router_files DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/router.generated.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/router.generated.h
    ${CMAKE_CURRENT_BINARY_DIR}/handlers.h
    ${CMAKE_CURRENT_BINARY_DIR}/handlers.cpp
)

# ─── Page generation: pages/*.html → ${build}/pages/*.cpp ─────────────
set(PAGE_CPP_LIST
    ${CMAKE_CURRENT_BINARY_DIR}/pages/index.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/pages/user.cpp
)

set(CRDL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/routes.crdl")
foreach(GEN_CPP ${PAGE_CPP_LIST})
    get_filename_component(CPP_BASENAME ${GEN_CPP} NAME_WE)
    set(SRC_HTML "${CMAKE_CURRENT_SOURCE_DIR}/pages/${CPP_BASENAME}.html")
    if(EXISTS ${SRC_HTML})
        add_custom_command(
            OUTPUT ${GEN_CPP}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pages
            COMMAND sh -c "${WEAVEPP_EXECUTABLE} ${SRC_HTML} -o ${CMAKE_CURRENT_BINARY_DIR}/pages --func \"$(${CRDL_FUNC} ${CRDL_FILE} ${CPP_BASENAME})\""
            DEPENDS ${SRC_HTML} ${CRDL_FILE}
            COMMENT "weave++: ${CPP_BASENAME}.html → ${CPP_BASENAME}.cpp"
            VERBATIM
        )
    endif()
endforeach()

# ─── Executable ───────────────────────────────────────────────────────
file(GLOB DB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/db/*.cpp")
add_executable(simple_http_template_hs
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/handlers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/router.generated.cpp
    ${PAGE_CPP_LIST}
    ${DB_SRC}
)
add_dependencies(simple_http_template_hs generate_router_files)

target_include_directories(simple_http_template_hs PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${YXMYSQL_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIR}
)

target_link_libraries(simple_http_template_hs PRIVATE
    httpserver::httpserver
    gflags
    ${JSONCPP_LIBRARY}
    ${YXMYSQL_LIBRARY}
    ${MYSQLCLIENT_LIBRARY}
)
