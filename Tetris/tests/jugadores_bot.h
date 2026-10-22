#pragma once
#include "tetris_logica.h"

/* Jugador automatico para probar la jugabilidad (solo pruebas). Para cada pieza
 * nueva evalua todas las rotaciones y columnas y elige la que deja el tablero
 * con menos huecos y menos altura; despues la mueve con pulsos de teclado. */

typedef struct {
  uint32_t pieza_planeada; /* numero de pieza para la que hay plan */
  int hay_plan;
  int rot_obj, px_obj;
  int tic;
  int espera;
} BotTetris;

static inline int bot_evaluar(const EstadoTetris *e, int rot, int px) {
  if (tetris_colision(e, e->tipo, (uint8_t)rot, px, TETRIS_APARICION_Y))
    return -1000000;
  int py = TETRIS_APARICION_Y;
  while (!tetris_colision(e, e->tipo, (uint8_t)rot, px, py + 1))
    py++;
  uint8_t t[TETRIS_FILAS_TOTAL][TETRIS_COLUMNAS];
  for (int y = 0; y < TETRIS_FILAS_TOTAL; y++)
    for (int x = 0; x < TETRIS_COLUMNAS; x++)
      t[y][x] = e->tablero[y][x];
  const int8_t *f = tetris_forma(e->tipo, (uint8_t)rot);
  for (int i = 0; i < 4; i++)
    t[py + f[2 * i + 1]][px + f[2 * i]] = 1;

  int lineas = 0;
  for (int y = 0; y < TETRIS_FILAS_TOTAL; y++) {
    int llena = 1;
    for (int x = 0; x < TETRIS_COLUMNAS; x++)
      if (!t[y][x])
        llena = 0;
    lineas += llena;
  }
  int alturas[TETRIS_COLUMNAS], huecos = 0, suma = 0, rugosidad = 0;
  for (int x = 0; x < TETRIS_COLUMNAS; x++) {
    int h = 0, bloque = 0;
    for (int y = 0; y < TETRIS_FILAS_TOTAL; y++) {
      if (t[y][x]) {
        if (!bloque) {
          h = TETRIS_FILAS_TOTAL - y;
          bloque = 1;
        }
      } else if (bloque) {
        huecos++;
      }
    }
    alturas[x] = h;
    suma += h;
  }
  for (int x = 0; x + 1 < TETRIS_COLUMNAS; x++)
    rugosidad += alturas[x] > alturas[x + 1] ? alturas[x] - alturas[x + 1]
                                              : alturas[x + 1] - alturas[x];
  return -510 * suma + 760 * lineas - 356 * huecos - 184 * rugosidad;
}

static inline EntradaTetris bot_decidir(BotTetris *b, const EstadoTetris *e) {
  EntradaTetris in = {0, 0, 0, 0, 0, 0};
  b->tic++;
  if (!b->hay_plan || b->pieza_planeada != e->piezas) {
    int mejor = -2000000;
    for (int rot = 0; rot < 4; rot++)
      for (int px = -2; px < TETRIS_COLUMNAS; px++) {
        int v = bot_evaluar(e, rot, px);
        if (v > mejor) {
          mejor = v;
          b->rot_obj = rot;
          b->px_obj = px;
        }
      }
    b->hay_plan = 1;
    b->pieza_planeada = e->piezas;
    b->espera = 0;
  }
  b->espera++;
  if (b->espera > 80) { /* no logro acomodarla: la suelta donde esta */
    in.caida_directa = 1;
    return in;
  }
  if (b->tic & 1)
    return in; /* cuadros impares: suelta las teclas */
  if (e->rot != b->rot_obj)
    in.rotar_der = 1;
  else if (e->px < b->px_obj)
    in.derecha = 1;
  else if (e->px > b->px_obj)
    in.izquierda = 1;
  else
    in.caida_directa = 1;
  return in;
}
