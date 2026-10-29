# Road Fighter

Road Fighter es el clásico videojuego de carreras desarrollado y publicado por Konami en 1984, considerado uno de los pioneros del género de carreras de vista cenital (top-down).

En este juego, el jugador controla un automóvil de carreras rojo que debe avanzar por la carretera, esquivar otros vehículos y llegar a la meta.

En esta versión para KUBIX el auto acelera, frena y cambia de carril entre el tráfico, mientras un tablero lateral muestra el tiempo y la distancia recorrida hasta la meta de 200 metros.

|  | |
| :---: | :---: |
| <img alt="Road Fighter" src="./img/juego.png" width="300"/> | <img alt="Demostración de Road Fighter" src="./img/road_fighter_demo.gif" width="300"/> |

## Características del juego

- Carretera de 4 carriles con desplazamiento vertical continuo (vallas y líneas que se mueven según la velocidad).
- Auto del jugador con aceleración y frenado progresivos (velocidad máxima de 10).
- Tráfico de 18 vehículos (autos azules, autos amarillos y camiones) repartidos a lo largo del recorrido.
- Detección de colisiones con el tráfico: el auto se muestra destruido unos instantes.
- Cronómetro (minutos y segundos) y contador de distancia en metros.
- Meta a los 200 metros: aparece el mensaje *FINISH*, el tiempo se detiene y con A o B se vuelve al menú.
- Menú de inicio con Singleplayer, Tutorial, Scores y Exit (como en el diagrama), pantalla de pausa y tabla de los 5 mejores tiempos.
- Sistema visual basado en sprites.

## Objetivo

Llegar a la meta, 200 metros más adelante, en el menor tiempo posible.

Durante el recorrido, el jugador debe:

- Esquivar los vehículos del tráfico cambiando de carril.
- Acelerar sin perder el control del auto.
- Mantener el ritmo para que el cronómetro marque el mejor tiempo.

## Controles

| Acción | Control |
|---|---|
| Acelerar | Flecha arriba |
| Frenar | Flecha abajo |
| Mover hacia la izquierda | Flecha izquierda |
| Mover hacia la derecha | Flecha derecha |
| Pausar la partida | Enter |
| Elegir una opción del menú | Flechas arriba y abajo |
| Confirmar en el menú, reanudar en la pausa o volver desde la meta | Tecla A (en el menú también Enter) |
| Volver al menú desde la pausa, el tutorial o los tiempos | Tecla B |

## Pantallas

1. **Menú:** SINGLEPLAYER, TUTORIAL, SCORES y EXIT, como en el diagrama. No tiene Multiplayer porque el juego es de un solo jugador. EXIT (o B) termina el programa y con eso se vuelve al menú de KUBIX.
2. **Partida:** SINGLEPLAYER empieza una carrera nueva desde cero. A la derecha se ven el tiempo (minutos y segundos) y la distancia en metros.
3. **Pausa:** con Enter durante la partida. A reanuda y B vuelve al menú (la carrera se abandona).
4. **Meta:** al llegar a los 200 metros aparece *FINISH*, el tiempo queda congelado y, tras un instante, la pista *A: BACK*. El tiempo entra en la tabla si está entre los 5 mejores.
5. **Tutorial y Scores:** TUTORIAL muestra los controles y SCORES los 5 mejores tiempos (se guardan en memoria mientras corre el juego).

| Menú | Pausa | Meta |
| :---: | :---: | :---: |
| <img alt="Pantalla de inicio de Road Fighter" src="./img/menu.png" width="150"/> | <img alt="Pausa de Road Fighter" src="./img/pausa.png" width="150"/> | <img alt="Meta de Road Fighter" src="./img/fin.png" width="150"/> |

## Diseño visual

El juego usa los sprites de [`roadFighter/assets`](./assets), convertidos a `include/assets.h`. La estética se inspira en las máquinas arcade de carreras de los años 80.

<div align="center">
  <img width="428" height="288" alt="Paleta de assets de Road Fighter" src="./img/assets_road_fighter.png" />
</div>

| Asset | Uso |
|---|---|
| `MainMenu` | Fondo del menú (el título; los iconos de modos que trae se tapan y se escribe la lista de opciones). |
| `PauseMenu` | Pantalla de pausa. |
| `SimplerRoad` | Fondo de la carretera. |
| `fences`, `roadLines` | Vallas y líneas que se desplazan sobre la carretera. |
| `RedCar` | Auto del jugador. |
| `BlueCar`, `YellowCar`, `Truck` | Vehículos del tráfico. |
| `CarDestroyed` | Auto del jugador tras un choque. |

`CarFront`, `Crash1`, `Crash2`, `Crash3`, `MainMenuV2` y `RoadFighterPlaying` están en `roadFighter/assets` pero el juego no los usa, así que se omiten al generar `assets.h` (`--exclude`) y no ocupan memoria. `Road` solo se usa en una pantalla de ajustes a la que aún no se llega desde el juego.

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
└── img/
    ├── juego.png, menu.png, pausa.png, fin.png
    ├── assets_road_fighter.png
    └── road_fighter_demo.gif
```

## Tecnologías utilizadas

- Lenguaje C.
- SDL2.
- CMake.
- Sprites gráficos.
- Sistema de entrada basado en controles.
- Arquitectura orientada a una plataforma de videojuegos retro.

## Ejecución

El proyecto se compila con CMake y SDL2. Para obtener información sobre la compilación y ejecución, consulte [`forDev.md`](./forDev.md).

## Límites conocidos

- Los choques solo muestran el auto destruido: todavía no restan velocidad, vidas ni puntos.
- El tráfico va a velocidad constante y en posiciones fijas; no hay combustible ni varias etapas.
- No hay modo de 2 jugadores, por eso el menú no tiene Multiplayer.
- Los mejores tiempos se guardan solo en memoria mientras corre el juego y no piden nombre.
- EXIT y B en el menú terminan el programa; el menú general de KUBIX (ver [`menu`](../menu)) lo vuelve a mostrar.
