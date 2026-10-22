/* Pruebas automaticas de la logica de Pong (sin SDL).
 * Compilar y correr (desde la carpeta pong/):
 *   gcc -Wall -Wextra -Wconversion -Iinclude -o pong_tests \
 *       tests/pong_tests.c src/pong_logica.c && ./pong_tests
 * Con -v imprime tambien las pruebas que pasan. Sale con codigo 1 si falla alguna. */
#include "assets.h"
#include "jugadores_bot.h"
#include "pong_logica.h"
#include <stdio.h>
#include <string.h>

static int total, fallos, verbose;

#define CHECK(cond, nombre)                                                    \
  do {                                                                         \
    total++;                                                                   \
    if (!(cond)) {                                                             \
      fallos++;                                                                \
      printf("FALLA  %s (linea %d)\n", nombre, __LINE__);                      \
    } else if (verbose) {                                                      \
      printf("ok     %s\n", nombre);                                           \
    }                                                                          \
  } while (0)

typedef AccionPong (*Bot)(const EstadoPong *);
static AccionPong bot_pasivo(const EstadoPong *e) {
  (void)e;
  return PONG_ACCION_NINGUNA;
}

static EstadoPong nuevo(ModoPong m, NivelIA n) {
  EstadoPong e;
  pong_inicializar(&e, 12345, m, n);
  return e;
}
/* Estado de dos jugadores sin espera de saque: la IA no interviene. */
static EstadoPong cancha(void) {
  EstadoPong e = nuevo(PONG_DOS_JUGADORES, PONG_IA_NORMAL);
  e.espera_saque = 0;
  return e;
}
static void poner(EstadoPong *e, int x16, int y_px, int vx, int vy) {
  e->pelota_x16 = (int16_t)x16;
  e->pelota_y16 = (int16_t)(y_px << PONG_FRAC_BITS);
  e->pelota_vx = (int16_t)vx;
  e->pelota_vy = (int16_t)vy;
}
static void paso(EstadoPong *e) {
  pong_actualizar(e, PONG_ACCION_NINGUNA, PONG_ACCION_NINGUNA);
}
static int abs_i(int v) { return v < 0 ? -v : v; }
/* vy de un golpe lateral = valor de la zona +-2 (variacion aleatoria) */
static int cerca(int v, int esperado) { return abs_i(v - esperado) <= 2; }

static uint32_t azar_estado = 2463534242u;
static uint32_t azar(void) {
  azar_estado ^= azar_estado << 13;
  azar_estado ^= azar_estado >> 17;
  azar_estado ^= azar_estado << 5;
  return azar_estado >> 8;
}

/* Intenta un golpe en la paleta izquierda (por defecto y=30, alto 10) con la
 * pelota arriba en `by` (px); devuelve el estado despues del paso. */
static EstadoPong golpe_izq(int by, int golpes_previos) {
  EstadoPong e = cancha();
  e.paleta_izq_y = 30;
  e.golpes = (uint8_t)golpes_previos;
  poner(&e, 70, by, -16, 0);
  paso(&e);
  return e;
}
static EstadoPong golpe_der(int by, int golpes_previos) {
  EstadoPong e = cancha();
  e.paleta_der_y = 30;
  e.golpes = (uint8_t)golpes_previos;
  poner(&e, 920, by, 16, 0);
  paso(&e);
  return e;
}

