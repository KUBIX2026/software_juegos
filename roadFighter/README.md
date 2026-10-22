# Road Fighter

Road Fighter es el clásico videojuego de carreras desarrollado y publicado por Konami en 1984, considerado uno de los pioneros del género de carreras de vista cenital (top-down).

En este juego, el jugador controla un automóvil de carreras rojo que debe avanzar por la carretera, esquivar otros vehículos y superar diferentes obstáculos hasta llegar a la meta.

La experiencia combina velocidad, reflejos y estrategia, ya que es necesario controlar el vehículo, evitar colisiones y administrar correctamente el recorrido para completar cada etapa.

|  | |
| :---: | :---: |
| <img alt="Road Fighter" src="https://github.com/user-attachments/assets/53e3281b-b5a2-47d7-ae04-d43d84a477ce"/> | <video width="400" controls src="https://github.com/user-attachments/assets/67fc98b9-b5a2-47d7-ae04-d43d84a477ce"></video> |

## Características del juego

- Carreras contra el reloj.
- Carretera con desplazamiento vertical.
- Vehículo controlado por el jugador.
- Tráfico dinámico.
- Diferentes tipos de vehículos.
- Obstáculos en la carretera.
- Colisiones con otros vehículos y elementos del escenario.
- Sistema de velocidad.
- Sistema de combustible.
- Diferentes etapas o recorridos.
- Meta de llegada.
- Pantallas de inicio, pausa y configuración.
- Sistema visual basado en sprites.
- Ambientación inspirada en los videojuegos arcade clásicos.

## Objetivo

El objetivo del juego es avanzar por la carretera y llegar a la meta antes de que se acabe el tiempo disponible.

Durante el recorrido, el jugador debe:

- Esquivar los vehículos del tráfico.
- Evitar los obstáculos.
- Mantener el control del automóvil.
- Administrar la velocidad.
- Utilizar los vehículos especiales para recargar combustible.
- Completar cada etapa de la carrera.

## Controles

| Acción | Control |
|---|---|
| Mover hacia arriba | Flecha arriba |
| Mover hacia abajo | Flecha abajo |
| Mover hacia la izquierda | Flecha izquierda |
| Mover hacia la derecha | Flecha derecha |
| Iniciar o confirmar | Enter |
| Acción A | Tecla A |
| Acción B | Tecla B |

## Diseño visual

El juego utiliza una estética inspirada en las máquinas arcade de carreras de los años 80.

La paleta de colores y los recursos gráficos fueron diseñados para representar:

- La carretera.
- Los vehículos.
- El escenario.
- Los obstáculos.
- Los elementos de la interfaz.
- La identidad visual retro del juego.

<div align="center">
  <img width="192" height="128" alt="Paleta de assets de Road Fighter" src="https://github.com/user-attachments/assets/d6b94a26-676e-412d-a016-44b89d41940c" />
</div>

## Desarrollo del juego

Road Fighter cuenta con una versión desarrollada en lenguaje C, con una estructura orientada a sistemas retro y videojuegos de recursos limitados.

Actualmente, el juego cuenta con:

- Pantalla de inicio.
- Pantalla principal de juego.
- Pantalla de pausa.
- Pantalla de configuración.
- Movimiento del vehículo.
- Movimiento de vehículos enemigos.
- Carretera con desplazamiento.
- Diferentes carriles.
- Distancia recorrida.
- Meta del recorrido.
- Detección de colisiones.
- Sprites y fondos gráficos.
- Video demostrativo del funcionamiento.

## Estructura del proyecto

```text
roadFighter/
├── CMakeLists.txt
├── README.md
├── forDev.md
├── src/
│   └── main.c
├── include/
│   ├── assets.h
│   ├── basic_font.h
│   └── hal.h
└── assets/
Tecnologías utilizadas
- Lenguaje C.
- SDL2.
- CMake.
- Sprites gráficos.
- Sistema de entrada basado en controles.
- Arquitectura orientada a una plataforma de videojuegos retro.
Ejecución
El proyecto puede compilarse utilizando CMake y SDL2.
Para obtener información sobre la compilación y ejecución, consulte:
roadFighter/forDev.md
Assets
Los recursos gráficos utilizados por el juego se encuentran en:
roadFighter/assets/
Estos recursos incluyen los elementos visuales necesarios para representar el automóvil, los vehículos del tráfico, la carretera y los diferentes componentes del escenario.