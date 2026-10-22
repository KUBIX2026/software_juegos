#include "pong_logica.h"

enum {
  PELOTA_MAX_X16 = (PONG_ANCHO - PONG_PELOTA_LADO) << PONG_FRAC_BITS,
  PELOTA_MAX_Y16 = (PONG_ALTO - PONG_PELOTA_LADO) << PONG_FRAC_BITS,
  PELOTA_LADO16 = PONG_PELOTA_LADO << PONG_FRAC_BITS,
  /* Posicion de la pelota (esquina izq.) en la que toca cada paleta. */
  PLANO_IZQ16 = (PONG_PALETA_IZQ_X + PONG_PALETA_ANCHO) << PONG_FRAC_BITS,
  PLANO_DER16 = (PONG_PALETA_DER_X << PONG_FRAC_BITS) - PELOTA_LADO16
};

static const int16_t VX_NIVEL[PONG_NIVELES_VEL] = {16, 20, 26, 32};

/* vy segun la zona de la paleta que golpea la pelota (0 = borde superior). */
static const int16_t VY_ZONA[PONG_NIVELES_VEL][5] = {
    {-20, -10, 0, 10, 20},
    {-24, -12, 0, 12, 24},
    {-30, -15, 0, 15, 30},
    {-36, -18, 0, 18, 36}};

/* Zona (0..4) segun donde cae el centro de la pelota respecto a la paleta. */
static const uint8_t ZONA_ALTO10[11] = {0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4};
static const uint8_t ZONA_ALTO9[10] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4};

static const int16_t SAQUE_VY[4] = {-8, -4, 4, 8};

/* Pequena variacion de vy en cada golpe lateral: sin ella dos paletas pueden
 * repetir el mismo recorrido para siempre. El centro de la paleta no varia. */
static const int16_t VY_VARIACION[4] = {-2, -1, 1, 2};

/* IA: mascaras de cuadros en que mira la pelota y en que mueve la paleta. */
static const uint8_t IA_MASCARA_MIRA[3] = {3, 1, 0};
static const uint8_t IA_MASCARA_MUEVE[3] = {1, 0, 0};
static const int8_t IA_PASO[3] = {1, 1, 2};
enum { IA_GOLPES_CANSANCIO = 13 };
static const int8_t IA_ERROR[3][16] = {
    {-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7},
    {-6, -5, -4, -3, -3, -2, -1, 0, 0, 1, 2, 3, 3, 4, 5, 6},
    {-2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2}};

static uint32_t pong_rand(EstadoPong *e) {
  uint32_t x = e->prng_estado;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  e->prng_estado = x;
  return x >> 8;
}

static int16_t limitar_paleta(int16_t y, int16_t alto) {
  if (y < 0)
    return 0;
  if (y > PONG_ALTO - alto)
    return (int16_t)(PONG_ALTO - alto);
  return y;
}

static int16_t mover_paleta(int16_t y, AccionPong a, int16_t alto) {
  if (a == PONG_ACCION_ARRIBA)
    y = (int16_t)(y - PONG_PALETA_VELOCIDAD);
  else if (a == PONG_ACCION_ABAJO)
    y = (int16_t)(y + PONG_PALETA_VELOCIDAD);
  return limitar_paleta(y, alto);
}

uint8_t pong_nivel_velocidad(const EstadoPong *e) {
  uint8_t n = 0;
  if (e->golpes >= PONG_GOLPES_POR_NIVEL)
    n = 1;
  if (e->golpes >= 2 * PONG_GOLPES_POR_NIVEL)
    n = 2;
  if (e->golpes >= 3 * PONG_GOLPES_POR_NIVEL)
    n = 3;
  return n;
}

int16_t pong_pelota_x(const EstadoPong *e) {
  return (int16_t)(e->pelota_x16 >> PONG_FRAC_BITS);
}

int16_t pong_pelota_y(const EstadoPong *e) {
  return (int16_t)(e->pelota_y16 >> PONG_FRAC_BITS);
}

/* Elige el desvio con que la IA apuntara a la proxima pelota. Si el
 * intercambio se alarga, el desvio crece: ningun rally dura para siempre. */
static void ia_nueva_jugada(EstadoPong *e) {
  int16_t error = IA_ERROR[e->nivel_ia][pong_rand(e) & 15];
  if (e->golpes > IA_GOLPES_CANSANCIO) {
    int16_t extra = (int16_t)(e->golpes - IA_GOLPES_CANSANCIO);
    error = (int16_t)(error >= 0 ? error + extra : error - extra);
  }
  e->ia_error = (int8_t)error;
}