static void pruebas_estado_inicial(void) {
  EstadoPong e = nuevo(PONG_UN_JUGADOR, PONG_IA_NORMAL);
  CHECK(e.paleta_izq_y == 27, "inicio: paleta izquierda centrada");
  CHECK(e.paleta_der_y == 27, "inicio: paleta derecha centrada");
  CHECK(e.puntos_izq == 0 && e.puntos_der == 0, "inicio: marcador 0-0");
  CHECK(!e.juego_terminado && e.ganador == 0, "inicio: partida en curso");
  CHECK(e.espera_saque == PONG_ESPERA_SAQUE, "inicio: espera de saque");
  CHECK(pong_pelota_x(&e) == 31, "inicio: pelota en el centro horizontal");
  CHECK(pong_pelota_y(&e) >= 16 && pong_pelota_y(&e) <= 47,
        "inicio: pelota lejos de las paredes");
  CHECK(abs_i(e.pelota_vx) == 16, "inicio: velocidad horizontal base");
  CHECK(e.pelota_vy != 0 && abs_i(e.pelota_vy) <= 8,
        "inicio: saque con angulo suave");
  CHECK(e.modo == PONG_UN_JUGADOR && e.nivel_ia == PONG_IA_NORMAL,
        "inicio: guarda modo y dificultad");
  CHECK(e.golpes == 0 && pong_nivel_velocidad(&e) == 0,
        "inicio: nivel de velocidad 0");

  EstadoPong z;
  pong_inicializar(&z, 0, PONG_UN_JUGADOR, PONG_IA_FACIL);
  CHECK(z.prng_estado != 0, "semilla 0 no deja el generador en cero");
  for (int i = 0; i < 100; i++)
    paso(&z);
  CHECK(z.puntos_izq + z.puntos_der + z.espera_saque >= 0,
        "semilla 0: la partida avanza");

  EstadoPong a, b;
  pong_inicializar(&a, 777, PONG_UN_JUGADOR, PONG_IA_NORMAL);
  pong_inicializar(&b, 777, PONG_UN_JUGADOR, PONG_IA_NORMAL);
  for (int i = 0; i < 500; i++) {
    pong_actualizar(&a, (i & 8) ? PONG_ACCION_ARRIBA : PONG_ACCION_ABAJO,
                    PONG_ACCION_NINGUNA);
    pong_actualizar(&b, (i & 8) ? PONG_ACCION_ARRIBA : PONG_ACCION_ABAJO,
                    PONG_ACCION_NINGUNA);
  }
  CHECK(memcmp(&a, &b, sizeof a) == 0, "misma semilla, misma partida");

  int izq = 0, der = 0, vy_distintos = 0, vistos[4] = {0, 0, 0, 0};
  for (uint32_t s = 1; s <= 60; s++) {
    EstadoPong t;
    pong_inicializar(&t, s, PONG_UN_JUGADOR, PONG_IA_NORMAL);
    if (t.pelota_vx < 0)
      izq++;
    else
      der++;
    int idx = t.pelota_vy == -8 ? 0 : t.pelota_vy == -4 ? 1
              : t.pelota_vy == 4 ? 2 : t.pelota_vy == 8 ? 3 : -1;
    if (idx >= 0)
      vistos[idx] = 1;
  }
  for (int i = 0; i < 4; i++)
    vy_distintos += vistos[i];
  CHECK(izq > 5 && der > 5, "el primer saque sale hacia ambos lados");
  CHECK(vy_distintos == 4, "los cuatro angulos de saque aparecen");
}

static void pruebas_paletas(void) {
  EstadoPong e = cancha();
  int y0 = e.paleta_izq_y;
  pong_actualizar(&e, PONG_ACCION_ARRIBA, PONG_ACCION_NINGUNA);
  CHECK(e.paleta_izq_y == y0 - PONG_PALETA_VELOCIDAD, "arriba mueve 2 px");
  pong_actualizar(&e, PONG_ACCION_ABAJO, PONG_ACCION_NINGUNA);
  CHECK(e.paleta_izq_y == y0, "abajo mueve 2 px");
  pong_actualizar(&e, PONG_ACCION_NINGUNA, PONG_ACCION_NINGUNA);
  CHECK(e.paleta_izq_y == y0, "sin accion no se mueve");

  for (int i = 0; i < 40; i++)
    pong_actualizar(&e, PONG_ACCION_ARRIBA, PONG_ACCION_ARRIBA);
  CHECK(e.paleta_izq_y == 0, "limite superior izquierda");
  CHECK(e.paleta_der_y == 0, "limite superior derecha (2 jugadores)");
  for (int i = 0; i < 60; i++)
    pong_actualizar(&e, PONG_ACCION_ABAJO, PONG_ACCION_ABAJO);
  CHECK(e.paleta_izq_y == PONG_ALTO - PONG_PALETA_IZQ_ALTO,
        "limite inferior izquierda usa el alto real (10)");
  CHECK(e.paleta_der_y == PONG_ALTO - PONG_PALETA_DER_ALTO,
        "limite inferior derecha usa el alto real (9)");

  EstadoPong u = nuevo(PONG_UN_JUGADOR, PONG_IA_NORMAL);
  EstadoPong v = nuevo(PONG_UN_JUGADOR, PONG_IA_NORMAL);
  for (int i = 0; i < 80; i++) {
    pong_actualizar(&u, PONG_ACCION_NINGUNA, PONG_ACCION_ARRIBA);
    pong_actualizar(&v, PONG_ACCION_NINGUNA, PONG_ACCION_NINGUNA);
  }
  CHECK(memcmp(&u, &v, sizeof u) == 0,
        "1 jugador: la accion del jugador 2 se ignora");

  EstadoPong d = nuevo(PONG_DOS_JUGADORES, PONG_IA_DIFICIL);
  int y = d.paleta_der_y;
  for (int i = 0; i < 50; i++)
    paso(&d);
  CHECK(d.paleta_der_y == y, "2 jugadores: la IA no mueve la paleta");
  pong_actualizar(&d, PONG_ACCION_NINGUNA, PONG_ACCION_ABAJO);
  CHECK(d.paleta_der_y == y + PONG_PALETA_VELOCIDAD,
        "2 jugadores: la paleta derecha obedece al jugador 2");
}

