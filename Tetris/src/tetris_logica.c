#include "tetris_logica.h"

/* Celdas de cada pieza (x, y) dentro de su caja de 4x4, para las 4 rotaciones
 * (0 = aparicion, 1 = giro a la derecha, 2, 3). */
static const int8_t FORMAS[TETRIS_PIEZAS][4][4][2] = {
    /* I */ {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
             {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
             {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
             {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    /* O */ {{{1, 0}, {2, 0}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
    /* T */ {{{1, 0}, {0, 1}, {1, 1}, {2, 1}},
             {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
             {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
             {{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
    /* S */ {{{1, 0}, {2, 0}, {0, 1}, {1, 1}},
             {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
             {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
             {{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
    /* Z */ {{{0, 0}, {1, 0}, {1, 1}, {2, 1}},
             {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
             {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
             {{1, 0}, {0, 1}, {1, 1}, {0, 2}}},
    /* J */ {{{0, 0}, {0, 1}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
             {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
             {{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
    /* L */ {{{2, 0}, {0, 1}, {1, 1}, {2, 1}},
             {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
             {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
             {{0, 0}, {1, 0}, {1, 1}, {1, 2}}}};

/* Cuadros por fila segun el nivel (a unos 33 cuadros por segundo). */
static const uint8_t GRAVEDAD[TETRIS_NIVEL_MAX + 1] = {
    26, 23, 20, 18, 15, 13, 10, 7, 5, 4, 3, 3, 3, 2, 2, 1};

/* Puntos base por 1, 2, 3 y 4 lineas (se multiplican por nivel + 1). */
static const uint16_t PUNTOS_LINEAS[5] = {0, 40, 100, 300, 1200};

/* Desplazamientos (dx, dy) que se prueban, en orden, cuando la pieza no cabe al
 * rotar: el sistema estandar SRS. dy positivo es hacia abajo en pantalla. Hay 8
 * giros: 0->1, 1->0, 1->2, 2->1, 2->3, 3->2, 3->0 y 0->3 (rotaciones 0..3). */
static const int8_t KICKS_JLSTZ[8][5][2] = {
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},
    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}};
static const int8_t KICKS_I[8][5][2] = {
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},
    {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
    {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}};

const int8_t *tetris_kicks(uint8_t tipo, uint8_t desde, uint8_t hacia) {
  int id = hacia == ((desde + 1) & 3) ? desde * 2 : (desde == 0 ? 7 : desde * 2 - 1);
  return tipo == TETRIS_I ? &KICKS_I[id][0][0] : &KICKS_JLSTZ[id][0][0];
}

static uint32_t tetris_rand(EstadoTetris *e) {
  uint32_t x = e->prng_estado;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  e->prng_estado = x;
  return x >> 8;
}

const int8_t *tetris_forma(uint8_t tipo, uint8_t rot) {
  return &FORMAS[tipo][rot & 3][0][0];
}

uint8_t tetris_frames_por_fila(uint8_t nivel) {
  return GRAVEDAD[nivel > TETRIS_NIVEL_MAX ? TETRIS_NIVEL_MAX : nivel];
}

int tetris_colision(const EstadoTetris *e, uint8_t tipo, uint8_t rot, int px,
                    int py) {
  const int8_t *f = tetris_forma(tipo, rot);
  for (int i = 0; i < 4; i++) {
    int x = px + f[2 * i];
    int y = py + f[2 * i + 1];
    if (x < 0 || x >= TETRIS_COLUMNAS || y < 0 || y >= TETRIS_FILAS_TOTAL)
      return 1;
    if (e->tablero[y][x])
      return 1;
  }
  return 0;
}

int8_t tetris_sombra_py(const EstadoTetris *e) {
  int py = e->py;
  while (!tetris_colision(e, e->tipo, e->rot, e->px, py + 1))
    py++;
  return (int8_t)py;
}

uint8_t tetris_celda(const EstadoTetris *e, int columna, int fila) {
  if (columna < 0 || columna >= TETRIS_COLUMNAS || fila < 0 ||
      fila >= TETRIS_FILAS)
    return 0;
  return e->tablero[fila + TETRIS_FILAS_OCULTAS][columna];
}

static void llenar_bolsa(EstadoTetris *e) {
  for (uint8_t i = 0; i < TETRIS_PIEZAS; i++)
    e->bolsa[i] = i;
  for (int i = TETRIS_PIEZAS - 1; i > 0; i--) {
    uint32_t r;
    do {
      r = tetris_rand(e) & 7;
    } while (r > (uint32_t)i);
    uint8_t t = e->bolsa[i];
    e->bolsa[i] = e->bolsa[r];
    e->bolsa[r] = t;
  }
  e->bolsa_pos = 0;
}

uint8_t tetris_robar_pieza(EstadoTetris *e) {
  if (e->bolsa_pos >= TETRIS_PIEZAS)
    llenar_bolsa(e);
  return e->bolsa[e->bolsa_pos++];
}

static void terminar(EstadoTetris *e) {
  e->juego_terminado = 1;
  e->eventos |= TETRIS_EVT_FIN;
}

static void aparecer(EstadoTetris *e) {
  e->tipo = e->siguiente;
  e->siguiente = tetris_robar_pieza(e);
  e->rot = 0;
  e->px = 3;
  e->py = TETRIS_APARICION_Y;
  e->py_max = TETRIS_APARICION_Y;
  e->gravedad_cuenta = 0;
  e->bloqueo_cuenta = 0;
  e->bloqueo_reinicios = 0;
  if (tetris_colision(e, e->tipo, e->rot, e->px, e->py))
    terminar(e);
}

void tetris_inicializar(EstadoTetris *e, uint32_t semilla) {
  uint8_t *bytes = (uint8_t *)e; /* deja tambien el relleno en cero */
  for (unsigned i = 0; i < sizeof(*e); i++)
    bytes[i] = 0;
  e->prng_estado = semilla ? semilla : 0x9E3779B9u;
  e->bolsa_pos = TETRIS_PIEZAS; /* fuerza a llenar la primera bolsa */
  e->siguiente = tetris_robar_pieza(e);
  aparecer(e);
}

static void sumar_puntos(EstadoTetris *e, uint32_t puntos) {
  e->puntaje += puntos;
  if (e->puntaje > TETRIS_PUNTAJE_MAX)
    e->puntaje = TETRIS_PUNTAJE_MAX;
}

/* Quita las filas completas y baja las de arriba; devuelve cuantas quito. */
static int limpiar_lineas(EstadoTetris *e) {
  int destino = TETRIS_FILAS_TOTAL - 1;
  int quitadas = 0;
  for (int origen = TETRIS_FILAS_TOTAL - 1; origen >= 0; origen--) {
    int llena = 1;
    for (int c = 0; c < TETRIS_COLUMNAS; c++)
      if (!e->tablero[origen][c])
        llena = 0;
    if (llena) {
      quitadas++;
      continue;
    }
    if (destino != origen)
      for (int c = 0; c < TETRIS_COLUMNAS; c++)
        e->tablero[destino][c] = e->tablero[origen][c];
    destino--;
  }
  for (; destino >= 0; destino--)
    for (int c = 0; c < TETRIS_COLUMNAS; c++)
      e->tablero[destino][c] = 0;
  return quitadas;
}

static void puntuar_lineas(EstadoTetris *e, int n) {
  e->ultimas_lineas = (uint8_t)n;
  if (n == 0)
    return;
  for (int k = 0; k <= e->nivel; k++)
    sumar_puntos(e, PUNTOS_LINEAS[n]);
  e->lineas = (uint16_t)(e->lineas > 65535 - n ? 65535 : e->lineas + n);
  e->lineas_nivel = (uint8_t)(e->lineas_nivel + n);
  while (e->lineas_nivel >= TETRIS_LINEAS_POR_NIVEL) {
    e->lineas_nivel = (uint8_t)(e->lineas_nivel - TETRIS_LINEAS_POR_NIVEL);
    if (e->nivel < TETRIS_NIVEL_MAX) {
      e->nivel++;
      e->eventos |= TETRIS_EVT_NIVEL;
    }
  }
  e->eventos = (uint8_t)(e->eventos |
                         (n == 4 ? TETRIS_EVT_TETRIS : TETRIS_EVT_LINEA));
}

static void fijar(EstadoTetris *e) {
  const int8_t *f = tetris_forma(e->tipo, e->rot);
  int todas_ocultas = 1;
  for (int i = 0; i < 4; i++) {
    int x = e->px + f[2 * i];
    int y = e->py + f[2 * i + 1];
    e->tablero[y][x] = (uint8_t)(e->tipo + 1);
    if (y >= TETRIS_FILAS_OCULTAS)
      todas_ocultas = 0;
  }
  e->piezas++;
  e->abajo_bloqueado = 1;
  e->eventos |= TETRIS_EVT_FIJAR;
  int n = limpiar_lineas(e);
  puntuar_lineas(e, n);
  if (todas_ocultas)
    terminar(e); /* la pieza quedo fijada por encima del tablero */
  else
    aparecer(e);
}

/* Al mover o rotar una pieza apoyada se reinicia la espera de bloqueo, hasta
 * un maximo de reinicios para que no se pueda evitar para siempre. */
static void reiniciar_bloqueo(EstadoTetris *e) {
  if (e->bloqueo_cuenta > 0 && e->bloqueo_reinicios < TETRIS_BLOQUEO_REINICIOS) {
    e->bloqueo_cuenta = 0;
    e->bloqueo_reinicios++;
  }
}

static int mover(EstadoTetris *e, int dx) {
  if (tetris_colision(e, e->tipo, e->rot, e->px + dx, e->py))
    return 0;
  e->px = (int8_t)(e->px + dx);
  e->eventos |= TETRIS_EVT_MOVER;
  reiniciar_bloqueo(e);
  return 1;
}

static int rotar(EstadoTetris *e, int sentido) {
  if (e->tipo == TETRIS_O)
    return 0; /* el cuadrado no cambia al rotar */
  uint8_t nueva = (uint8_t)((e->rot + sentido) & 3);
  const int8_t *kicks = tetris_kicks(e->tipo, e->rot, nueva);
  for (int k = 0; k < 5; k++) {
    int px = e->px + kicks[2 * k];
    int py = e->py + kicks[2 * k + 1];
    if (!tetris_colision(e, e->tipo, nueva, px, py)) {
      e->rot = nueva;
      e->px = (int8_t)px;
      e->py = (int8_t)py;
      e->eventos |= TETRIS_EVT_ROTAR;
      reiniciar_bloqueo(e);
      return 1;
    }
  }
  return 0;
}

/* Izquierda y derecha: un paso al presionar y, tras un momento, repeticion. */
static void horizontal(EstadoTetris *e, const EntradaTetris *in) {
  int dir = 0;
  if (in->izquierda && !in->derecha)
    dir = -1;
  else if (in->derecha && !in->izquierda)
    dir = 1;
  if (dir == 0) {
    e->das_dir = 0;
    e->das_cuenta = 0;
    return;
  }
  if (e->das_dir != dir) {
    e->das_dir = (int8_t)dir;
    e->das_cuenta = 0;
    mover(e, dir);
    return;
  }
  e->das_cuenta++;
  if (e->das_cuenta >= TETRIS_DAS_INICIAL) {
    mover(e, dir);
    e->das_cuenta = TETRIS_DAS_INICIAL - TETRIS_DAS_REPETICION;
  }
}

void tetris_actualizar(EstadoTetris *e, EntradaTetris in) {
  e->eventos = 0;
  e->ultimas_lineas = 0;
  if (e->juego_terminado)
    return;
  /* Si "abajo" seguia presionado al fijarse la pieza anterior, la nueva no cae
   * rapido hasta que se suelte la tecla. */
  if (e->abajo_bloqueado) {
    if (!in.abajo)
      e->abajo_bloqueado = 0;
    in.abajo = 0;
  }

  if (in.rotar_der)
    rotar(e, 1);
  else if (in.rotar_izq)
    rotar(e, 3);
  horizontal(e, &in);

  if (in.caida_directa) {
    int filas = 0;
    while (!tetris_colision(e, e->tipo, e->rot, e->px, e->py + 1)) {
      e->py++;
      filas++;
    }
    sumar_puntos(e, (uint32_t)filas);
    sumar_puntos(e, (uint32_t)filas); /* 2 puntos por fila */
    fijar(e);
    return;
  }

  uint8_t periodo = GRAVEDAD[e->nivel];
  if (in.abajo && periodo > TETRIS_CAIDA_SUAVE_FRAMES)
    periodo = TETRIS_CAIDA_SUAVE_FRAMES;
  e->gravedad_cuenta++;
  if (e->gravedad_cuenta >= periodo) {
    e->gravedad_cuenta = 0;
    if (!tetris_colision(e, e->tipo, e->rot, e->px, e->py + 1)) {
      e->py++;
      if (in.abajo)
        sumar_puntos(e, 1);
      if (e->py > e->py_max) {
        e->py_max = e->py;
        e->bloqueo_cuenta = 0;
        e->bloqueo_reinicios = 0;
      }
    }
  }

  if (tetris_colision(e, e->tipo, e->rot, e->px, e->py + 1)) {
    e->bloqueo_cuenta++;
    if (in.abajo || e->bloqueo_cuenta >= TETRIS_BLOQUEO_FRAMES)
      fijar(e);
  } else {
    e->bloqueo_cuenta = 0;
  }
}

int tetris_registrar_puntaje(uint32_t top[TETRIS_TOP_N], uint32_t puntaje) {
  if (puntaje <= top[TETRIS_TOP_N - 1])
    return 0;
  int i = TETRIS_TOP_N - 1;
  while (i > 0 && top[i - 1] < puntaje) {
    top[i] = top[i - 1];
    --i;
  }
  top[i] = puntaje;
  return 1;
}
