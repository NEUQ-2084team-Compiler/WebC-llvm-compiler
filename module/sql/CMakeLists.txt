cmake_minimum_required(VERSION 3.13)
project(KingtousProject)
set(CMAKE_CXX_STANDARD 14)

message(STATUS ${CMAKE_SOURCE_DIR})
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/obj/unknown)
set(LOCATION "lib")

if (APPLE)
    message(STATUS "macOS detected")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/obj/x86_64/mac)
    include_directories("/usr/local/Cellar/mysql-connector-c++/include")
    link_directories(/usr/local/Cellar/mysql-connector-c++/lib)
    #    set(CMAKE_C_FLAGS  "-fPIC")
    #    set(CMAKE_SHARED_LINKER_FLAGS "-fPIC")
endif ()

if (UNIX)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/obj/x86_64/linux)
    message(STATUS "unix detected")
endif ()

aux_source_directory(src SRC)

message(STATUS "ksql files: ${SRC}")

add_library(ksql STATIC src/ksql.cpp)

if (UNIX)
    # 目前sqlite只在linux上启用
    find_package(SQLite3 REQUIRED)
    if (SQLITE3_FOUND)
        target_include_directories(ksql PUBLIC ${SQLite3_INCLUDE_DIRS})
        target_link_libraries(ksql PUBLIC ${SQLite3_LIBRARIES})
    endif (SQLITE3_FOUND)
endif ()

if (WIN32)
    target_link_libraries(ksqldemo ws2_32 secur32 mysqlclient)
endif ()

add_executable(ksqldemo src/ksqldemo.cpp)
target_link_libraries(ksqldemo kjson ksql mysqlcppconn)

install(TARGETS ksql
        ARCHIVE DESTINATION ${LOCATION}
        LIBRARY DESTINATION ${LOCATION}
        )