static void pruebas_saque_y_movimiento(void) {
  EstadoPong e = nuevo(PONG_DOS_JUGADORES, PONG_IA_NORMAL);
  int x0 = e.pelota_x16, y0 = e.pelota_y16;
  for (int i = 0; i < PONG_ESPERA_SAQUE; i++)
    paso(&e);
  CHECK(e.pelota_x16 == x0 && e.pelota_y16 == y0,
        "la pelota espera quieta durante el saque");
  CHECK(e.espera_saque == 0, "la espera termina a los 20 cuadros");
  paso(&e);
  CHECK(e.pelota_x16 != x0, "despues de la espera la pelota se mueve");

  EstadoPong p = nuevo(PONG_DOS_JUGADORES, PONG_IA_NORMAL);
  int py = p.paleta_izq_y;
  pong_actualizar(&p, PONG_ACCION_ABAJO, PONG_ACCION_NINGUNA);
  CHECK(p.paleta_izq_y == py + 2, "las paletas se mueven durante la espera");

  EstadoPong r = cancha();
  poner(&r, 30 * 16, 30, 16, 0);
  paso(&r);
  CHECK(r.pelota_x16 == 31 * 16 && r.pelota_y16 == 30 * 16,
        "avance recto: 1 px por cuadro a velocidad 16");
  poner(&r, 30 * 16, 30, 24, 8);
  paso(&r);
  CHECK(r.pelota_x16 == 30 * 16 + 24 && r.pelota_y16 == 30 * 16 + 8,
        "avance en diagonal con subpixeles");
}

static void pruebas_paredes(void) {
  EstadoPong e = cancha();
  poner(&e, 30 * 16, 0, 16, -16);
  paso(&e);
  CHECK(e.pelota_vy == 16, "rebote en el techo invierte vy");
  CHECK(e.pelota_y16 == 16, "rebote en el techo refleja la posicion");
  CHECK(e.eventos & PONG_EVT_PARED, "rebote en el techo avisa evento pared");

  poner(&e, 30 * 16, 62, 16, 16);
  paso(&e);
  CHECK(e.pelota_vy == -16, "rebote en el piso invierte vy");
  CHECK(e.pelota_y16 == 62 * 16 - 16, "rebote en el piso refleja la posicion");
  CHECK(e.eventos & PONG_EVT_PARED, "rebote en el piso avisa evento pared");

  poner(&e, 30 * 16, 30, 16, 16);
  paso(&e);
  CHECK(!(e.eventos & PONG_EVT_PARED), "sin pared no hay evento pared");
  CHECK(e.pelota_vx == 16, "el rebote en pared no cambia vx");
}

static void pruebas_golpe_paleta_izquierda(void) {
  EstadoPong e = golpe_izq(34, 0);
  CHECK(e.pelota_vx > 0, "golpe izquierdo: la pelota regresa a la derecha");
  CHECK(e.eventos & PONG_EVT_PALETA, "golpe izquierdo: evento de paleta");
  CHECK(e.golpes == 1, "golpe izquierdo: cuenta un golpe");
  CHECK(e.pelota_x16 >= 64, "golpe izquierdo: la pelota sale de la paleta");
  CHECK(e.puntos_izq + e.puntos_der == 0, "golpe izquierdo: sin puntos");

  /* zonas: pos = by + 1 - paleta_y, paleta en y = 30 */
  CHECK(cerca(golpe_izq(29, 0).pelota_vy, -20), "zona 0 (borde superior): sube mucho");
  CHECK(cerca(golpe_izq(31, 0).pelota_vy, -10), "zona 1: sube poco");
  CHECK(golpe_izq(34, 0).pelota_vy == 0, "zona 2 (centro): recto, sin variacion");
  CHECK(cerca(golpe_izq(37, 0).pelota_vy, 10), "zona 3: baja poco");
  CHECK(cerca(golpe_izq(39, 0).pelota_vy, 20), "zona 4 (borde inferior): baja mucho");

  /* bordes exactos del solape */
  CHECK(golpe_izq(29, 0).eventos & PONG_EVT_PALETA, "borde superior toca (by=py-1)");
  CHECK(!(golpe_izq(28, 0).eventos & PONG_EVT_PALETA), "justo arriba no toca");
  CHECK(golpe_izq(39, 0).eventos & PONG_EVT_PALETA, "borde inferior toca (by=py+9)");
  CHECK(!(golpe_izq(40, 0).eventos & PONG_EVT_PALETA), "justo abajo no toca");
}

