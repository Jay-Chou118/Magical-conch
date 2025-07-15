cmake_minimum_required(VERSION 3.10)
project(magical-conch)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找PortAudio库
find_library(PORTAUDIO_LIBRARY NAMES portaudio)
if(NOT PORTAUDIO_LIBRARY)
    message(FATAL_ERROR "PortAudio library not found. Please install it with: brew install portaudio")
endif()

# 包含头文件目录
include_directories(
    include
    /opt/homebrew/include  # Apple Silicon路径
    # /usr/local/include   # Intel路径
    src/reed-solomon
)

# 收集源文件
file(GLOB SOURCES
    src/*.cpp
    src/reed-solomon/*.hpp
)

# 创建可执行文件 - 必须先于target_link_libraries
add_executable(magical-conch ${SOURCES} src/main.cpp)

# 链接库
if(APPLE)
    target_link_libraries(magical-conch
        ${PORTAUDIO_LIBRARY}
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework Carbon"
    )
else()
    target_link_libraries(magical-conch ${PORTAUDIO_LIBRARY})
endif()