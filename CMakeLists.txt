cmake_minimum_required(VERSION 3.12)
project(fonda_sched CXX)
enable_language(C)

option(PEDANTIC_FLAGS "Enable pedantic compiler flags" OFF)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if (PEDANTIC_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra")
endif ()
if (WARNINGS_AS_ERRORS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif ()

option(FONDA_BUILD_MEMDAG "Build external tool dagp" ON)


set(FONDA_INSTALL_BIN_DIR "bin"
        CACHE PATH "Installation directory for executables")
set(FONDA_INSTALL_LIB_DIR "lib"
        CACHE PATH "Installation directory for libraries")
set(FONDA_INSTALL_INCLUDE_DIR "include"
        CACHE PATH "Installation directory for header files")
set(FONDA_INSTALL_PKGCONFIG_DIR "lib/pkgconfig"
        CACHE PATH "Installation directory for pkg-config file")
set(FONDA_CXX_STANDARD "17"
        CACHE STRING "CXX Version to compile with. Currently fixed to 17")

#set(FONDA_CXX_FLAGS "-O3 -fpermissive") #-pg -O0 -fsanitize=undefined -O3  -fsanitize=address -O0 -pg -g
#set(FONDA_LINK_FLAGS "") #-fsanitize=undefined

if (DEFINED ENV{CONDA_PREFIX})
    set(FONDA_SEARCH_PREFIX "$ENV{CONDA_PREFIX}")
    set(FONDA_SEARCH_PREFIX_LIB "${FONDA_SEARCH_PREFIX}/lib")
    set(FONDA_SEARCH_PREFIX_INCLUDE "${FONDA_SEARCH_PREFIX}/include")
endif ()

if (FONDA_BUILD_MEMDAG)
    find_package(PkgConfig)

    pkg_check_modules(GraphViz REQUIRED IMPORTED_TARGET libgvc)

    message(STATUS "GraphViz include dirs: ${GraphViz_INCLUDE_DIRS}")

    set(MEMDAG_SOURCE_DIR ${PROJECT_SOURCE_DIR}/extlibs/memdag/src/)
    add_library(memdag ${PROJECT_SOURCE_DIR}/extlibs/memdag/src/memdag.cpp)
    target_sources(memdag PUBLIC
#            ${MEMDAG_SOURCE_DIR}/seq-schedules-for-restrict.cpp
            ${MEMDAG_SOURCE_DIR}/sp-ization.cpp
#            ${MEMDAG_SOURCE_DIR}/edge-selection-heuristics-for-restrict.cpp
            ${MEMDAG_SOURCE_DIR}/graph-algorithms.cpp
            ${MEMDAG_SOURCE_DIR}/tree.cpp
            ${MEMDAG_SOURCE_DIR}/fifo.cpp
            ${MEMDAG_SOURCE_DIR}/heap.cpp
            ${MEMDAG_SOURCE_DIR}/sp-recognition.cpp
            ${MEMDAG_SOURCE_DIR}/graph.cpp
#            ${MEMDAG_SOURCE_DIR}/maxmemory.cpp
            ${MEMDAG_SOURCE_DIR}/sp-traversals.cpp
            ${MEMDAG_SOURCE_DIR}/tree-opt-sched.cpp
            ${MEMDAG_SOURCE_DIR}/tree-postorder-sched.cpp
            ${MEMDAG_SOURCE_DIR}/cluster.cpp
    )

    target_include_directories(memdag PUBLIC extlibs/memdag/src ${IGRAPH_INCLUDE_DIRS} ${GraphViz_INCLUDE_DIRS} extlibs/csv/single_include)
    target_link_libraries(memdag PRIVATE ${IGRAPH_LIBRARIES} PkgConfig::GraphViz)

    set_target_properties(memdag PROPERTIES CXX_STANDARD ${FONDA_CXX_STANDARD})

endif ()

# Add "include/" and "extlibs/"
include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/extlibs"
)

#pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

add_executable(fonda_scheduler "${PROJECT_SOURCE_DIR}/fonda_scheduler/main.cpp"
        )

target_include_directories(fonda_scheduler PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_sources(fonda_scheduler PRIVATE "${PROJECT_SOURCE_DIR}/fonda_scheduler/io/graphWeightsBuilder.cpp"
        "${PROJECT_SOURCE_DIR}/fonda_scheduler/common.cpp"
        "${PROJECT_SOURCE_DIR}/fonda_scheduler/StaticScheduler.cpp"
        "${PROJECT_SOURCE_DIR}/fonda_scheduler/DynamicSchedulerRuntimeSystem.cpp"
        "${PROJECT_SOURCE_DIR}/fonda_scheduler/DynamicSchedulerScheduler.cpp"

)

target_link_libraries(fonda_scheduler PRIVATE memdag)
set_target_properties(fonda_scheduler PROPERTIES
        CXX_STANDARD ${FONDA_CXX_STANDARD}
        COMPILE_FLAGS "${FONDA_CXX_FLAGS}"
        LINK_FLAGS "${FONDA_LINK_FLAGS}")


IF (FONDA_UNIT_TESTS)
    enable_testing()
    IF (EXISTS "${PROJECT_SOURCE_DIR}/extlibs/googletest/CMakeLists.txt")
        option(BUILD_GTEST "Builds the googletest subproject" ON)
        add_subdirectory(extlibs/googletest)
    ENDIF ()
    include_directories(SYSTEM ./lib)
   # add_executable(dagp-tests ${PROJECT_SOURCE_DIR}/extlibs/dagP/tests/dagP-unittests.cpp "${PROJECT_SOURCE_DIR}/fonda_scheduler/test/test_grapht.cpp")
    set_target_properties(dagp-tests PROPERTIES
            CXX_STANDARD ${FONDA_CXX_STANDARD}
            COMPILE_FLAGS "${FONDA_CXX_FLAGS}"
            LINK_FLAGS "${FONDA_LINK_FLAGS}")
    target_include_directories(dagp-tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_sources(dagp-tests PRIVATE
            "${PROJECT_SOURCE_DIR}/fonda_scheduler/common.cpp"
            "${PROJECT_SOURCE_DIR}/fonda_scheduler/StaticScheduler.cpp"
            "${PROJECT_SOURCE_DIR}/fonda_scheduler/DynamicSchedulerRuntimeSystem.cpp"
            "${PROJECT_SOURCE_DIR}/fonda_scheduler/DynamicSchedulerScheduler.cpp"
            "${PROJECT_SOURCE_DIR}/fonda_scheduler/cluster.cpp")
    target_link_libraries(dagp-tests
            PRIVATE
            gtest
            memdag
    )
ENDIF ()