static void pruebas_golpe_paleta_derecha(void) {
  EstadoPong e = golpe_der(34, 0);
  CHECK(e.pelota_vx < 0, "golpe derecho: la pelota regresa a la izquierda");
  CHECK(e.eventos & PONG_EVT_PALETA, "golpe derecho: evento de paleta");
  CHECK(e.golpes == 1, "golpe derecho: cuenta un golpe");
  CHECK(e.pelota_x16 <= 928, "golpe derecho: la pelota sale de la paleta");

  /* alto 9: pos = by + 1 - 30 */
  CHECK(cerca(golpe_der(29, 0).pelota_vy, -20), "derecha zona 0");
  CHECK(cerca(golpe_der(32, 0).pelota_vy, -10), "derecha zona 1");
  CHECK(golpe_der(33, 0).pelota_vy == 0, "derecha zona 2 (centro), sin variacion");
  CHECK(cerca(golpe_der(35, 0).pelota_vy, 10), "derecha zona 3");
  CHECK(cerca(golpe_der(38, 0).pelota_vy, 20), "derecha zona 4");
  CHECK(golpe_der(29, 0).eventos & PONG_EVT_PALETA, "derecha: borde superior toca");
  CHECK(!(golpe_der(28, 0).eventos & PONG_EVT_PALETA), "derecha: justo arriba no toca");
  CHECK(golpe_der(38, 0).eventos & PONG_EVT_PALETA, "derecha: borde inferior toca (alto 9)");
  CHECK(!(golpe_der(39, 0).eventos & PONG_EVT_PALETA), "derecha: justo abajo no toca");
}

static void pruebas_sin_tunel_y_pelota_pasada(void) {
  int fallo_izq = 0, fallo_der = 0;
  for (int x16 = 64; x16 < 64 + 32; x16++) {
    EstadoPong e = cancha();
    e.paleta_izq_y = 30;
    poner(&e, x16, 34, -32, 0);
    paso(&e);
    if (!(e.eventos & PONG_EVT_PALETA))
      fallo_izq++;
  }
  for (int x16 = 928 - 31; x16 <= 928; x16++) {
    EstadoPong e = cancha();
    e.paleta_der_y = 30;
    poner(&e, x16, 34, 32, 0);
    paso(&e);
    if (!(e.eventos & PONG_EVT_PALETA))
      fallo_der++;
  }
  CHECK(fallo_izq == 0, "sin tunel a la velocidad maxima (izquierda)");
  CHECK(fallo_der == 0, "sin tunel a la velocidad maxima (derecha)");

  EstadoPong p = cancha();
  p.paleta_izq_y = 28;
  poner(&p, 40, 30, -16, 0); /* ya paso la paleta */
  paso(&p);
  CHECK(p.pelota_vx < 0 && !(p.eventos & PONG_EVT_PALETA),
        "pelota que ya paso la paleta izquierda no rebota");
  for (int i = 0; i < 10 && !p.puntos_der; i++)
    paso(&p);
  CHECK(p.puntos_der == 1, "pelota que paso la paleta termina en punto");

  EstadoPong q = cancha();
  q.paleta_der_y = 28;
  poner(&q, 940, 30, 16, 0);
  paso(&q);
  CHECK(q.pelota_vx > 0 && !(q.eventos & PONG_EVT_PALETA),
        "pelota que ya paso la paleta derecha no rebota");
  for (int i = 0; i < 10 && !q.puntos_izq; i++)
    paso(&q);
  CHECK(q.puntos_izq == 1, "pelota que paso la paleta derecha termina en punto");
}

