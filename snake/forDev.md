# Guía de compilación: Snake

Este proyecto utiliza **CMake** como sistema de compilación y requiere la biblioteca **SDL2** para funcionar.

## Prerrequisitos

- **Linux (Ubuntu/Debian) y WSL:**

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libsdl2-dev
```

- **macOS (Homebrew):**

```bash
brew install sdl2 cmake
```

- **Windows:** se recomienda utilizar **vcpkg**, o descargar los archivos de desarrollo de SDL2 para MinGW (`SDL2-devel-...-mingw.zip` de libsdl.org) y configurar CMake apuntando a esa carpeta:

```bash
cmake -G Ninja -S . -B build -DCMAKE_PREFIX_PATH=C:/ruta/SDL2-2.32.10/x86_64-w64-mingw32
```

Para ejecutar, `SDL2.dll` (carpeta `bin` de ese mismo paquete) debe estar junto a `snake.exe` o en el `PATH`.

## Compilación desde la terminal

Desde la carpeta `snake` (la que contiene `CMakeLists.txt`):

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

El ejecutable `snake` queda dentro de la carpeta `build`. Para ejecutarlo:

```bash
./snake
```

En **WSL** con Windows 11, la ventana del juego se abre en el escritorio de Windows a través de WSLg.

## Compilación con Visual Studio

Abre **Visual Studio**, elige **Open a local folder** y selecciona la carpeta `snake`. Visual Studio detecta el `CMakeLists.txt` y configura el proyecto. Luego selecciona `snake.exe` en **Startup Item** y presiona **F5**.

## Regenerar los sprites

Los sprites de `include/assets.h` se generan a partir de los PNG de [`snake/assets`](./assets) con la herramienta del repositorio. Desde la raíz del repositorio (requiere Python con Pillow):

```bash
python3 TOOLS/converter.py -i snake/assets -o snake/include/assets.h --exclude MainMenuV1 SnakePlaying
```

`converter.py` reescribe `assets.h` completo, cabecera incluida, así que se puede ejecutar las veces que haga falta. Los PNG con transparencia (canal alfa o el color `#0F081F`) quedan con el color transparente `0x0843`.
