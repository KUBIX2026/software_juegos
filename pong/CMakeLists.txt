cmake_minimum_required(VERSION 3.14)

project(pong LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)
include_directories(include)
# hal.h incluye <SDL2/SDL.h>. En Windows (MinGW) SDL2_INCLUDE_DIRS apunta a
# include/SDL2, asi que se agrega tambien la carpeta de arriba.
foreach(sdl_dir ${SDL2_INCLUDE_DIRS})
  include_directories("${sdl_dir}/..")
endforeach()

add_executable(pong src/main.c src/pong_logica.c)
target_link_libraries(pong PRIVATE ${SDL2_LIBRARIES})

# Pruebas de la logica (no necesitan SDL): ctest, o ./pong_tests
add_executable(pong_tests tests/pong_tests.c src/pong_logica.c)
enable_testing()
add_test(NAME pong_tests COMMAND pong_tests)
