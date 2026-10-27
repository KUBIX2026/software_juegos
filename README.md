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
| Road Fighter | [`roadFighter`](./roadFighter) | En desarrollo |
| Snake | [`snake`](./snake) | Prototipo funcional |
| Space Invaders | [`space_invaders`](./space_invaders) | Prototipo |
| Pong | [`pong`](./pong) | En pruebas |
| Breakout | [`breakout`](./breakout) | Prototipo |
| Tetris | [`tetris`](./tetris) | Planeado |

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

## Arquitectura general

![Diagrama del menú e inicio de juego](<./docs/diagramas/Diagrama logica juegos.png>)
