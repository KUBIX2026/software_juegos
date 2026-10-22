#pragma once
#include <stdint.h>

/* Reglas de Tetris. No depende de SDL ni de hal.h: se prueba con gcc solo.
 * Aritmetica entera, sin float ni divisiones; la unica multiplicacion es por 2
 * (la CPU de KUBIX es RV32I). */

enum {
  TETRIS_COLUMNAS = 10,
  TETRIS_FILAS = 20,         /* filas visibles */
  TETRIS_FILAS_OCULTAS = 2,  /* zona de aparicion, encima del tablero */
  TETRIS_APARICION_Y = 1,    /* fila de la caja al aparecer: ya se ve en pantalla */
  TETRIS_FILAS_TOTAL = TETRIS_FILAS + TETRIS_FILAS_OCULTAS,
  TETRIS_PIEZAS = 7,
  TETRIS_NIVEL_MAX = 15,
  TETRIS_LINEAS_POR_NIVEL = 10,
  TETRIS_PUNTAJE_MAX = 999999,
  TETRIS_TOP_N = 5,
  TETRIS_DAS_INICIAL = 8,     /* cuadros antes de repetir izquierda/derecha */
  TETRIS_DAS_REPETICION = 3,  /* cuadros entre repeticiones */
  TETRIS_BLOQUEO_FRAMES = 8,  /* cuadros de gracia antes de fijar la pieza */
  TETRIS_BLOQUEO_REINICIOS = 15,
  TETRIS_CAIDA_SUAVE_FRAMES = 2
};

/* Tipos de pieza; en el tablero cada celda guarda tipo + 1 (0 = vacia). */
enum { TETRIS_I = 0, TETRIS_O, TETRIS_T, TETRIS_S, TETRIS_Z, TETRIS_J, TETRIS_L };

/* Eventos del ultimo cuadro (mascara de bits) para disparar sonidos. */
enum {
  TETRIS_EVT_MOVER = 1,
  TETRIS_EVT_ROTAR = 2,
  TETRIS_EVT_FIJAR = 4,
  TETRIS_EVT_LINEA = 8,  /* se limpiaron 1, 2 o 3 lineas */
  TETRIS_EVT_TETRIS = 16, /* se limpiaron 4 lineas */
  TETRIS_EVT_NIVEL = 32,
  TETRIS_EVT_FIN = 64
};

/* izquierda, derecha y abajo son niveles (tecla mantenida); la caida directa
 * y las rotaciones son pulsos (solo el cuadro en que se presiona). */
typedef struct {
  uint8_t izquierda;
  uint8_t derecha;
  uint8_t abajo;
  uint8_t caida_directa;
  uint8_t rotar_der;
  uint8_t rotar_izq;
} EntradaTetris;

typedef struct {
  uint8_t tablero[TETRIS_FILAS_TOTAL][TETRIS_COLUMNAS];
  uint8_t tipo;      /* pieza que cae */
  uint8_t rot;       /* 0..3 */
  int8_t px;         /* columna de la caja de 4x4 de la pieza */
  int8_t py;         /* fila de la caja (0 = primera fila oculta) */
  int8_t py_max;     /* fila mas baja alcanzada por la pieza actual */
  uint8_t siguiente; /* vista previa */
  uint8_t bolsa[TETRIS_PIEZAS];
  uint8_t bolsa_pos;
  uint8_t nivel;     /* 0..TETRIS_NIVEL_MAX */
  uint8_t lineas_nivel;      /* lineas hacia el proximo nivel */
  uint8_t ultimas_lineas;    /* lineas del ultimo bloqueo (0..4) */
  uint8_t juego_terminado;
  uint8_t eventos;
  uint8_t gravedad_cuenta;
  uint8_t bloqueo_cuenta;
  uint8_t bloqueo_reinicios;
  uint8_t abajo_bloqueado; /* tras fijar una pieza, "abajo" no cuenta hasta soltarlo */
  uint8_t das_cuenta;
  int8_t das_dir;
  uint16_t lineas;   /* lineas totales */
  uint32_t puntaje;
  uint32_t piezas;   /* piezas fijadas */
  uint32_t prng_estado;
} EstadoTetris;

void tetris_inicializar(EstadoTetris *e, uint32_t semilla);
void tetris_actualizar(EstadoTetris *e, EntradaTetris entrada);

/* Bolsa de 7: cada tipo sale una vez por bolsa. */
uint8_t tetris_robar_pieza(EstadoTetris *e);

/* Las 4 celdas de la pieza: x0, y0, x1, y1, ... dentro de su caja de 4x4. */
const int8_t *tetris_forma(uint8_t tipo, uint8_t rot);
/* Las 5 pruebas (dx, dy) del sistema SRS al girar de `desde` a `hacia`. */
const int8_t *tetris_kicks(uint8_t tipo, uint8_t desde, uint8_t hacia);
int tetris_colision(const EstadoTetris *e, uint8_t tipo, uint8_t rot, int px,
                    int py);
int8_t tetris_sombra_py(const EstadoTetris *e); /* fila donde caeria la pieza */
uint8_t tetris_celda(const EstadoTetris *e, int columna, int fila); /* visible */
uint8_t tetris_frames_por_fila(uint8_t nivel);

/* Tabla de mejores puntajes (de mayor a menor); 1 si el puntaje entro. */
int tetris_registrar_puntaje(uint32_t top[TETRIS_TOP_N], uint32_t puntaje);