static void pruebas_aceleracion(void) {
  EstadoPong e = nuevo(PONG_DOS_JUGADORES, PONG_IA_NORMAL);
  e.golpes = 0;
  CHECK(pong_nivel_velocidad(&e) == 0, "0 golpes: nivel 0");
  e.golpes = 2;
  CHECK(pong_nivel_velocidad(&e) == 0, "2 golpes: nivel 0");
  e.golpes = 3;
  CHECK(pong_nivel_velocidad(&e) == 1, "3 golpes: nivel 1");
  e.golpes = 6;
  CHECK(pong_nivel_velocidad(&e) == 2, "6 golpes: nivel 2");
  e.golpes = 9;
  CHECK(pong_nivel_velocidad(&e) == 3, "9 golpes: nivel 3");
  e.golpes = 200;
  CHECK(pong_nivel_velocidad(&e) == 3, "muchos golpes: el nivel no pasa de 3");

  CHECK(golpe_izq(34, 0).pelota_vx == 16, "primer golpe: velocidad 16");
  CHECK(golpe_izq(34, 2).pelota_vx == 20, "tercer golpe: sube a 20");
  CHECK(golpe_izq(34, 5).pelota_vx == 26, "sexto golpe: sube a 26");
  CHECK(golpe_izq(34, 8).pelota_vx == 32, "noveno golpe: sube a 32");
  CHECK(golpe_izq(34, 40).pelota_vx == 32, "la velocidad horizontal tiene tope");
  CHECK(golpe_der(34, 8).pelota_vx == -32, "derecha: tambien acelera");
  CHECK(cerca(golpe_izq(29, 8).pelota_vy, -36), "nivel 3: el borde da el angulo maximo");
  CHECK(golpe_izq(34, 255).golpes == 255, "el contador de golpes se satura en 255");

  EstadoPong p = cancha();
  p.golpes = 7;
  p.paleta_izq_y = 0;
  poner(&p, 80, 40, -16, 0);
  for (int i = 0; i < 10 && !p.puntos_der; i++)
    paso(&p);
  CHECK(p.golpes == 0 && p.puntos_der == 1,
        "al marcar un punto la velocidad vuelve al nivel 0");
  CHECK(abs_i(p.pelota_vx) == 16, "el saque despues del punto va a velocidad base");
}

static void pruebas_puntos_y_fin(void) {
  EstadoPong e = cancha();
  e.paleta_izq_y = 0;
  poner(&e, 80, 40, -16, 0);
  for (int i = 0; i < 10 && !e.puntos_der; i++)
    paso(&e);
  CHECK(e.puntos_der == 1 && e.puntos_izq == 0, "se pierde por la izquierda: punto del otro lado");
  CHECK(e.eventos & PONG_EVT_PUNTO, "el punto avisa evento");
  CHECK(e.espera_saque == PONG_ESPERA_SAQUE, "tras el punto hay espera de saque");
  CHECK(pong_pelota_x(&e) == 31, "tras el punto la pelota vuelve al centro");
  CHECK(e.pelota_vx < 0, "tras anotar la derecha el saque va hacia la izquierda (quien perdio)");

  EstadoPong f = cancha();
  f.paleta_der_y = 0;
  poner(&f, 940, 40, 16, 0);
  for (int i = 0; i < 10 && !f.puntos_izq; i++)
    paso(&f);
  CHECK(f.puntos_izq == 1 && f.puntos_der == 0, "se pierde por la derecha: punto de la izquierda");
  CHECK(f.pelota_vx > 0, "tras anotar la izquierda el saque va hacia la derecha (quien perdio)");

  EstadoPong g = cancha();
  g.puntos_izq = PONG_PUNTOS_PARA_GANAR - 2;
  g.puntos_der = PONG_PUNTOS_PARA_GANAR - 1;
  g.paleta_der_y = 0;
  poner(&g, 940, 40, 16, 0);
  for (int i = 0; i < 10 && !g.juego_terminado && g.puntos_izq < 6; i++)
    paso(&g);
  CHECK(g.puntos_izq == 6 && !g.juego_terminado, "6 puntos todavia no ganan");

  g.paleta_der_y = 0;
  g.espera_saque = 0;
  poner(&g, 940, 40, 16, 0);
  for (int i = 0; i < 10 && !g.juego_terminado; i++)
    paso(&g);
  CHECK(g.juego_terminado && g.ganador == 1 && g.puntos_izq == 7,
        "7 puntos: gana la izquierda");
  CHECK(g.eventos & PONG_EVT_FIN, "el ultimo punto avisa evento de fin");
  EstadoPong copia = g;
  for (int i = 0; i < 1000; i++)
    pong_actualizar(&g, PONG_ACCION_ARRIBA, PONG_ACCION_ABAJO);
  copia.eventos = 0;
  CHECK(memcmp(&copia, &g, sizeof g) == 0, "terminada la partida nada cambia");
  CHECK(g.puntos_izq == 7, "el marcador no pasa de 7");

  EstadoPong h = cancha();
  h.puntos_der = PONG_PUNTOS_PARA_GANAR - 1;
  h.paleta_izq_y = 0;
  poner(&h, 80, 40, -16, 0);
  for (int i = 0; i < 10 && !h.juego_terminado; i++)
    paso(&h);
  CHECK(h.juego_terminado && h.ganador == 2, "7 puntos: gana la derecha");
}

