cmake_minimum_required(VERSION 3.14)

project(space_invaders LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)
include_directories(include)
# hal.h incluye <SDL2/SDL.h>. En Windows (MinGW) SDL2_INCLUDE_DIRS apunta a
# include/SDL2, asi que se agrega tambien la carpeta de arriba.
foreach(sdl_dir ${SDL2_INCLUDE_DIRS})
  include_directories("${sdl_dir}/..")
endforeach()

add_executable(space_invaders src/main.c)
target_link_libraries(space_invaders PRIVATE ${SDL2_LIBRARIES})
