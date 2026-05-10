cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(fox-http-example CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── fox-http (installed library) ─────────────────────────────────────
find_package(fox-http REQUIRED)

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

# fox-mysql (modern CMake package; pulls in mysqlclient via pkg-config)
find_package(fox-mysql REQUIRED)

# jsoncpp
find_package(jsoncpp REQUIRED)

# ─── Code generation: routes.crdl → handlers.h + router.generated.* ───
# Note: fox-route also writes a handlers.cpp scaffold on first run, but
# skips it on subsequent runs if the file already exists. It is treated
# as one-time scaffolding, not a build artifact — hence not listed as an
# OUTPUT here (would otherwise force fox-route to re-run on every build
# after a routes.crdl edit, since its mtime never gets refreshed).
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/router.generated.cpp
           ${CMAKE_CURRENT_BINARY_DIR}/router.generated.h
           ${CMAKE_CURRENT_BINARY_DIR}/handlers.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/routes.crdl
    COMMAND ${CRDL_COMPILER} -o ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/routes.crdl
    COMMENT "fox-route: routes.crdl → router.generated.*"
)

# ─── Page generation: pages/*.html → ${build}/pages/*.cpp ─────────────
set(PAGE_CPP_LIST
    ${CMAKE_CURRENT_BINARY_DIR}/pages/index.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/pages/user.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/pages/users.cpp
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
# CONFIGURE_DEPENDS so adding new db/*.cpp triggers a reconfigure.
file(GLOB DB_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/db/*.cpp")
add_executable(fox-http-example
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/handlers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/router.generated.cpp
    ${PAGE_CPP_LIST}
    ${DB_SRC}
)

target_include_directories(fox-http-example PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fox-http-example PRIVATE
    fox-http::fox-http
    fox-mysql::fox-mysql
    JsonCpp::JsonCpp
)