static void pruebas_ia(void) {
  /* vuelve al centro cuando la pelota se aleja */
  EstadoPong e = nuevo(PONG_UN_JUGADOR, PONG_IA_NORMAL);
  e.espera_saque = 0;
  e.paleta_der_y = 0;
  poner(&e, 40 * 16, 30, -16, 0);
  for (int i = 0; i < 30; i++)
    paso(&e);
  CHECK(e.paleta_der_y == 27, "IA normal: vuelve al centro si la pelota se aleja");

  /* persigue la pelota que viene */
  EstadoPong p = nuevo(PONG_UN_JUGADOR, PONG_IA_DIFICIL);
  p.espera_saque = 0;
  p.paleta_der_y = 27;
  poner(&p, 20 * 16, 50, 16, 0);
  for (int i = 0; i < 12; i++)
    paso(&p);
  CHECK(p.paleta_der_y > 27, "IA persigue la pelota que se acerca (abajo)");

  /* velocidades maximas de paleta por nivel */
  int maximo[3] = {0, 0, 0};
  for (int n = 0; n < 3; n++) {
    EstadoPong t = nuevo(PONG_UN_JUGADOR, (NivelIA)n);
    t.espera_saque = 0;
    t.paleta_der_y = 0;
    poner(&t, 20 * 16, 60, 16, 0);
    for (int i = 0; i < 20; i++) {
      int antes = t.paleta_der_y;
      t.pelota_x16 = 20 * 16;
      t.pelota_y16 = 60 << PONG_FRAC_BITS;
      t.pelota_vx = 16;
      t.pelota_vy = 0;
      paso(&t);
      if (abs_i(t.paleta_der_y - antes) > maximo[n])
        maximo[n] = abs_i(t.paleta_der_y - antes);
    }
  }
  CHECK(maximo[0] == 1, "IA facil: mueve 1 px a la vez");
  CHECK(maximo[1] == 1, "IA normal: mueve 1 px por cuadro");
  CHECK(maximo[2] == 2, "IA dificil: mueve hasta 2 px por cuadro");

  EstadoPong f = nuevo(PONG_UN_JUGADOR, PONG_IA_FACIL);
  f.espera_saque = 0;
  f.paleta_der_y = 0;
  int movs = 0;
  for (int i = 0; i < 20; i++) {
    int antes = f.paleta_der_y;
    poner(&f, 20 * 16, 60, 16, 0);
    paso(&f);
    if (f.paleta_der_y != antes)
      movs++;
  }
  CHECK(movs <= 10, "IA facil: se mueve solo cada 2 cuadros");

  /* la IA nunca sale del campo */
  int dentro = 1;
  for (int n = 0; n < 3; n++) {
    EstadoPong t = nuevo(PONG_UN_JUGADOR, (NivelIA)n);
    for (int i = 0; i < 20000 && !t.juego_terminado; i++) {
      pong_actualizar(&t, (i / 7) % 2 ? PONG_ACCION_ARRIBA : PONG_ACCION_ABAJO,
                      PONG_ACCION_NINGUNA);
      if (t.paleta_der_y < 0 || t.paleta_der_y > PONG_ALTO - PONG_PALETA_DER_ALTO)
        dentro = 0;
    }
  }
  CHECK(dentro, "la paleta de la IA siempre queda dentro del campo");
}

typedef struct {
  int gana_jugador, partidas, sin_terminar, golpes_max;
  long cuadros;
} Resumen;

static Resumen jugar(Bot b, NivelIA n, int partidas) {
  Resumen r = {0, partidas, 0, 0, 0};
  for (int s = 1; s <= partidas; s++) {
    EstadoPong e;
    pong_inicializar(&e, (uint32_t)s * 7919u, PONG_UN_JUGADOR, n);
    int t = 0;
    for (; t < 100000 && !e.juego_terminado; t++) {
      pong_actualizar(&e, b(&e), PONG_ACCION_NINGUNA);
      if (e.golpes > r.golpes_max)
        r.golpes_max = e.golpes;
    }
    if (!e.juego_terminado)
      r.sin_terminar++;
    if (e.ganador == 1)
      r.gana_jugador++;
    r.cuadros += t;
  }
  return r;
}

