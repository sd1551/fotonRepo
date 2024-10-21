# Минимальная версия CMake
cmake_minimum_required(VERSION 3.12)

# Название проекта
project(FotonProject)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 14)

# Добавление пути к конфигурации Boost
set(CMAKE_PREFIX_PATH "${BOOST_ROOT}/stage")

# Поиск пакета OpenCV
find_package(OpenCV REQUIRED)

# Поиск библиотеки Boost
find_package(Boost 1.86 REQUIRED COMPONENTS program_options)

# Указание путей для SQLite3
find_library(SQLite3_LIBRARY sqlite3 HINTS ${SQLite3_LIBRARY})
find_path(SQLite3_INCLUDE_DIR sqlite3.h HINTS ${SQLite3_INCLUDE_DIR})

# Добавление исполняемого файла проекта
add_executable(FotonProject "CreateImageFromDB.cpp")

# Указание директорий для заголовочных файлов
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${SQLite3_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

# Линковка библиотек
target_link_libraries(FotonProject ${OpenCV_LIBS})
target_link_libraries(FotonProject ${SQLite3_LIBRARY})
target_link_libraries(FotonProject ${Boost_LIBRARIES})