/* Pone la pelota en el centro; sale hacia la izquierda si `hacia_izq`. */
static void sacar(EstadoPong *e, uint8_t hacia_izq) {
  int16_t y = (int16_t)(16 + (pong_rand(e) & 31));
  e->pelota_x16 =
      (int16_t)(((PONG_ANCHO - PONG_PELOTA_LADO) / 2) << PONG_FRAC_BITS);
  e->pelota_y16 = (int16_t)(y << PONG_FRAC_BITS);
  e->pelota_vx = hacia_izq ? (int16_t)-VX_NIVEL[0] : VX_NIVEL[0];
  e->pelota_vy = SAQUE_VY[pong_rand(e) & 3];
  e->golpes = 0;
  e->espera_saque = PONG_ESPERA_SAQUE;
  ia_nueva_jugada(e);
}

void pong_inicializar(EstadoPong *e, uint32_t semilla, ModoPong modo,
                      NivelIA nivel) {
  uint8_t *bytes = (uint8_t *)e; /* deja tambien el relleno en cero */
  for (unsigned i = 0; i < sizeof(*e); i++)
    bytes[i] = 0;
  e->paleta_izq_y = (PONG_ALTO - PONG_PALETA_IZQ_ALTO) / 2;
  e->paleta_der_y = (PONG_ALTO - PONG_PALETA_DER_ALTO) / 2;
  e->puntos_izq = 0;
  e->puntos_der = 0;
  e->juego_terminado = 0;
  e->ganador = 0;
  e->modo = (uint8_t)modo;
  e->nivel_ia = (uint8_t)nivel;
  e->eventos = 0;
  e->ia_tick = 0;
  e->ia_objetivo = e->paleta_der_y;
  e->prng_estado = semilla ? semilla : 0x9E3779B9u;
  sacar(e, (uint8_t)(pong_rand(e) & 1));
}

/* La IA mueve la paleta derecha hacia donde cree que llegara la pelota. */
static void ia_mover(EstadoPong *e) {
  const uint8_t n = e->nivel_ia;
  const int16_t alto = PONG_PALETA_DER_ALTO;
  e->ia_tick++;
  if ((e->ia_tick & IA_MASCARA_MIRA[n]) == 0) {
    if (e->pelota_vx > 0) {
      int16_t centro =
          (int16_t)((e->pelota_y16 + PELOTA_LADO16 / 2) >> PONG_FRAC_BITS);
      e->ia_objetivo = (int16_t)(centro - alto / 2 + e->ia_error);
    } else {
      e->ia_objetivo = (int16_t)((PONG_ALTO - alto) / 2);
    }
  }
  if (e->ia_tick & IA_MASCARA_MUEVE[n])
    return;
  int16_t d = (int16_t)(e->ia_objetivo - e->paleta_der_y);
  if (d > IA_PASO[n])
    d = IA_PASO[n];
  else if (d < -IA_PASO[n])
    d = (int16_t)-IA_PASO[n];
  e->paleta_der_y = limitar_paleta((int16_t)(e->paleta_der_y + d), alto);
}

/* Anota un punto el lado izquierdo (anota_izq = 1) o el derecho (0). */
static void punto(EstadoPong *e, uint8_t anota_izq) {
  e->eventos |= PONG_EVT_PUNTO;
  if (anota_izq)
    e->puntos_izq++;
  else
    e->puntos_der++;
  if (e->puntos_izq >= PONG_PUNTOS_PARA_GANAR ||
      e->puntos_der >= PONG_PUNTOS_PARA_GANAR) {
    e->juego_terminado = 1;
    e->ganador = anota_izq ? 1 : 2;
    e->eventos |= PONG_EVT_FIN;
    e->pelota_vx = 0;
    e->pelota_vy = 0;
    return;
  }
  /* sale hacia quien perdio el punto */
  sacar(e, (uint8_t)!anota_izq);
}

static uint8_t zona_golpe(int16_t pelota_y, int16_t paleta_y, int16_t alto) {
  int16_t pos = (int16_t)(pelota_y + PONG_PELOTA_LADO / 2 - paleta_y);
  return alto == PONG_PALETA_IZQ_ALTO ? ZONA_ALTO10[pos] : ZONA_ALTO9[pos];
}