static void pruebas_jugabilidad(void) {
  Resumen pas[3], sig[3], apu[3];
  for (int n = 0; n < 3; n++) {
    pas[n] = jugar(bot_pasivo, (NivelIA)n, 200);
    sig[n] = jugar(bot_sigue, (NivelIA)n, 200);
    apu[n] = jugar(bot_apunta, (NivelIA)n, 200);
  }
  int todas_terminan = 1, golpes_ok = 1;
  for (int n = 0; n < 3; n++) {
    if (pas[n].sin_terminar || sig[n].sin_terminar || apu[n].sin_terminar)
      todas_terminan = 0;
    if (pas[n].golpes_max >= 255 || sig[n].golpes_max >= 255 ||
        apu[n].golpes_max >= 255)
      golpes_ok = 0;
  }
  CHECK(todas_terminan, "jugabilidad: toda partida termina (ningun rally infinito)");
  CHECK(golpes_ok, "jugabilidad: ningun intercambio llega a 255 golpes");

  CHECK(pas[2].gana_jugador == 0, "jugador pasivo pierde siempre contra dificil");
  CHECK(pas[1].gana_jugador < 20, "jugador pasivo casi nunca gana contra normal");
  CHECK(pas[0].gana_jugador > 0, "la IA facil puede fallar: un jugador pasivo a veces gana");
  CHECK(pas[0].gana_jugador >= pas[1].gana_jugador &&
            pas[1].gana_jugador >= pas[2].gana_jugador,
        "el jugador pasivo gana mas contra facil que contra normal y dificil");
  CHECK(apu[0].gana_jugador >= 190, "jugador que apunta vence a la IA facil");
  CHECK(apu[1].gana_jugador >= 180, "jugador que apunta vence a la IA normal");
  CHECK(apu[2].gana_jugador > 0, "la IA dificil sigue siendo vencible");
  CHECK(apu[2].gana_jugador < 200 || sig[2].gana_jugador < 200,
        "la IA dificil pierde menos que la facil frente a quien sigue la pelota");
  CHECK(sig[2].gana_jugador < sig[0].gana_jugador,
        "seguir la pelota gana menos contra dificil que contra facil");
  CHECK(sig[2].cuadros > sig[0].cuadros,
        "contra dificil las partidas son mas largas que contra facil");

  /* la IA tambien pierde puntos: contabilidad de ambos lados */
  EstadoPong e;
  pong_inicializar(&e, 99, PONG_UN_JUGADOR, PONG_IA_NORMAL);
  int puntos_ia_vistos = 0;
  for (int t = 0; t < 100000 && !e.juego_terminado; t++) {
    pong_actualizar(&e, bot_pasivo(&e), PONG_ACCION_NINGUNA);
    if (e.eventos & PONG_EVT_PUNTO)
      puntos_ia_vistos++;
  }
  CHECK(puntos_ia_vistos == e.puntos_izq + e.puntos_der,
        "cada punto del marcador viene con un evento");
}

static void pruebas_dos_jugadores(void) {
  /* un jugador apunta, el otro no se mueve: gana el que se mueve */
  EstadoPong e = nuevo(PONG_DOS_JUGADORES, PONG_IA_NORMAL);
  for (int t = 0; t < 100000 && !e.juego_terminado; t++)
    pong_actualizar(&e, bot_apunta(&e), PONG_ACCION_NINGUNA);
  CHECK(e.juego_terminado && e.ganador == 1,
        "2 jugadores: gana quien mueve su paleta contra el que no");
  CHECK(e.puntos_izq == 7 && e.puntos_der < 7, "2 jugadores: marcador 7 a n");
}

typedef struct {
  int viola, incoherente, prev;
} FuzzCuenta;

/* Avanza un cuadro y revisa todas las invariantes. */
static void paso_fuzz(EstadoPong *e, AccionPong a, AccionPong b,
                      FuzzCuenta *c) {
  pong_actualizar(e, a, b);
  if (e->paleta_izq_y < 0 || e->paleta_izq_y > PONG_ALTO - PONG_PALETA_IZQ_ALTO ||
      e->paleta_der_y < 0 || e->paleta_der_y > PONG_ALTO - PONG_PALETA_DER_ALTO)
    c->viola++;
  if (e->pelota_x16 < 0 || e->pelota_x16 > (PONG_ANCHO - 2) * 16 ||
      e->pelota_y16 < 0 || e->pelota_y16 > (PONG_ALTO - 2) * 16)
    c->viola++;
  if (abs_i(e->pelota_vx) > 32 || abs_i(e->pelota_vy) > 38)
    c->viola++;
  if (e->puntos_izq > 7 || e->puntos_der > 7)
    c->viola++;
  int suma = e->puntos_izq + e->puntos_der;
  if (suma < c->prev || suma > c->prev + 1)
    c->incoherente++;
  if (((e->eventos & PONG_EVT_PUNTO) != 0) != (suma == c->prev + 1))
    c->incoherente++;
  if ((e->ganador != 0) != (e->juego_terminado != 0))
    c->incoherente++;
  c->prev = suma;
}

