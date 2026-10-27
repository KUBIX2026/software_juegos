# Menú principal KUBIX

Menú de inicio de la consola. Muestra la pantalla de espera, deja elegir un juego en el selector y abre el juego elegido. Cuando el juego termina, el menú vuelve a aparecer.

## Pantallas

1. **Pantalla de espera** (`waitingScreen`): el logo de KUBIX con *PRESS A*.
2. **Selector de juegos** (`GameSelector` y `GameSelector2`): cuadrícula de 2 × 2 fichas en dos páginas. La ficha elegida se marca con borde azul y una línea blanca por dentro. Una flecha azul en el borde de la pantalla indica que hay otra página hacia ese lado.
3. **Aviso *NOT BUILT***: aparece si el juego elegido todavía no está compilado.

| Página | Arriba a la izquierda | Arriba a la derecha | Abajo a la izquierda | Abajo a la derecha |
|---|---|---|---|---|
| 1 | Road Fighter | Space Invaders | Snake | Pong |
| 2 | Tetris | (libre) | (libre) | (libre) |

## Controles

| Acción | Control |
|---|---|
| Salir de la pantalla de espera | A o Enter |
| Mover la selección | Flechas (al salir por un costado se pasa a la otra página) |
| Abrir el juego | A o Enter |
| Volver a la pantalla de espera | B |

Para volver al menú desde un juego, se sale del juego: en Snake, Space Invaders, Pong y Tetris con **B** en su menú, o cerrando su ventana (en Road Fighter, por ahora, solo cerrando la ventana).

## Cómo se abren los juegos

En el simulador cada juego es un programa aparte. El menú busca el ejecutable en la carpeta `build` de cada juego (también en `build/Release` y `build/Debug`, donde lo deja Visual Studio), oculta su ventana, lo ejecuta desde esa carpeta y espera a que termine. Ejecutarlo desde su carpeta `build` es necesario porque Pong y Tetris cargan sus sonidos desde `../assets`.

Cada juego ya tiene un punto de enganche para esto: su función `backToGeneralMenu()` termina el programa, y con eso se vuelve al menú.

En la FPGA no habrá programas separados, así que ahí el menú y los juegos tendrán que compilarse juntos en un solo programa.

## Agregar un juego

1. Agregar su ficha a `GameSelector2.png` (o a una página nueva) en [`assets/Menus`](../assets/Menus) y regenerar `include/assets.h`.
2. Agregar el juego a la lista `games` de `src/main.c`, en el mismo orden que las fichas: nombre, carpeta y nombre del ejecutable.
3. Agregar su carpeta a la lista de juegos de `CMakeLists.txt`.

## Estructura del proyecto

```text
menu/
├── CMakeLists.txt
├── README.md
├── forDev.md
├── src/
│   └── main.c
└── include/
    ├── assets.h
    ├── basic_font.h
    └── hal.h
```

## Ejecución

Para compilar el menú y los juegos, consulte [`forDev.md`](./forDev.md).
