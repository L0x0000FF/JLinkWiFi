cmake_minimum_required(VERSION 3.10)
project(WiFi_Scanner)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# 查找wiringPi库
find_library(WIRINGPI_LIB wiringPi)
find_path(WIRINGPI_INCLUDE_DIR wiringPi.h
    PATHS /usr/local/include /usr/include
)

# 使用pkg-config查找NetworkManager和GLib
find_package(PkgConfig REQUIRED)

# 查找NetworkManager
pkg_check_modules(NM QUIET libnm)
if(NM_FOUND)
    message(STATUS "NetworkManager found: ${NM_VERSION}")
else()
    message(WARNING "NetworkManager not found, using fallback scanning method")
    set(NM_FOUND FALSE)
endif()

# 查找GLib（NetworkManager依赖）
pkg_check_modules(GLIB QUIET glib-2.0)
if(GLIB_FOUND)
    message(STATUS "GLib found: ${GLIB_VERSION}")
else()
    message(WARNING "GLib not found, some features may be limited")
    set(GLIB_FOUND FALSE)
endif()

if(NOT WIRINGPI_LIB)
    message(FATAL_ERROR "wiringPi library not found! Please install wiringPi.")
endif()

if(NOT WIRINGPI_INCLUDE_DIR)
    message(FATAL_ERROR "wiringPi headers not found!")
endif()

# 创建可执行文件
add_executable(wifi_scanner 
    wifi_scanner.cpp 
    oled.cpp
)

# 包含头文件目录
target_include_directories(wifi_scanner PRIVATE 
    ${WIRINGPI_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接wiringPi库
target_link_libraries(wifi_scanner ${WIRINGPI_LIB})

# 如果找到NetworkManager和GLib，链接它们
if(NM_FOUND AND GLIB_FOUND)
    target_include_directories(wifi_scanner PRIVATE ${NM_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
    target_link_libraries(wifi_scanner ${NM_LIBRARIES} ${GLIB_LIBRARIES})
    
    # 添加编译定义
    target_compile_definitions(wifi_scanner PRIVATE HAVE_NETWORKMANAGER=1)
else()
    target_compile_definitions(wifi_scanner PRIVATE HAVE_NETWORKMANAGER=0)
endif()

# 设置编译选项
target_compile_options(wifi_scanner PRIVATE -Wall -O2)
