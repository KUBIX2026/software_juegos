#pragma once
#include <stdint.h>

/* Reglas de Pong. No depende de SDL ni de hal.h: se prueba con gcc solo.
 * Aritmetica entera (la CPU de KUBIX es RV32I, sin float). */

enum {
  PONG_ANCHO = 64,
  PONG_ALTO = 64,
  PONG_PUNTOS_PARA_GANAR = 7,
  PONG_PALETA_ANCHO = 2,
  PONG_PALETA_IZQ_ALTO = 10,
  PONG_PALETA_DER_ALTO = 10,
  PONG_PALETA_IZQ_X = 2,
  PONG_PALETA_DER_X = 60,
  PONG_PALETA_VELOCIDAD = 2,
  PONG_PELOTA_LADO = 2,
  PONG_FRAC_BITS = 4, /* la pelota se mueve en 1/16 de pixel */
  PONG_NIVELES_VEL = 4,
  PONG_GOLPES_POR_NIVEL = 3,
  PONG_ESPERA_SAQUE = 20, /* cuadros antes de que salga la pelota */
  PONG_TOP_N = 5          /* puntajes que guarda la tabla de mejores */
};

typedef enum {
  PONG_ACCION_NINGUNA = 0,
  PONG_ACCION_ARRIBA,
  PONG_ACCION_ABAJO
} AccionPong;

typedef enum { PONG_UN_JUGADOR = 0, PONG_DOS_JUGADORES } ModoPong;

typedef enum { PONG_IA_FACIL = 0, PONG_IA_NORMAL, PONG_IA_DIFICIL } NivelIA;

/* Eventos del ultimo cuadro (mascara de bits) para disparar sonidos. */
enum {
  PONG_EVT_PALETA = 1,
  PONG_EVT_PARED = 2,
  PONG_EVT_PUNTO = 4,
  PONG_EVT_FIN = 8
};

typedef struct {
  int16_t paleta_izq_y; /* pixeles, esquina superior */
  int16_t paleta_der_y;
  int16_t pelota_x16; /* 1/16 de pixel, esquina superior izquierda */
  int16_t pelota_y16;
  int16_t pelota_vx; /* 1/16 de pixel por cuadro */
  int16_t pelota_vy;
  uint8_t puntos_izq;
  uint8_t puntos_der;
  uint8_t juego_terminado;
  uint8_t ganador;  /* 0 = nadie, 1 = izquierda, 2 = derecha */
  uint8_t modo;     /* ModoPong */
  uint8_t nivel_ia; /* NivelIA */
  uint8_t golpes;   /* golpes seguidos en el punto actual */
  uint8_t espera_saque;
  uint8_t eventos;
  uint8_t ia_tick;
  int8_t ia_error;
  int16_t ia_objetivo; /* y (pixeles) a la que la IA quiere llevar su paleta */
  uint8_t ia_acum;     /* fraccion de pixel acumulada (1/16) de la velocidad de la IA */
  uint32_t prng_estado;
} EstadoPong;

void pong_inicializar(EstadoPong *e, uint32_t semilla, ModoPong modo,
                      NivelIA nivel);
/* En PONG_UN_JUGADOR la accion `der` se ignora y la mueve la IA. */
void pong_actualizar(EstadoPong *e, AccionPong izq, AccionPong der);

int16_t pong_pelota_x(const EstadoPong *e); /* pixeles */
int16_t pong_pelota_y(const EstadoPong *e);
uint8_t pong_nivel_velocidad(const EstadoPong *e); /* 0..PONG_NIVELES_VEL-1 */

/* Puntaje de una partida de un jugador: margen de puntos escalado por la
 * dificultad (0 si pierde 0-7 en facil, 42 si gana 7-0 en dificil). */
int pong_puntaje(const EstadoPong *e);
/* Inserta `puntaje` en la tabla (de mayor a menor); 1 si entro al top. */
int pong_registrar_puntaje(int top[PONG_TOP_N], int puntaje);
