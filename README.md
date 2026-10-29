# Software de juegos KUBIX

Repositorio del software de juegos desarrollado para KUBIX, una plataforma retro basada en un sistema con CPU, memoria, pantalla, controles y audio.

Este repositorio contiene:

- Juegos desarrollados para el proyecto.
- Prototipos y juegos en desarrollo.
- Assets gráficos y de audio.
- Simuladores para probar la lógica antes de usar la FPGA.
- Documentación de la lógica del menú y de los juegos.

> En esta etapa, algunos juegos funcionan como prototipos independientes y todavía no están integrados completamente con el hardware KUBIX.

---

## Juegos del proyecto

| Juego | Carpeta | Estado |
|---|---|---|
| Road Fighter | [`roadFighter`](./roadFighter) | Prototipo funcional |
| Snake | [`snake`](./snake) | Prototipo funcional |
| Space Invaders | [`space_invaders`](./space_invaders) | Prototipo funcional |
| Pong | [`pong`](./pong) | Prototipo funcional |
| Breakout | [`breakout`](./breakout) | Prototipo funcional |
| Tetris | [`Tetris`](./Tetris) | Prototipo funcional |

---

## Menú principal

El menú de la consola está en [`menu`](./menu): muestra la pantalla de espera y el selector de juegos, y desde ahí abre Road Fighter, Space Invaders, Snake, Pong, Tetris o Breakout. Al compilarlo se compilan también todos los juegos:

```bash
cd menu
mkdir build && cd build
cmake ..
cmake --build .
./kubix
```
---

## Cómo correr los juegos en Windows (app de escritorio)

Para abrir cualquier juego con un clic, sin escribir comandos:

1. **Compilar (una sola vez):** doble clic en [`TOOLS/compilar_windows.bat`](./TOOLS/compilar_windows.bat). Compila los 6 juegos, el menú de KUBIX y la app **`KUBIX.exe`**. Necesita `gcc` (MinGW) y SDL2, pero no CMake. Si no encuentra SDL2 solo, se le indica la carpeta: `.\TOOLS\compilar_windows.ps1 -Sdl2Dir C:\ruta\SDL2-2.32.10`.
2. **Abrir:** doble clic en `KUBIX.exe` (en la carpeta del proyecto). Con `.\TOOLS\compilar_windows.ps1 -AccesoDirecto` también queda un icono **KUBIX** en el escritorio.
3. **Jugar:** un clic en la ficha del juego. Las teclas 1 a 6 y Enter también sirven. Al cerrar el juego, la ventana de KUBIX vuelve sola.

Otras formas de abrir un juego:

| Quiero | Cómo |
|---|---|
| El menú de la consola (selector con flechas y tecla A) | Botón **Menú KUBIX (consola)** de la app, o `menu\build\kubix.exe` |
| Un juego directo | `KUBIX.exe --lanzar pong` (claves: `roadfighter`, `spaceinvaders`, `snake`, `pong`, `tetris`, `breakout`, `menu`) |
| Un juego sin la app | `<juego>\build\<juego>.exe` (el de Tetris es `Tetris\build\tetris.exe`) |

Los controles del teclado equivalen al control NES: flechas, **Enter** = Start, tecla **A** y tecla **B**. El detalle de cada juego está en su README.

---

## Mapa de pantallas

![Mapa de pantallas de KUBIX](<./docs/diagramas/Mapa de pantallas.png>)

Maqueta visual de las pantallas de la consola: pantalla de espera, selector de juegos, menú de cada juego, partida y pausa (muestra Breakout, Road Fighter, Snake y Pong). Complementa al diagrama lógico de la sección siguiente.


---

## Arquitectura general

![Diagrama del menú e inicio de juego](<./docs/diagramas/Diagrama logica juegos.png>)

El diagrama reúne dos flujos conectados:

- **Diagrama 1, menú principal:** sirve para elegir uno de los 6 juegos. Los juegos se despliegan en la pantalla principal; con las flechas se mueve la selección, **A** abre el juego elegido y **B** no tiene efecto en este menú.
- **Diagrama 2, inicio de juego:** cubre el submenú del juego (Singleplayer, Multiplayer, Tutorial, Scores y Exit), la partida y el guardado del score. La flecha de **A** del Diagrama 1 llega a su inicio, y los conectores **M** (B en el submenú y Exit) regresan al **MENU** del Diagrama 1.

Detalles que fija el diagrama:

- Los sprites se cargan una sola vez, antes del ciclo de juego.
- En Multiplayer, quien envía el dato maneja el reloj.
- Cada juego define qué muestra la pantalla auxiliar (matriz LED 32×8 con MAX7219).
- El score se guarda en una EEPROM externa por I2C.
