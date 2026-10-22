#pragma once
#include "pong_logica.h"

/* Jugadores automaticos para probar la jugabilidad (solo pruebas). */

/* Cuadros hasta que la pelota llega al plano de la paleta izquierda,
 * simulando una copia del estado; devuelve y (centro de la pelota) en px. */
static inline int bot_prediccion_y(const EstadoPong *e) {
  EstadoPong c = *e;
  for (int i = 0; i < 400; i++) {
    int x_antes = c.pelota_x16;
    pong_actualizar(&c, PONG_ACCION_NINGUNA, PONG_ACCION_NINGUNA);
    if (c.pelota_vx > 0 || c.juego_terminado || c.espera_saque)
      break;
    if (x_antes >= 64 && c.pelota_x16 < 64)
      break;
  }
  return pong_pelota_y(&c) + 1;
}

/* Sigue el centro de la pelota (sin apuntar). */
static inline AccionPong bot_sigue(const EstadoPong *e) {
  int centro = e->paleta_izq_y + PONG_PALETA_IZQ_ALTO / 2;
  int cy = pong_pelota_y(e) + 1;
  if (centro < cy - 1)
    return PONG_ACCION_ABAJO;
  if (centro > cy + 1)
    return PONG_ACCION_ARRIBA;
  return PONG_ACCION_NINGUNA;
}

/* Predice donde llegara la pelota y la golpea con el borde opuesto a la IA
 * para mandarla lejos de ella. */
static inline AccionPong bot_apunta(const EstadoPong *e) {
  if (e->pelota_vx >= 0 || e->espera_saque) {
    int centro = e->paleta_izq_y + PONG_PALETA_IZQ_ALTO / 2;
    if (centro < 32 - 1)
      return PONG_ACCION_ABAJO;
    if (centro > 32 + 1)
      return PONG_ACCION_ARRIBA;
    return PONG_ACCION_NINGUNA;
  }
  int cy = bot_prediccion_y(e);
  int ia_centro = e->paleta_der_y + PONG_PALETA_DER_ALTO / 2;
  /* si la IA esta arriba, apuntar abajo (pos alta) y viceversa */
  int pos = ia_centro < 32 ? PONG_PALETA_IZQ_ALTO : 0;
  int objetivo = cy - pos;
  if (objetivo < 0)
    objetivo = 0;
  if (objetivo > PONG_ALTO - PONG_PALETA_IZQ_ALTO)
    objetivo = PONG_ALTO - PONG_PALETA_IZQ_ALTO;
  if (e->paleta_izq_y < objetivo - 1)
    return PONG_ACCION_ABAJO;
  if (e->paleta_izq_y > objetivo + 1)
    return PONG_ACCION_ARRIBA;
  return PONG_ACCION_NINGUNA;
}

/* Jugador que predice donde caera la pelota, como una persona con practica, pero
 * con error en la estimacion (+-ruido px) y reaccion lenta (decide cada `cada`
 * cuadros) y que apunta a los bordes de su paleta. Sirve para medir el equilibrio
 * de las dificultades. */
typedef struct {
  int objetivo, tic, cada, ruido;
  uint32_t r;
} BotPersona;

static inline int bot_persona_ruido(BotPersona *b) {
  b->r ^= b->r << 13;
  b->r ^= b->r >> 17;
  b->r ^= b->r << 5;
  return (int)((b->r >> 8) % (uint32_t)(2 * b->ruido + 1)) - b->ruido;
}

static inline AccionPong bot_persona(BotPersona *b, const EstadoPong *e) {
  int centro = e->paleta_izq_y + PONG_PALETA_IZQ_ALTO / 2;
  b->tic++;
  if (e->pelota_vx >= 0 || e->espera_saque) {
    b->objetivo = 32;
  } else if (b->tic % b->cada == 0) {
    int ia = e->paleta_der_y + PONG_PALETA_DER_ALTO / 2;
    int pos = ia < 32 ? PONG_PALETA_IZQ_ALTO - 1 : 0;
    b->objetivo = bot_prediccion_y(e) - pos + PONG_PALETA_IZQ_ALTO / 2 + bot_persona_ruido(b);
  }
  if (centro < b->objetivo - 1)
    return PONG_ACCION_ABAJO;
  if (centro > b->objetivo + 1)
    return PONG_ACCION_ARRIBA;
  return PONG_ACCION_NINGUNA;
}
