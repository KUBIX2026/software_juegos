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
| Breakout | [`breakout`](./breakout) | Planeado |
| Tetris | [`tetris`](./tetris) | En desarrollo |

---

## Arquitectura general

La lógica de los juegos debe comunicarse con el hardware mediante una capa de abstracción:

```text
Juego
  ↓
HAL / capa de abstracción
  ↓
Simulador o hardware KUBIX
En el simulador, las funciones de pantalla, controles y audio utilizan bibliotecas de PC, principalmente SDL2.
En la integración con KUBIX, esas mismas funciones deberán comunicarse con los módulos del sistema:
- Display-Driver.
- nes_controller.
- PS2_keyboard.
- ps2_mouse.
- I2S.
- SPI-Flash.
- SPI-RAM.
- UART.
- I2C.
Lógica del menú y de los juegos
El flujo funcional general del sistema se documenta en el siguiente diagrama:
Lógica del menú y de los juegos
El flujo general contempla:
Inicialización del sistema
        ↓
Pantalla de inicio
        ↓
Menú principal
        ↓
Selección del juego
        ↓
Carga del juego
        ↓
Ejecución del juego
        ↓
Video, audio y controles
        ↓
Puntajes y regreso al menú
Estado de integración
Actualmente los juegos no tienen todos el mismo nivel de integración.
Algunos utilizan directamente SDL2 o la consola de Windows, mientras que otros utilizan una HAL más cercana a la arquitectura prevista para KUBIX.
Por esta razón, cada README de juego indica:
- Qué está implementado.
- En qué plataforma funciona.
- Qué controles utiliza.
- Qué falta para integrarlo con la FPGA.
Simulador HAL
El simulador de la capa de abstracción se encuentra en:
halSimulator (./halSimulator)
Su objetivo es permitir probar funciones de pantalla, controles y audio sin utilizar todavía la FPGA.
Para compilarlo se necesita:
- CMake.
- Un compilador de C.
- SDL2.
- Los archivos de desarrollo de SDL2.
La guía de compilación se encuentra en:
halSimulator/README.md (./halSimulator/README.md)
Assets
Los assets gráficos y de audio se conservan dentro del repositorio.