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