static int hay_solape(int16_t pelota_y, int16_t paleta_y, int16_t alto) {
  return pelota_y + PONG_PELOTA_LADO > paleta_y && pelota_y < paleta_y + alto;
}

static int16_t angulo_golpe(EstadoPong *e, uint8_t nivel, uint8_t zona) {
  int16_t vy = VY_ZONA[nivel][zona];
  if (zona != 2)
    vy = (int16_t)(vy + VY_VARIACION[pong_rand(e) & 3]);
  return vy;
}

static void mover_pelota(EstadoPong *e) {
  int16_t nx = (int16_t)(e->pelota_x16 + e->pelota_vx);
  int16_t ny = (int16_t)(e->pelota_y16 + e->pelota_vy);

  if (ny < 0) {
    ny = (int16_t)-ny;
    e->pelota_vy = (int16_t)-e->pelota_vy;
    e->eventos |= PONG_EVT_PARED;
  } else if (ny > PELOTA_MAX_Y16) {
    ny = (int16_t)(PELOTA_MAX_Y16 + PELOTA_MAX_Y16 - ny);
    e->pelota_vy = (int16_t)-e->pelota_vy;
    e->eventos |= PONG_EVT_PARED;
  }
  const int16_t py = (int16_t)(ny >> PONG_FRAC_BITS);

  if (e->pelota_vx < 0 && e->pelota_x16 >= PLANO_IZQ16 && nx < PLANO_IZQ16 &&
      hay_solape(py, e->paleta_izq_y, PONG_PALETA_IZQ_ALTO)) {
    uint8_t zona = zona_golpe(py, e->paleta_izq_y, PONG_PALETA_IZQ_ALTO);
    if (e->golpes < 255)
      e->golpes++;
    uint8_t n = pong_nivel_velocidad(e);
    nx = (int16_t)(PLANO_IZQ16 + PLANO_IZQ16 - nx);
    e->pelota_vx = VX_NIVEL[n];
    e->pelota_vy = angulo_golpe(e, n, zona);
    e->eventos |= PONG_EVT_PALETA;
    ia_nueva_jugada(e);
  } else if (e->pelota_vx > 0 && e->pelota_x16 <= PLANO_DER16 &&
             nx > PLANO_DER16 &&
             hay_solape(py, e->paleta_der_y, PONG_PALETA_DER_ALTO)) {
    uint8_t zona = zona_golpe(py, e->paleta_der_y, PONG_PALETA_DER_ALTO);
    if (e->golpes < 255)
      e->golpes++;
    uint8_t n = pong_nivel_velocidad(e);
    nx = (int16_t)(PLANO_DER16 + PLANO_DER16 - nx);
    e->pelota_vx = (int16_t)-VX_NIVEL[n];
    e->pelota_vy = angulo_golpe(e, n, zona);
    e->eventos |= PONG_EVT_PALETA;
  }

  e->pelota_x16 = nx;
  e->pelota_y16 = ny;

  if (nx <= 0) {
    e->pelota_x16 = 0;
    punto(e, 0);
  } else if (nx >= PELOTA_MAX_X16) {
    e->pelota_x16 = PELOTA_MAX_X16;
    punto(e, 1);
  }
}

void pong_actualizar(EstadoPong *e, AccionPong izq, AccionPong der) {
  e->eventos = 0;
  if (e->juego_terminado)
    return;
  e->paleta_izq_y = mover_paleta(e->paleta_izq_y, izq, PONG_PALETA_IZQ_ALTO);
  if (e->modo == PONG_DOS_JUGADORES)
    e->paleta_der_y = mover_paleta(e->paleta_der_y, der, PONG_PALETA_DER_ALTO);
  else
    ia_mover(e);
  if (e->espera_saque) {
    e->espera_saque--;
    return;
  }
  mover_pelota(e);
}

int pong_puntaje(const EstadoPong *e) {
  return (e->puntos_izq - e->puntos_der + PONG_PUNTOS_PARA_GANAR) *
         (e->nivel_ia + 1);
}

int pong_registrar_puntaje(int top[PONG_TOP_N], int puntaje) {
  if (puntaje <= top[PONG_TOP_N - 1])
    return 0;
  int i = PONG_TOP_N - 1;
  while (i > 0 && top[i - 1] < puntaje) {
    top[i] = top[i - 1];
    --i;
  }
  top[i] = puntaje;
  return 1;
}
