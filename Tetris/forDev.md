# Guía de compilación: Tetris

Este proyecto utiliza **CMake** como sistema de compilación y requiere la biblioteca **SDL2** para ejecutar el juego. La lógica de las reglas (`src/tetris_logica.c`) no depende de SDL2 y sus pruebas se compilan con cualquier compilador de C.

## Prerrequisitos

- **Linux (Ubuntu/Debian):**

```bash
sudo apt-get update
sudo apt-get install libsdl2-dev cmake
```

- **macOS (Homebrew):**

```bash
brew install sdl2 cmake
```

- **Windows:** se recomienda **vcpkg**, o descargar los archivos de desarrollo de SDL2 y configurarlos en tu entorno.

### Windows con MinGW

Se configura apuntando a la carpeta de SDL2 para MinGW (`SDL2-devel-...-mingw.zip` de libsdl.org):

```bash
cmake -G Ninja -S . -B build -DCMAKE_PREFIX_PATH=C:/ruta/SDL2-2.32.10/i686-w64-mingw32
```

Para ejecutar, `SDL2.dll` (carpeta `bin` de ese mismo paquete) debe estar junto a `tetris.exe` o en el `PATH`.

## Compilar el juego

Desde la carpeta `Tetris`:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Ejecuta el juego **desde la carpeta `build`**: los sonidos se buscan en `../../assets/Tetris`.

## Probar la lógica (sin SDL2)

Con CMake, después de compilar:

```bash
ctest
```

O directamente con gcc, desde la carpeta `Tetris`:

```bash
gcc -Wall -Wextra -Wconversion -Iinclude -o tetris_tests tests/tetris_tests.c src/tetris_logica.c
./tetris_tests
```

Con `-v` imprime también las pruebas que pasan. El programa termina con código 1 si alguna falla.

## Regenerar los sprites

`include/assets.h` se genera a partir de los PNG de [`assets/Tetris`](../assets/Tetris) con la herramienta del repositorio. Desde la raíz del repositorio (requiere Python y Pillow):

```bash
pip install pillow
python3 TOOLS/converter.py -i assets/Tetris -o Tetris/include/assets.h --macros
```

El píxel transparente (canal alfa o el color `#0F081F`) se escribe como `0x0843`, el color que `sendSprite()` omite. `--macros` agrega las macros `Nombre_W` y `Nombre_H` con el tamaño de cada sprite.

## Visual Studio

Abre la carpeta `Tetris` con **Open a local folder**, selecciona `tetris.exe` en **Startup Item** y ejecútalo con **F5**. Visual Studio configura CMake solo.
