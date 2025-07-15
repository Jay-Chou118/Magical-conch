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

# 查找依赖库
find_library(PORTAUDIO_LIBRARY NAMES portaudio)
find_path(SDL2_INCLUDE_DIR SDL.h HINTS /opt/homebrew/include/SDL2)
find_library(SDL2_LIBRARY NAMES SDL2 HINTS /opt/homebrew/lib)
find_library(MATH_LIBRARY m)

# 检查依赖
if(NOT PORTAUDIO_LIBRARY)
    message(FATAL_ERROR "PortAudio library not found. Install with: brew install portaudio")
endif()
if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
    message(FATAL_ERROR "SDL2 not found. Install with: brew install sdl2")
endif()

# 包含头文件目录
include_directories(
    include
    /opt/homebrew/include
    ${SDL2_INCLUDE_DIR}
    src/reed-solomon
)

# 收集源文件
set(SOURCES
    src/ggwave-common-sdl2.cpp
    src/ggwave-common.cpp
    src/ggwave.cpp
    src/main.cpp
    # 添加Reed-Solomon源文件（如果有实现文件）
    src/reed-solomon/rs.hpp
)

# 创建可执行文件
add_executable(magical-conch ${SOURCES})

# 链接库
target_link_libraries(magical-conch
    ${PORTAUDIO_LIBRARY}
    ${SDL2_LIBRARY}
    ${MATH_LIBRARY}
)

# Apple特定框架
if(APPLE)
    target_link_libraries(magical-conch
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework Carbon"
    )
endif()

# 安装规则
# install(TARGETS magical-conch DESTINATION bin)