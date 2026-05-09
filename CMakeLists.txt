cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(fox-http-example CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── fox-http (installed library) ─────────────────────────────────────
# Consume the system-installed fox-http via its exported targets file.
# (No fox-httpConfig.cmake is shipped, so include the Targets file directly.)
if(NOT TARGET fox-http::fox-http)
    find_package(Boost REQUIRED)
    find_package(Threads REQUIRED)
    find_file(FOX_HTTP_TARGETS_FILE NAMES fox-httpTargets.cmake
        PATHS /usr/local/lib/cmake/fox-http
              /usr/lib/cmake/fox-http
              /usr/lib64/cmake/fox-http)
    if(NOT FOX_HTTP_TARGETS_FILE)
        message(FATAL_ERROR "fox-http install not found (looked for fox-httpTargets.cmake under /usr/local/lib/cmake/fox-http).")
    endif()
    include(${FOX_HTTP_TARGETS_FILE})
endif()

# ─── Code-gen toolchain (fox-http-based) ──────────────────
# Installed under /usr/local/bin (or any directory on PATH).
find_program(CRDL_COMPILER NAMES fox-route)
find_program(CRDL_FUNC NAMES fox-route-func)
find_program(FOX_PAGE_EXECUTABLE NAMES fox-page)
if(NOT CRDL_COMPILER)
    message(FATAL_ERROR "fox-route not found — install fox-route to a directory on PATH.")
endif()
if(NOT CRDL_FUNC)
    message(FATAL_ERROR "fox-route-func not found — install fox-route to a directory on PATH.")
endif()
if(NOT FOX_PAGE_EXECUTABLE)
    message(FATAL_ERROR "fox-page not found — install fox-page to a directory on PATH.")
endif()
message(STATUS "fox-route: ${CRDL_COMPILER}")
message(STATUS "fox-page:       ${FOX_PAGE_EXECUTABLE}")

# ─── Third-party deps ─────────────────────────────────────────────────

# fox-mysql
set(FOX_MYSQL_ROOT "" CACHE PATH "fox-mysql install root (optional)")
find_path(FOX_MYSQL_INCLUDE_DIR NAMES fox-mysql/fox-mysql.h
    HINTS ${FOX_MYSQL_ROOT} PATHS /usr/local/include /usr/include
    PATH_SUFFIXES include include/fox-mysql)
find_library(FOX_MYSQL_LIBRARY NAMES fox-mysql fox-mysql_static
    HINTS ${FOX_MYSQL_ROOT} PATHS /usr/local/lib /usr/lib /usr/lib64
    PATH_SUFFIXES lib lib64)
if(NOT FOX_MYSQL_INCLUDE_DIR OR NOT FOX_MYSQL_LIBRARY)
    message(FATAL_ERROR "fox-mysql not found.")
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
    COMMENT "fox-route: routes.crdl → router.generated.*"
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
            COMMAND sh -c "${FOX_PAGE_EXECUTABLE} ${SRC_HTML} -o ${CMAKE_CURRENT_BINARY_DIR}/pages --func \"$(${CRDL_FUNC} ${CRDL_FILE} ${CPP_BASENAME})\""
            DEPENDS ${SRC_HTML} ${CRDL_FILE}
            COMMENT "fox-page: ${CPP_BASENAME}.html → ${CPP_BASENAME}.cpp"
            VERBATIM
        )
    endif()
endforeach()

# ─── Executable ───────────────────────────────────────────────────────
file(GLOB DB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/db/*.cpp")
add_executable(fox-http-example
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/handlers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/router.generated.cpp
    ${PAGE_CPP_LIST}
    ${DB_SRC}
)
add_dependencies(fox-http-example generate_router_files)

target_include_directories(fox-http-example PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FOX_MYSQL_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIR}
)

target_link_libraries(fox-http-example PRIVATE
    fox-http::fox-http
    ${JSONCPP_LIBRARY}
    ${FOX_MYSQL_LIBRARY}
    ${MYSQLCLIENT_LIBRARY}
)
