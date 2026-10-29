# Guía de compilación: menú principal KUBIX

Este proyecto utiliza **CMake** como sistema de compilación y requiere la biblioteca **SDL2** para funcionar. Al compilar el menú se compilan también todos los juegos, cada uno en su propia carpeta `build`.

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

- **Windows:** se recomienda utilizar **vcpkg**, o descargar los archivos de desarrollo de SDL2 para MinGW y pasar su carpeta con `-DCMAKE_PREFIX_PATH=...`. Esa ruta se pasa también a los juegos.

## Compilación desde la terminal

Desde la carpeta `menu`:

```bash
mkdir build
cd build
cmake ..
cmake --build .
./kubix
```

`cmake --build .` compila el menú (`kubix`) y luego Road Fighter, Space Invaders, Snake, Pong y Tetris en `<juego>/build`, igual que si se compilaran por separado.

En **WSL** con Windows 11, las ventanas se abren en el escritorio de Windows a través de WSLg.

### Opciones

| Opción | Uso |
|---|---|
| `-DKUBIX_BUILD_GAMES=OFF` | Compila solo el menú. Los juegos se compilan aparte, cada uno con su `forDev.md`. |
| `./kubix <ruta>` | Usa otra raíz del repositorio para buscar los juegos. Por defecto se usa la carpeta donde está `menu`, guardada al compilar. |

Si un juego se compiló antes por separado con otro generador de CMake (por ejemplo Ninja), hay que borrar su carpeta `build` para que el menú pueda compilarlo con el suyo.

## Regenerar los sprites

Los sprites de `include/assets.h` se generan a partir de los PNG de [`assets/Menus`](../assets/Menus). Desde la raíz del repositorio (requiere Python con Pillow):

```bash
python3 TOOLS/converter.py -i assets/Menus -o menu/include/assets.h
```

`converter.py` reescribe `assets.h` completo, cabecera incluida, así que se puede ejecutar las veces que haga falta. Los PNG con transparencia (canal alfa o el color `#0F081F`) quedan con el color transparente `0x0843`.
