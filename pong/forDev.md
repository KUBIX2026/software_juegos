# Guía de compilación: Pong

Este proyecto utiliza **CMake** como sistema de compilación y requiere la biblioteca **SDL2** para ejecutar el juego. La lógica de las reglas (`src/pong_logica.c`) no depende de SDL2 y sus pruebas se compilan con cualquier compilador de C.

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

### Windows con MinGW (comprobado)

Se probó con SDL2 2.32.10 (`SDL2-devel-...-mingw.zip` de libsdl.org), CMake 4.4 y Ninja. Se configura apuntando a la carpeta de SDL2 para MinGW:

```bash
cmake -G Ninja -S . -B build -DCMAKE_PREFIX_PATH=C:/ruta/SDL2-2.32.10/i686-w64-mingw32
```

Para ejecutar, `SDL2.dll` (carpeta `bin` de ese mismo paquete) debe estar junto a `pong.exe` o en el `PATH`.

## Compilar el juego

Desde la carpeta `pong`:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Ejecuta el juego **desde la carpeta `build`**: los sonidos se buscan en `../assets`.

## Probar la lógica (sin SDL2)

Con CMake, después de compilar:

```bash
ctest
```

O directamente con gcc, desde la carpeta `pong`:

```bash
gcc -Wall -Wextra -Wconversion -Iinclude -o pong_tests tests/pong_tests.c src/pong_logica.c
./pong_tests
```

Con `-v` imprime también las pruebas que pasan. El programa termina con código 1 si alguna falla.

## Regenerar los sprites

`include/assets.h` se genera a partir de `assets/*.png` (requiere Python y Pillow):

```bash
pip install pillow
python tools/png2assets.py
```

El píxel transparente se escribe como `0x0843`, el color que `sendSprite()` omite.

## Visual Studio

Abre la carpeta `pong` con **Open a local folder**, selecciona `pong.exe` en **Startup Item** y ejecútalo con **F5**. Visual Studio configura CMake solo.
