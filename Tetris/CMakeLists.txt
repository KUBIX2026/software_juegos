cmake_minimum_required(VERSION 3.14)

project(tetris LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pruebas de la logica (no necesitan SDL): ctest, o ./tetris_tests
add_executable(tetris_tests tests/tetris_tests.c src/tetris_logica.c)
target_include_directories(tetris_tests PRIVATE include)
enable_testing()
add_test(NAME tetris_tests COMMAND tetris_tests)

# El juego necesita SDL2; se compila solo si existe src/main.c
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)
  find_package(SDL2 REQUIRED)
  include_directories(include)
  # hal.h incluye <SDL2/SDL.h>. En Windows (MinGW) SDL2_INCLUDE_DIRS apunta a
  # include/SDL2, asi que se agrega tambien la carpeta de arriba.
  foreach(sdl_dir ${SDL2_INCLUDE_DIRS})
    include_directories("${sdl_dir}/..")
  endforeach()
  add_executable(tetris src/main.c src/tetris_logica.c)
  target_link_libraries(tetris PRIVATE ${SDL2_LIBRARIES})
endif()
