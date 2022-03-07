cmake_minimum_required(VERSION 3.13)
project(webc_compiler)
if (WIN32)
    set(CMAKE_CXX_STANDARD 14)
else ()
    set(CMAKE_CXX_STANDARD 17)
endif ()

# 始终选择最高版本依赖
SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

# LLVM
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
message(STATUS "LLVM : ${LLVM_INCLUDE_DIRS}")

# Clang
find_package(Clang REQUIRED CONFIG)
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${ClANG_DEFINITIONS})
message(STATUS "Clang : ${CLANG_INCLUDE_DIRS}")

# LLD
#find_package(LLD REQUIRED CONFIG)
#message(STATUS "find lld ${LLD_INCLUDE_DIRS}")
#include_directories(${LLD_INCLUDE_DIRS})
#add_definitions(${LLD_DEFINITIONS})

# Boost
find_package(Boost COMPONENTS system filesystem thread chrono regex REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIR})
message(STATUS "Boost : ${Boost_INCLUDE_DIRS}")

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
#include_directories(${GTK3_INCLUDE_DIRS})
#link_directories(${GTK3_LIBRARY_DIRS})
#add_definitions(${GTK3_CFLAGS_OTHER})

# PkgConfig
find_package(PkgConfig REQUIRED)

# Bison
#find_package(BISON)
#BISON_TARGET(Parser parser/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser/Parser.cpp
#        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser/Parser.h)

# C++静态链接
#set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "GTK3_INCLUDE_DIRS: ${GTK3_INCLUDE_DIRS}")
message(STATUS "GKT3_LIBRARIES: ${GTK3_LIBRARIES}")
message(STATUS "GTK3_LINK_LIBRARIES: ${GTK3_LINK_LIBRARIES}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using defs in: ${LLVM_DEFINITIONS}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}")

# import gtkmm, for linux use 3.0, for macos use 4.0 for test
pkg_check_modules(GTKMM gtkmm-3.0 gtksourceviewmm-3.0)
link_directories(${GTKMM_LIBRARY_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})
message(STATUS "GTKMM Lib ${GTKMM_LIBRARY_DIRS}")
message(STATUS "GTKMM Inc ${GTKMM_INCLUDE_DIRS}")

pkg_check_modules(SSLLib openssl)
message(STATUS "found ssl lib in ${SSLLib_INCLUDE_DIRS}")
message(STATUS "found ssl lib in ${SSLLib_LIBRARY_DIRS}")
include_directories(${SSLLib_INCLUDE_DIRS})
link_directories(${SSLLib_LIBRARY_DIRS})

# 导入头文件
include_directories(
        .
        parser
        codegen
        args
)

aux_source_directory(. ROOT)
message(STATUS "根目录含有：${ROOT}")
aux_source_directory(parser PARSER)
message(STATUS "解释器文件含有：${PARSER}")
aux_source_directory(ast AST)
message(STATUS "语法树定义：${AST}")
aux_source_directory(codegen CodeGen)
message(STATUS "代码生成：${CodeGen}")
aux_source_directory(args ARGS)
message(STATUS "Console Parser：${ARGS}")
aux_source_directory(pass PASS)
message(STATUS "Pass：${PASS}")
aux_source_directory(extern EXT)
message(STATUS "外部声明：${EXT}")
aux_source_directory(compiler COMPILER)
message(STATUS "编译器：${COMPILER}")
aux_source_directory(ui UI)
message(STATUS "窗口实现：${UI}")

add_executable(webc_compiler
        ${ROOT}
        ${AST}
        ${PARSER}
        # ${BISON_Parser_OUTPUTS}
        ${CodeGen}
        ${UI}
        ${ARGS}
        ${PASS}
        ${EXT}
        ${COMPILER}
        )
# 平台公用宏定义
add_compile_definitions(webc_compiler PUBLIC ASM_SUFFIX=".s")
# CGUI存在则生成可视化界面 DEBUG_FLAG开启调试模式 CLANG_SUPPORT开启可执行文件输出
add_compile_definitions(webc_compiler PUBLIC CLANG_SUPPORT=1 CGUI=1)

if (WIN32)
    add_compile_definitions(webc_compiler PUBLIC EXECUTABLE_SUFFIX=".exe" OBJ_SUFFIX=".obj")
else ()
    add_compile_definitions(webc_compiler PUBLIC EXECUTABLE_SUFFIX=".out" OBJ_SUFFIX=".o")
endif ()

message(STATUS "host system is ${CMAKE_HOST_SYSTEM_PROCESSOR}")

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
    add_compile_definitions(webc_compiler PUBLIC SYSY_AARCH64=0)
    llvm_map_components_to_libnames(llvm_libs support core irreader
            aarch64info aarch64asmparser aarch64desc aarch64codegen aarch64disassembler #arm64
            )
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm")
    add_compile_definitions(webc_compiler PUBLIC SYSY_ARM=0)
    llvm_map_components_to_libnames(llvm_libs support core irreader
            arminfo armasmparser armdesc armcodegen armdisassembler #arm
            )
else ()
    add_compile_definitions(webc_compiler PUBLIC SYSY_X86=0)
    llvm_map_components_to_libnames(llvm_libs support core irreader
            x86info x86asmparser x86desc x86codegen x86disassembler #X86
            )
endif ()
message(STATUS "Found ${llvm_libs}")

# Link against LLVM libraries
if (WIN32)
    target_link_libraries(webc_compiler
            LLVM
            ${GTKMM_LIBRARIES}
            ${SSLLib_LIBRARIES}
            Boost::thread
            Boost::chrono
            Boost::regex
            clangDriver
            wsock32
            )
else ()
    target_link_libraries(webc_compiler
            LLVM
            ${GTKMM_LIBRARIES}
            ${SSLLib_LIBRARIES}
            Boost::thread
            Boost::chrono
            Boost::regex
            clangDriver
            )
endif ()

install(TARGETS webc_compiler)
message(STATUS "配置目标库")
add_subdirectory(module)