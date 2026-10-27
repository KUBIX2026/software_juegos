cmake_minimum_required(VERSION 3.14)

project(kubix LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)
include_directories(include)
# hal.h incluye <SDL2/SDL.h>. En Windows (MinGW) SDL2_INCLUDE_DIRS apunta a
# include/SDL2, asi que se agrega tambien la carpeta de arriba.
foreach(sdl_dir ${SDL2_INCLUDE_DIRS})
  include_directories("${sdl_dir}/..")
endforeach()

# Raiz del repositorio: el menu busca cada juego en <raiz>/<juego>/build.
get_filename_component(KUBIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

add_executable(kubix src/main.c)
target_link_libraries(kubix PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(kubix PRIVATE KUBIX_ROOT="${KUBIX_ROOT}")

# Compila tambien cada juego en su propia carpeta build, igual que si se
# compilara por separado. Se desactiva con -DKUBIX_BUILD_GAMES=OFF.
option(KUBIX_BUILD_GAMES "Compilar los juegos junto con el menu" ON)
if(KUBIX_BUILD_GAMES)
  string(REPLACE ";" "$<SEMICOLON>" prefix_path "${CMAKE_PREFIX_PATH}")
  foreach(game roadFighter space_invaders snake pong Tetris breakout)
    add_custom_target(${game}_game ALL
      COMMAND ${CMAKE_COMMAND} -S ${KUBIX_ROOT}/${game}
              -B ${KUBIX_ROOT}/${game}/build -G ${CMAKE_GENERATOR}
              "-DCMAKE_PREFIX_PATH=${prefix_path}"
      COMMAND ${CMAKE_COMMAND} --build ${KUBIX_ROOT}/${game}/build
              --config $<CONFIG>
      COMMENT "Compilando ${game}"
      VERBATIM)
  endforeach()
endif()