static void pruebas_fuzz(void) {
  FuzzCuenta c = {0, 0, 0};
  int no_termina = 0;
  long cuadros = 0;
  for (int g = 0; g < 3000; g++) {
    EstadoPong e;
    pong_inicializar(&e, azar() + 1u, (ModoPong)(azar() & 1),
                     (NivelIA)(azar() % 3u));
    c.prev = 0;
    int t = 0;
    for (; t < 200000 && !e.juego_terminado; t++) {
      AccionPong a = (AccionPong)(azar() % 3u), b = (AccionPong)(azar() % 3u);
      if ((azar() & 31) == 0) { /* rachas que siguen la pelota: hay golpes */
        for (int k = 0; k < 20 && !e.juego_terminado; k++)
          paso_fuzz(&e, bot_sigue(&e), b, &c);
      }
      paso_fuzz(&e, a, b, &c);
    }
    cuadros += t;
    if (!e.juego_terminado)
      no_termina++;
    else if (e.puntos_izq != 7 && e.puntos_der != 7)
      c.incoherente++;
  }
  CHECK(c.viola == 0, "fuzz: posiciones, velocidades y marcador siempre en rango");
  CHECK(c.incoherente == 0, "fuzz: eventos y marcador coherentes en todo momento");
  CHECK(no_termina == 0, "fuzz: 3000 partidas al azar terminan");
  CHECK(cuadros > 3000, "fuzz: se jugaron cuadros de verdad");
}

static void pruebas_puntajes(void) {
  EstadoPong e = nuevo(PONG_UN_JUGADOR, PONG_IA_DIFICIL);
  e.puntos_izq = 7;
  e.puntos_der = 0;
  CHECK(pong_puntaje(&e) == 42, "puntaje maximo: 7-0 en dificil = 42");
  e.puntos_izq = 0;
  e.puntos_der = 7;
  CHECK(pong_puntaje(&e) == 0, "puntaje minimo: perder 0-7 = 0");
  e.nivel_ia = PONG_IA_FACIL;
  e.puntos_izq = 7;
  e.puntos_der = 3;
  CHECK(pong_puntaje(&e) == 11, "puntaje facil 7-3 = 11");
  e.nivel_ia = PONG_IA_NORMAL;
  CHECK(pong_puntaje(&e) == 22, "mismo marcador en normal vale el doble");

  int top[PONG_TOP_N] = {0, 0, 0, 0, 0};
  CHECK(!pong_registrar_puntaje(top, 0), "un puntaje 0 no entra a la tabla");
  CHECK(pong_registrar_puntaje(top, 10) && top[0] == 10, "el primero entra");
  pong_registrar_puntaje(top, 30);
  pong_registrar_puntaje(top, 20);
  pong_registrar_puntaje(top, 5);
  pong_registrar_puntaje(top, 40);
  CHECK(top[0] == 40 && top[1] == 30 && top[2] == 20 && top[3] == 10 && top[4] == 5,
        "la tabla queda ordenada de mayor a menor");
  CHECK(!pong_registrar_puntaje(top, 5), "empatar con el ultimo no entra");
  CHECK(pong_registrar_puntaje(top, 25) && top[2] == 25 && top[3] == 20 &&
            top[4] == 10,
        "uno nuevo se inserta en su lugar y sale el peor");
  CHECK(pong_registrar_puntaje(top, 99) && top[0] == 99 && top[4] == 20,
        "un nuevo record queda primero");
}

static void pruebas_assets(void) {
  CHECK(Player1_H == PONG_PALETA_IZQ_ALTO && Player1_W == PONG_PALETA_ANCHO,
        "assets: Player1 mide lo mismo que la paleta izquierda de la logica");
  CHECK(Player2_H == PONG_PALETA_DER_ALTO && Player2_W == PONG_PALETA_ANCHO,
        "assets: Player2 mide lo mismo que la paleta derecha de la logica");
  CHECK(Ball_W == PONG_PELOTA_LADO && Ball_H == PONG_PELOTA_LADO,
        "assets: la pelota mide lo mismo que en la logica");
  CHECK(Escenario_W == PONG_ANCHO && Escenario_H == PONG_ALTO,
        "assets: el escenario cubre los 64x64");
  CHECK(Num0_H == 9 && Num9_H == 9 && Num1_W == 1 && Num0_W == 6,
        "assets: digitos de 9 de alto (el 1 mide 1 de ancho)");
  CHECK(sizeof(Escenario) / sizeof(Escenario[0]) == 4096,
        "assets: el escenario tiene 4096 pixeles");
}

int main(int argc, char **argv) {
  verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
  pruebas_estado_inicial();
  pruebas_paletas();
  pruebas_saque_y_movimiento();
  pruebas_paredes();
  pruebas_golpe_paleta_izquierda();
  pruebas_golpe_paleta_derecha();
  pruebas_sin_tunel_y_pelota_pasada();
  pruebas_aceleracion();
  pruebas_puntos_y_fin();
  pruebas_ia();
  pruebas_jugabilidad();
  pruebas_dos_jugadores();
  pruebas_fuzz();
  pruebas_puntajes();
  pruebas_assets();
  printf("\n%d pruebas, %d correctas, %d fallidas\n", total, total - fallos,
         fallos);
  return fallos ? 1 : 0;
}
