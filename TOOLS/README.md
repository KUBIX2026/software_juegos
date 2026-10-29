# TOOLS

Herramientas del repositorio.

## converter.py

Convierte los PNG de una carpeta de assets en el archivo `include/assets.h` de un juego: un arreglo `const uint16_t` en formato RGB565 por cada sprite. La consola no lee PNG, lee esos números.

Requiere Python y Pillow (`pip install pillow`). Se ejecuta desde la raíz del repositorio:

```bash
python3 TOOLS/converter.py -i snake/assets -o snake/include/assets.h --exclude MainMenuV1 SnakePlaying
python3 TOOLS/converter.py -i Tetris/assets -o Tetris/include/assets.h --macros
python3 TOOLS/converter.py -i pong/assets -o pong/include/assets.h --macros --exclude MainMenuV1 MainMenuV2 PauseMenu PongPlaying
```

| Opción | Uso |
|---|---|
| `-i`, `--input` | Carpeta con los PNG (obligatoria). |
| `-o`, `--output` | Archivo `.h` de salida (por defecto `./include/assets.h`). |
| `--exclude` | Nombres de PNG (sin `.png`) que no se convierten, por ejemplo diseños que el juego no usa y no deben ocupar memoria. Un nombre que no existe es un error. |
| `--macros` | Agrega `Nombre_W` y `Nombre_H` con el tamaño de cada sprite (formato de Tetris y Pong). |

Cómo se comporta:

- **Reescribe el archivo completo**, cabecera incluida, así que se puede ejecutar las veces que haga falta.
- **Transparencia:** un píxel con alfa menor que 128, o del color `#0F081F` de la convención del repositorio, se escribe como `0x0843`, el color que `sendSprite()` omite.
- **Los sprites salen en orden alfabético** y el nombre del arreglo es el del archivo, que debe ser un identificador válido de C.
- **Si algo falla, no escribe nada:** una carpeta que no existe, un PNG ilegible, un nombre no válido, una carpeta sin PNG o un sprite de más de 255 píxeles por lado. Avisa con `ERROR:` y termina con código distinto de cero.
- **Conserva los saltos de línea** (CRLF o LF) del archivo de salida si ya existe, para que `git diff` solo muestre los sprites que cambiaron.

## Comandos de cada juego

Desde la raíz del repositorio:

```bash
python3 TOOLS/converter.py -i roadFighter/assets -o roadFighter/include/assets.h --exclude CarFront Crash1 Crash2 Crash3 MainMenuV2 RoadFighterPlaying
python3 TOOLS/converter.py -i snake/assets -o snake/include/assets.h --exclude MainMenuV1 SnakePlaying
python3 TOOLS/converter.py -i space_invaders/assets -o space_invaders/include/assets.h --exclude MainMenuV1 SpaceInvadersPlaying
python3 TOOLS/converter.py -i breakout/assets -o breakout/include/assets.h --exclude BreakoutPlaying MainMenuV1
python3 TOOLS/converter.py -i pong/assets -o pong/include/assets.h --macros --exclude MainMenuV1 MainMenuV2 PauseMenu PongPlaying
python3 TOOLS/converter.py -i Tetris/assets -o Tetris/include/assets.h --macros
python3 TOOLS/converter.py -i menu/assets -o menu/include/assets.h
```

Los sprites que se omiten con `--exclude` siguen en `assets/`. Para usarlos en un juego, se quitan de su lista y se vuelve a generar `assets.h`.

## compilar_windows.bat y KUBIX.exe

Compilan en Windows, sin CMake, los 6 juegos, el menú de KUBIX y la app de escritorio `KUBIX.exe` (ventana con un clic por juego; el código está en `lanzador/KubixLauncher.cs`). Necesitan `gcc` (MinGW) y SDL2. Cada ejecutable queda en `<juego>/build`, junto con `SDL2.dll`; ni los ejecutables ni `build/` se suben al repositorio.

```bash
TOOLS\compilar_windows.bat
.\TOOLS\compilar_windows.ps1 -Sdl2Dir C:\ruta\SDL2-2.32.10 -AccesoDirecto
```

| Opción | Uso |
|---|---|
| `-Sdl2Dir` | Carpeta de SDL2 (o la variable de entorno `SDL2_DIR`). Si no se indica, busca en `%USERPROFILE%\dev\SDL2-*`, `C:\SDL2*` y similares. |
| `-AccesoDirecto` | Crea el icono KUBIX en el escritorio. |
| `-SinLanzador` | No compila `KUBIX.exe` (solo los juegos y el menú). |
| `-NoPausa` | No espera Enter al terminar. |

`KUBIX.exe --lanzar <clave> [segundos]` abre un juego directo y `KUBIX.exe --captura salida.png` guarda una imagen de la ventana.
