# Минимальная версия CMake
cmake_minimum_required(VERSION 3.12)

# Название проекта
project(FotonProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавление исполняемого файла проекта
add_executable(FotonProject "TileMapService.cpp")

