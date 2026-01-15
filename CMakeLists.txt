cmake_minimum_required(VERSION 3.10)
project(magical-conch)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# 设置默认构建类型为Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 跨平台依赖查找
if(ANDROID)
    # Android特定配置
    find_library(SDL2_LIBRARY SDL2)
    find_library(LOG_LIBRARY log)
elseif(UNIX AND NOT APPLE)
    # Linux配置
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    include_directories(${SDL2_INCLUDE_DIRS})
elseif(APPLE)
    # macOS配置
    find_path(SDL2_INCLUDE_DIR SDL.h HINTS /opt/homebrew/include/SDL2 /usr/local/include/SDL2)
    find_library(SDL2_LIBRARY NAMES SDL2 SDL2-2.0 HINTS /opt/homebrew/lib /usr/local/lib)
    if(SDL2_INCLUDE_DIR)
        include_directories(${SDL2_INCLUDE_DIR})
    endif()
elseif(WIN32)
    # Windows配置
    find_path(SDL2_INCLUDE_DIR SDL.h HINTS "C:/Program Files/SDL2/include/SDL2")
    find_library(SDL2_LIBRARY SDL2.lib HINTS "C:/Program Files/SDL2/lib/x64")
    include_directories(${SDL2_INCLUDE_DIR})
endif()

find_library(MATH_LIBRARY m)

# 检查依赖
if(NOT SDL2_LIBRARY)
    message(FATAL_ERROR "SDL2 library not found. Please install SDL2 development files.")
endif()

# 包含头文件目录
include_directories(
    include
    ${SDL2_INCLUDE_DIR}
    src/reed-solomon
)

# 收集源文件
set(SOURCES
    src/ggwave-common-sdl2.cpp
    src/ggwave-common.cpp
    src/ggwave.cpp
    src/main.cpp
    src/reed-solomon/rs.hpp
)

# 创建可执行文件
add_executable(magical-conch ${SOURCES})

# 跨平台链接
if(ANDROID)
    target_link_libraries(magical-conch
        ${SDL2_LIBRARY}
        ${LOG_LIBRARY}
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(magical-conch
        ${SDL2_LIBRARIES}
        ${MATH_LIBRARY}
    )
elseif(APPLE)
    target_link_libraries(magical-conch
        ${SDL2_LIBRARY}
        ${MATH_LIBRARY}
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework Carbon"
    )
elseif(WIN32)
    target_link_libraries(magical-conch
        ${SDL2_LIBRARY}
        winmm
    )
endif()

# 安装规则
# install(TARGETS magical-conch DESTINATION bin)