/* Pruebas automaticas de la logica de Tetris (sin SDL).
 * Compilar y correr (desde la carpeta Tetris/):
 *   gcc -Wall -Wextra -Wconversion -Iinclude -o tetris_tests \
 *       tests/tetris_tests.c src/tetris_logica.c && ./tetris_tests
 * Con -v imprime tambien las pruebas que pasan. Sale con codigo 1 si falla alguna. */
#include "assets.h"
#include "jugadores_bot.h"
#include "tetris_logica.h"
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

static EstadoTetris nuevo(void) {
  EstadoTetris e;
  tetris_inicializar(&e, 12345);
  return e;
}
static EstadoTetris con_pieza(uint8_t tipo) {
  EstadoTetris e = nuevo();
  e.tipo = tipo;
  e.rot = 0;
  e.px = 3;
  e.py = TETRIS_APARICION_Y;
  e.py_max = TETRIS_APARICION_Y;
  return e;
}
static EntradaTetris nada(void) {
  EntradaTetris i = {0, 0, 0, 0, 0, 0};
  return i;
}
static void paso(EstadoTetris *e) { tetris_actualizar(e, nada()); }
static void con(EstadoTetris *e, EntradaTetris in) { tetris_actualizar(e, in); }
static int ocupadas(const EstadoTetris *e) {
  int n = 0;
  for (int y = 0; y < TETRIS_FILAS_TOTAL; y++)
    for (int x = 0; x < TETRIS_COLUMNAS; x++)
      if (e->tablero[y][x])
        n++;
  return n;
}
/* Llena una fila del tablero (0..21) menos la columna `hueco` (-1 = ninguna). */
static void fila_llena(EstadoTetris *e, int fila, int hueco) {
  for (int x = 0; x < TETRIS_COLUMNAS; x++)
    e->tablero[fila][x] = (uint8_t)(x == hueco ? 0 : 1);
}
static int forma_x(uint8_t t, uint8_t r, int i) { return tetris_forma(t, r)[2 * i]; }
static int forma_y(uint8_t t, uint8_t r, int i) {
  return tetris_forma(t, r)[2 * i + 1];
}
static int min_x(const EstadoTetris *e) {
  int m = 99;
  for (int i = 0; i < 4; i++)
    if (e->px + forma_x(e->tipo, e->rot, i) < m)
      m = e->px + forma_x(e->tipo, e->rot, i);
  return m;
}
static int max_x(const EstadoTetris *e) {
  int m = -99;
  for (int i = 0; i < 4; i++)
    if (e->px + forma_x(e->tipo, e->rot, i) > m)
      m = e->px + forma_x(e->tipo, e->rot, i);
  return m;
}

static uint32_t azar_estado = 2463534242u;
static uint32_t azar(void) {
  azar_estado ^= azar_estado << 13;
  azar_estado ^= azar_estado >> 17;
  azar_estado ^= azar_estado << 5;
  return azar_estado >> 8;
}

/* ---------------------------------------------------------------- formas */
static int mismo_conjunto(uint8_t t, uint8_t r1, uint8_t r2, int modo) {
  /* modo 3: giro dentro de una caja de 3x3; modo 4: dentro de 4x4 */
  int n = modo;
  for (int i = 0; i < 4; i++) {
    int x = forma_x(t, r1, i), y = forma_y(t, r1, i);
    int gx = n - 1 - y, gy = x; /* giro a la derecha */
    int esta = 0;
    for (int j = 0; j < 4; j++)
      if (forma_x(t, r2, j) == gx && forma_y(t, r2, j) == gy)
        esta = 1;
    if (!esta)
      return 0;
  }
  return 1;
}

static void pruebas_formas(void) {
  int validas = 1;
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++)
    for (uint8_t r = 0; r < 4; r++) {
      for (int i = 0; i < 4; i++) {
        int x = forma_x(t, r, i), y = forma_y(t, r, i);
        if (x < 0 || x > 3 || y < 0 || y > 3)
          validas = 0;
        for (int j = i + 1; j < 4; j++)
          if (x == forma_x(t, r, j) && y == forma_y(t, r, j))
            validas = 0;
      }
    }
  CHECK(validas, "formas: 4 celdas distintas dentro de la caja de 4x4");
  CHECK(TETRIS_I == 0 && TETRIS_O == 1 && TETRIS_T == 2 && TETRIS_S == 3 &&
            TETRIS_Z == 4 && TETRIS_J == 5 && TETRIS_L == 6,
        "formas: numeracion de las piezas");

  const char *nombres[7] = {"I", "O", "T", "S", "Z", "J", "L"};
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++) {
    if (t == TETRIS_O)
      continue;
    int ok = 1;
    for (uint8_t r = 0; r < 4; r++)
      if (!mismo_conjunto(t, r, (uint8_t)((r + 1) & 3), t == TETRIS_I ? 4 : 3))
        ok = 0;
    char msg[80];
    snprintf(msg, sizeof msg, "formas: la rotacion de %s es un giro de 90 grados",
             nombres[t]);
    CHECK(ok, msg);
  }
  int o_igual = 1;
  for (uint8_t r = 1; r < 4; r++)
    for (int i = 0; i < 4; i++)
      if (forma_x(TETRIS_O, r, i) != forma_x(TETRIS_O, 0, i) ||
          forma_y(TETRIS_O, r, i) != forma_y(TETRIS_O, 0, i))
        o_igual = 0;
  CHECK(o_igual, "formas: el cuadrado O es igual en las 4 rotaciones");
  CHECK(tetris_forma(TETRIS_T, 4) == tetris_forma(TETRIS_T, 0),
        "formas: la rotacion 4 equivale a la 0");
}

/* ---------------------------------------------------------------- inicio */
static void pruebas_inicio(void) {
  EstadoTetris e = nuevo();
  CHECK(ocupadas(&e) == 0, "inicio: tablero vacio");
  CHECK(e.nivel == 0 && e.puntaje == 0 && e.lineas == 0, "inicio: nivel, puntaje y lineas en 0");
  CHECK(!e.juego_terminado, "inicio: partida en curso");
  CHECK(e.px == 3 && e.py == TETRIS_APARICION_Y && e.rot == 0,
        "inicio: la pieza aparece arriba al centro");
  int visibles = 1;
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++) {
    int alguna = 0;
    for (int i = 0; i < 4; i++)
      if (TETRIS_APARICION_Y + forma_y(t, 0, i) >= TETRIS_FILAS_OCULTAS)
        alguna = 1;
    if (!alguna)
      visibles = 0;
  }
  CHECK(visibles, "inicio: las 7 piezas ya se ven en pantalla en cuanto aparecen");
  CHECK(e.tipo < 7 && e.siguiente < 7, "inicio: pieza actual y siguiente validas");
  CHECK(!tetris_colision(&e, e.tipo, e.rot, e.px, e.py), "inicio: la pieza no choca");
  CHECK(e.piezas == 0, "inicio: ninguna pieza fijada");

  int distintas = 1;
  for (uint32_t s = 1; s <= 50; s++) {
    EstadoTetris t;
    tetris_inicializar(&t, s);
    if (t.tipo == t.siguiente)
      distintas = 0;
  }
  CHECK(distintas, "inicio: la pieza actual y la siguiente salen de la misma bolsa y difieren");

  EstadoTetris z;
  tetris_inicializar(&z, 0);
  CHECK(z.prng_estado != 0, "semilla 0 no deja el generador en cero");

  EstadoTetris a, b;
  tetris_inicializar(&a, 777);
  tetris_inicializar(&b, 777);
  for (int i = 0; i < 2000; i++) {
    EntradaTetris in = nada();
    in.izquierda = (uint8_t)((i / 5) & 1);
    in.rotar_der = (uint8_t)(i % 9 == 0);
    in.caida_directa = (uint8_t)(i % 37 == 0);
    tetris_actualizar(&a, in);
    tetris_actualizar(&b, in);
  }
  CHECK(memcmp(&a, &b, sizeof a) == 0, "misma semilla, misma partida");

  int tipos[7] = {0};
  for (uint32_t s = 1; s <= 70; s++) {
    EstadoTetris t;
    tetris_inicializar(&t, s);
    tipos[t.tipo] = 1;
  }
  int cuantos = 0;
  for (int i = 0; i < 7; i++)
    cuantos += tipos[i];
  CHECK(cuantos == 7, "semillas distintas dan las 7 piezas como primera");
}

/* ----------------------------------------------------------------- bolsa */
static void pruebas_bolsa(void) {
  EstadoTetris e = nuevo();
  e.bolsa_pos = TETRIS_PIEZAS;
  int permutaciones = 1, cuentas[7] = {0, 0, 0, 0, 0, 0, 0};
  int ultima[7];
  for (int i = 0; i < 7; i++)
    ultima[i] = -100;
  int holgura_max = 0;
  for (int b = 0; b < 500; b++) {
    int vistos[7] = {0};
    for (int k = 0; k < 7; k++) {
      uint8_t p = tetris_robar_pieza(&e);
      int pos = b * 7 + k;
      if (p >= 7) {
        permutaciones = 0;
        continue;
      }
      vistos[p]++;
      cuentas[p]++;
      if (ultima[p] >= 0 && pos - ultima[p] - 1 > holgura_max)
        holgura_max = pos - ultima[p] - 1;
      ultima[p] = pos;
    }
    for (int i = 0; i < 7; i++)
      if (vistos[i] != 1)
        permutaciones = 0;
  }
  CHECK(permutaciones, "bolsa: cada grupo de 7 piezas trae las 7 distintas");
  int iguales = 1;
  for (int i = 1; i < 7; i++)
    if (cuentas[i] != cuentas[0])
      iguales = 0;
  CHECK(iguales && cuentas[0] == 500, "bolsa: las 7 piezas salen exactamente igual de seguido");
  CHECK(holgura_max <= 12, "bolsa: una pieza nunca tarda mas de 12 piezas en volver");
  CHECK(holgura_max >= 6, "bolsa: el orden cambia de una bolsa a otra");
}

/* ----------------------------------------------------------- movimiento */
static void pruebas_movimiento(void) {
  EstadoTetris e = con_pieza(TETRIS_T);
  EntradaTetris in = nada();
  in.izquierda = 1;
  con(&e, in);
  CHECK(e.px == 2, "izquierda mueve una columna");
  CHECK(e.eventos & TETRIS_EVT_MOVER, "mover avisa evento");
  e = con_pieza(TETRIS_T);
  in = nada();
  in.derecha = 1;
  con(&e, in);
  CHECK(e.px == 4, "derecha mueve una columna");
  e = con_pieza(TETRIS_T);
  in.izquierda = 1;
  in.derecha = 1;
  con(&e, in);
  CHECK(e.px == 3, "izquierda y derecha a la vez no mueven");

  int pared_izq = 1, pared_der = 1;
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++) {
    EstadoTetris a = con_pieza(t);
    for (int i = 0; i < 40; i++) {
      EntradaTetris p = nada();
      p.izquierda = (uint8_t)(i & 1);
      con(&a, p);
    }
    if (min_x(&a) != 0)
      pared_izq = 0;
    EstadoTetris b = con_pieza(t);
    for (int i = 0; i < 40; i++) {
      EntradaTetris p = nada();
      p.derecha = (uint8_t)(i & 1);
      con(&b, p);
    }
    if (max_x(&b) != TETRIS_COLUMNAS - 1)
      pared_der = 0;
  }
  CHECK(pared_izq, "las 7 piezas llegan a la pared izquierda y no la pasan");
  CHECK(pared_der, "las 7 piezas llegan a la pared derecha y no la pasan");

  /* repeticion automatica: mover en los cuadros 0, 8, 11, 14 */
  EstadoTetris d = con_pieza(TETRIS_O);
  d.px = 7;
  int px[20];
  EntradaTetris izq = nada();
  izq.izquierda = 1;
  for (int k = 0; k < 20; k++) {
    con(&d, izq);
    px[k] = d.px;
  }
  CHECK(px[0] == 6 && px[7] == 6, "repeticion: un solo paso al presionar");
  CHECK(px[8] == 5 && px[10] == 5, "repeticion: empieza a los 8 cuadros");
  CHECK(px[11] == 4 && px[13] == 4 && px[14] == 3,
        "repeticion: despues avanza cada 3 cuadros");

  EstadoTetris r = con_pieza(TETRIS_O);
  r.px = 7;
  con(&r, izq);
  paso(&r);
  con(&r, izq);
  CHECK(r.px == 5, "soltar y volver a presionar mueve de inmediato");
}

/* -------------------------------------------------------------- rotacion */
static void pruebas_rotacion(void) {
  int vuelve_der = 1, vuelve_izq = 1, ida_vuelta = 1;
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++) {
    EstadoTetris e = con_pieza(t);
    e.py = 6;
    EntradaTetris der = nada(), izq = nada();
    der.rotar_der = 1;
    izq.rotar_izq = 1;
    for (int i = 0; i < 4; i++)
      con(&e, der);
    if (e.rot != 0 || e.px != 3)
      vuelve_der = 0;
    for (int i = 0; i < 4; i++)
      con(&e, izq);
    if (e.rot != 0 || e.px != 3)
      vuelve_izq = 0;
    con(&e, der);
    con(&e, izq);
    if (e.rot != 0)
      ida_vuelta = 0;
  }
  CHECK(vuelve_der, "4 giros a la derecha dejan la pieza como estaba");
  CHECK(vuelve_izq, "4 giros a la izquierda dejan la pieza como estaba");
  CHECK(ida_vuelta, "derecha e izquierda se cancelan");

  EstadoTetris e = con_pieza(TETRIS_T);
  e.py = 6;
  EntradaTetris der = nada();
  der.rotar_der = 1;
  con(&e, der);
  CHECK(e.rot == 1 && (e.eventos & TETRIS_EVT_ROTAR), "rotar avisa evento y cambia la rotacion");
  EstadoTetris o = con_pieza(TETRIS_O);
  o.py = 6;
  con(&o, der);
  CHECK(o.rot == 0 && !(o.eventos & TETRIS_EVT_ROTAR), "el cuadrado O no rota ni avisa");

  /* desplazamientos contra la pared */
  EstadoTetris t = con_pieza(TETRIS_T);
  t.rot = 1;
  t.px = -1;
  t.py = 6;
  EntradaTetris izq = nada();
  izq.rotar_izq = 1;
  con(&t, izq);
  CHECK(t.rot == 0 && t.px == 0, "T junto a la pared izquierda rota y se corre una columna");

  EstadoTetris i = con_pieza(TETRIS_I);
  i.rot = 1;
  i.px = -2;
  i.py = 6;
  con(&i, der);
  CHECK(i.rot == 2 && i.px == 0, "I vertical en la pared rota y se corre dos columnas");

  EstadoTetris d = con_pieza(TETRIS_I);
  d.rot = 1;
  d.px = 7;
  d.py = 6;
  con(&d, der);
  CHECK(d.rot == 2 && d.px == 6, "I vertical en la pared derecha rota y se corre una columna");

  /* rotacion imposible: el estado no cambia */
  EstadoTetris b = con_pieza(TETRIS_T);
  b.py = 6;
  for (int y = 5; y <= 9; y++)
    fila_llena(&b, y, -1);
  for (int k = 0; k < 4; k++)
    b.tablero[b.py + forma_y(b.tipo, 0, k)][b.px + forma_x(b.tipo, 0, k)] = 0;
  EstadoTetris antes = b;
  con(&b, der);
  CHECK(b.rot == 0 && b.px == antes.px && !(b.eventos & TETRIS_EVT_ROTAR),
        "rotacion bloqueada por todos lados: no cambia nada");

  /* rotar una pieza apoyada reinicia la espera de bloqueo */
  EstadoTetris g = con_pieza(TETRIS_T);
  g.rot = 2; /* con el tallo hacia abajo */
  g.py = 19; /* el tallo queda en la ultima fila: apoyada */
  paso(&g);
  paso(&g);
  int cuenta = g.bloqueo_cuenta;
  con(&g, der);
  /* se reinicia a 0 y ese mismo cuadro vuelve a contar 1 */
  CHECK(cuenta == 2 && g.rot == 3 && g.bloqueo_cuenta == 1 && g.bloqueo_reinicios == 1,
        "rotar una pieza apoyada reinicia la espera de bloqueo");
}

/* Rotacion SRS: giros contra el piso, la pila y las paredes. */
static void pruebas_srs(void) {
  /* los datos de las pruebas son simetricos: ir y volver se deshace */
  int simetricos = 1;
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++) {
    if (t == TETRIS_O)
      continue;
    for (uint8_t r = 0; r < 4; r++) {
      const int8_t *ida = tetris_kicks(t, r, (uint8_t)((r + 1) & 3));
      const int8_t *vuelta = tetris_kicks(t, (uint8_t)((r + 1) & 3), r);
      for (int k = 1; k < 5; k++)
        if (ida[2 * k] != -vuelta[2 * k] || ida[2 * k + 1] != -vuelta[2 * k + 1])
          simetricos = 0;
      if (ida[0] != 0 || ida[1] != 0)
        simetricos = 0;
    }
  }
  CHECK(simetricos, "SRS: la tabla de ida y la de vuelta son opuestas y la primera prueba es (0, 0)");
  CHECK(tetris_kicks(TETRIS_T, 0, 1) != tetris_kicks(TETRIS_T, 0, 3) &&
            tetris_kicks(TETRIS_T, 0, 1)[2] == -1 && tetris_kicks(TETRIS_T, 0, 3)[2] == 1,
        "SRS: girar 0 a la derecha prueba primero a la izquierda y a la izquierda, a la derecha");
  CHECK(tetris_kicks(TETRIS_I, 0, 1) != tetris_kicks(TETRIS_T, 0, 1),
        "SRS: la pieza I tiene su propia tabla");

  /* toda pieza apoyada en el piso puede girar en las 4 rotaciones, ida y vuelta */
  int piso_der = 1, piso_izq = 1;
  for (uint8_t t = 0; t < TETRIS_PIEZAS; t++)
    for (uint8_t r = 0; r < 4; r++) {
      EstadoTetris e = con_pieza(t);
      e.rot = r;
      e.py = tetris_sombra_py(&e);
      EntradaTetris der = nada(), izq = nada();
      der.rotar_der = 1;
      izq.rotar_izq = 1;
      EstadoTetris a = e, b = e;
      con(&a, der);
      con(&b, izq);
      if (t != TETRIS_O && (a.rot != ((r + 1) & 3) || a.py > e.py + 0))
        piso_der = 0;
      if (t != TETRIS_O && (b.rot != ((r + 3) & 3)))
        piso_izq = 0;
    }
  CHECK(piso_der, "SRS: toda pieza apoyada en el piso puede girar a la derecha");
  CHECK(piso_izq, "SRS: toda pieza apoyada en el piso puede girar a la izquierda");

  EstadoTetris tp = con_pieza(TETRIS_T);
  tp.py = tetris_sombra_py(&tp); /* T plana apoyada en el piso: py = 20 */
  EntradaTetris der = nada();
  der.rotar_der = 1;
  con(&tp, der);
  CHECK(tp.rot == 1 && tp.px == 2 && tp.py == 19,
        "SRS: T plana en el piso gira a vertical subiendo una fila y corriendose a la izquierda");

  EstadoTetris ip = con_pieza(TETRIS_I);
  ip.py = tetris_sombra_py(&ip); /* I horizontal en el piso */
  con(&ip, der);
  CHECK(ip.rot == 1 && ip.px == 4 && ip.py == 18,
        "SRS: I horizontal en el piso gira a vertical subiendo dos filas");

  /* encima de la pila tambien gira */
  EstadoTetris pila = con_pieza(TETRIS_L);
  for (int y = 17; y < TETRIS_FILAS_TOTAL; y++)
    fila_llena(&pila, y, -1);
  pila.py = tetris_sombra_py(&pila);
  EstadoTetris antes = pila;
  con(&pila, der);
  CHECK(pila.rot == 1 && pila.py < antes.py && !tetris_colision(&pila, pila.tipo, pila.rot, pila.px, pila.py),
        "SRS: una pieza plana sobre la pila gira hacia arriba sin chocar");

  /* hueco en U: la pieza T entra y gira dentro de un pozo de 3 de ancho */
  EstadoTetris pozo = con_pieza(TETRIS_T);
  pozo.px = 3;
  pozo.py = 8;
  for (int y = 8; y < TETRIS_FILAS_TOTAL; y++)
    for (int x = 0; x < TETRIS_COLUMNAS; x++)
      pozo.tablero[y][x] = (uint8_t)(x < 3 || x > 6 ? 1 : 0);
  con(&pozo, der);
  CHECK(!tetris_colision(&pozo, pozo.tipo, pozo.rot, pozo.px, pozo.py),
        "SRS: girar dentro de un pozo nunca deja la pieza encimada");
}

/* -------------------------------------------------------------- gravedad */
static void pruebas_gravedad(void) {
  EstadoTetris e = nuevo();
  const int y0 = TETRIS_APARICION_Y;
  for (int i = 0; i < 25; i++)
    paso(&e);
  CHECK(e.py == y0, "nivel 0: a los 25 cuadros todavia no baja");
  paso(&e);
  CHECK(e.py == y0 + 1, "nivel 0: baja una fila a los 26 cuadros");
  for (int i = 0; i < 26; i++)
    paso(&e);
  CHECK(e.py == y0 + 2, "nivel 0: baja una fila cada 26 cuadros");

  int decreciente = 1;
  for (uint8_t n = 1; n <= TETRIS_NIVEL_MAX; n++)
    if (tetris_frames_por_fila(n) > tetris_frames_por_fila((uint8_t)(n - 1)))
      decreciente = 0;
  CHECK(decreciente, "gravedad: cada nivel cae igual o mas rapido");
  CHECK(tetris_frames_por_fila(0) == 26 && tetris_frames_por_fila(15) == 1,
        "gravedad: del nivel 0 (26 cuadros) al 15 (1 cuadro)");
  CHECK(tetris_frames_por_fila(200) == tetris_frames_por_fila(15),
        "gravedad: un nivel fuera de rango se trata como el maximo");

  EstadoTetris r = nuevo();
  r.nivel = 15;
  paso(&r);
  paso(&r);
  paso(&r);
  CHECK(r.py == y0 + 3, "nivel 15: baja una fila por cuadro");

  EstadoTetris s = nuevo();
  EntradaTetris abajo = nada();
  abajo.abajo = 1;
  for (int i = 0; i < 10; i++)
    con(&s, abajo);
  CHECK(s.py == y0 + 5, "caida suave: baja una fila cada 2 cuadros");
  CHECK(s.puntaje == 5, "caida suave: 1 punto por fila");
  EstadoTetris s2 = nuevo();
  s2.nivel = 15;
  con(&s2, abajo);
  CHECK(s2.py == y0 + 1, "caida suave: en nivel 15 sigue a un cuadro por fila");
}

/* ---------------------------------------------------------------- bloqueo */
static void pruebas_bloqueo(void) {
  EstadoTetris e = con_pieza(TETRIS_I);
  e.py = 20; /* I horizontal ocupa la fila py + 1 = 21 (la ultima) */
  for (int i = 0; i < TETRIS_BLOQUEO_FRAMES - 1; i++)
    paso(&e);
  CHECK(e.piezas == 0 && ocupadas(&e) == 0, "bloqueo: la pieza espera 8 cuadros antes de fijarse");
  paso(&e);
  CHECK(e.piezas == 1, "bloqueo: a los 8 cuadros se fija");
  CHECK(e.eventos & TETRIS_EVT_FIJAR, "bloqueo: avisa evento de fijar");
  CHECK(ocupadas(&e) == 4, "bloqueo: quedan 4 celdas en el tablero");
  CHECK(e.tablero[21][3] == TETRIS_I + 1 && e.tablero[21][6] == TETRIS_I + 1,
        "bloqueo: las celdas guardan el tipo de pieza + 1");
  CHECK(e.py == TETRIS_APARICION_Y && e.px == 3,
        "bloqueo: aparece la pieza siguiente arriba");

  EstadoTetris m = con_pieza(TETRIS_O);
  m.py = 20; /* O ocupa py..py+1 = 20..21 */
  m.px = 4;
  for (int i = 0; i < 5; i++)
    paso(&m);
  EntradaTetris izq = nada();
  izq.izquierda = 1;
  con(&m, izq);
  CHECK(m.bloqueo_cuenta == 1 && m.bloqueo_reinicios == 1,
        "mover una pieza apoyada reinicia la espera (queda en 1: ese cuadro cuenta)");
  for (int i = 0; i < 6; i++)
    paso(&m);
  CHECK(m.piezas == 0, "despues de reiniciar todavia no se fija");

  EstadoTetris w = con_pieza(TETRIS_O);
  w.py = 20;
  w.px = 4;
  for (int i = 0; i < 400 && w.piezas == 0; i++) {
    EntradaTetris p = nada();
    p.izquierda = (uint8_t)((i / 2) & 1 ? 0 : (i & 1));
    p.derecha = (uint8_t)((i / 2) & 1 ? (i & 1) : 0);
    con(&w, p);
  }
  CHECK(w.piezas == 1, "mover sin parar no evita que la pieza se fije (maximo de reinicios)");

  EstadoTetris a = con_pieza(TETRIS_O);
  a.py = 20;
  EntradaTetris abajo = nada();
  abajo.abajo = 1;
  con(&a, abajo);
  CHECK(a.piezas == 1, "abajo mantenido en el piso fija la pieza de inmediato");

  /* la pieza nueva no cae rapido mientras no se suelte "abajo" */
  int y_antes = a.py;
  for (int i = 0; i < 10; i++)
    con(&a, abajo);
  CHECK(a.py == y_antes && a.puntaje == 0,
        "abajo seguido de una pieza fijada: la pieza nueva no cae rapido ni da puntos");
  paso(&a); /* se suelta */
  CHECK(!a.abajo_bloqueado, "al soltar abajo se vuelve a permitir la caida suave");
  for (int i = 0; i < 4; i++)
    con(&a, abajo);
  CHECK(a.py == y_antes + 2 && a.puntaje == 2, "despues de soltar, abajo vuelve a funcionar");
}

/* ------------------------------------------------------------ caida directa */
static void pruebas_caida_directa(void) {
  EstadoTetris e = con_pieza(TETRIS_I);
  CHECK(tetris_sombra_py(&e) == 20, "sombra: I horizontal en tablero vacio cae hasta py = 20");
  EntradaTetris dd = nada();
  dd.caida_directa = 1;
  con(&e, dd);
  CHECK(e.piezas == 1 && ocupadas(&e) == 4, "caida directa: fija la pieza al instante");
  CHECK(e.puntaje == 2 * (20 - TETRIS_APARICION_Y),
        "caida directa: 2 puntos por cada fila que baja");
  CHECK(e.eventos & TETRIS_EVT_FIJAR, "caida directa: avisa evento de fijar");
  CHECK(e.py == TETRIS_APARICION_Y, "caida directa: aparece la pieza siguiente");
  CHECK(e.tablero[21][3] == 1 && e.tablero[21][6] == 1, "caida directa: queda en la ultima fila");

  EstadoTetris f = con_pieza(TETRIS_O);
  fila_llena(&f, 15, 0); /* con un hueco en la columna 0, para que no se limpie */
  CHECK(tetris_sombra_py(&f) == 13, "sombra: se apoya sobre los bloques");
  con(&f, dd);
  CHECK(f.tablero[13][4] == TETRIS_O + 1 && f.tablero[14][5] == TETRIS_O + 1,
        "caida directa: se apoya sobre los bloques");

  int coincide = 1;
  for (int n = 0; n < 200; n++) {
    EstadoTetris g = con_pieza((uint8_t)(azar() % 7u));
    for (int k = 0; k < 30; k++)
      g.tablero[8 + (int)(azar() % 14u)][azar() % 10u] = 1;
    for (int k = 0; k < 4; k++)
      g.tablero[g.py + forma_y(g.tipo, 0, k)][g.px + forma_x(g.tipo, 0, k)] = 0;
    int8_t sombra = tetris_sombra_py(&g);
    if (tetris_colision(&g, g.tipo, g.rot, g.px, sombra) ||
        !tetris_colision(&g, g.tipo, g.rot, g.px, sombra + 1))
      coincide = 0;
  }
  CHECK(coincide, "sombra: siempre es la ultima fila libre antes de chocar");
}

/* ----------------------------------------------------------------- lineas */
/* I vertical en la columna 9 (px = 7), caida directa. Llena las filas dadas. */
static EstadoTetris con_lineas(int primera_fila, int ultima_fila) {
  EstadoTetris e = con_pieza(TETRIS_I);
  e.rot = 1;
  e.px = 7;
  for (int y = primera_fila; y <= ultima_fila; y++)
    fila_llena(&e, y, 9);
  return e;
}

/* Puntos de la caida directa de la I vertical desde la aparicion hasta py = 18. */
#define CAIDA_I (2 * (18 - TETRIS_APARICION_Y))

static void pruebas_lineas(void) {
  EntradaTetris dd = nada();
  dd.caida_directa = 1;

  EstadoTetris cuatro = con_lineas(18, 21);
  con(&cuatro, dd);
  CHECK(cuatro.lineas == 4 && cuatro.ultimas_lineas == 4, "4 lineas: cuenta 4");
  CHECK(cuatro.puntaje == 1200 + CAIDA_I, "4 lineas: 1200 + 2 puntos por cada fila de caida");
  CHECK(cuatro.eventos & TETRIS_EVT_TETRIS, "4 lineas: avisa evento tetris");
  CHECK(!(cuatro.eventos & TETRIS_EVT_LINEA), "4 lineas: no avisa el evento de 1 a 3");
  CHECK(ocupadas(&cuatro) == 0, "4 lineas: el tablero queda vacio");

  EstadoTetris una = con_lineas(21, 21);
  una.tablero[17][0] = 3; /* marca encima, para ver que baja */
  con(&una, dd);
  CHECK(una.lineas == 1 && una.puntaje == 40 + CAIDA_I, "1 linea: 40 + caida");
  CHECK(una.eventos & TETRIS_EVT_LINEA, "1 linea: avisa evento de linea");
  CHECK(una.tablero[18][0] == 3 && una.tablero[17][0] == 0,
        "1 linea: lo que estaba arriba baja una fila");
  CHECK(una.tablero[21][9] == TETRIS_I + 1 && una.tablero[20][9] == TETRIS_I + 1 &&
            una.tablero[19][9] == TETRIS_I + 1 && ocupadas(&una) == 4,
        "1 linea: los 3 bloques de la I que sobran bajan");

  EstadoTetris dos = con_lineas(20, 21);
  con(&dos, dd);
  CHECK(dos.lineas == 2 && dos.puntaje == 100 + CAIDA_I, "2 lineas: 100 + caida");
  EstadoTetris tres = con_lineas(19, 21);
  con(&tres, dd);
  CHECK(tres.lineas == 3 && tres.puntaje == 300 + CAIDA_I, "3 lineas: 300 + caida");

  /* filas no contiguas: la del medio queda y baja */
  EstadoTetris salto = con_lineas(19, 21);
  salto.tablero[20][0] = 0; /* la fila 20 tiene un segundo hueco */
  con(&salto, dd);
  CHECK(salto.lineas == 2, "filas no contiguas: se limpian solo las completas");
  CHECK(salto.tablero[21][0] == 0 && salto.tablero[21][1] == 1 &&
            salto.tablero[21][9] == TETRIS_I + 1,
        "filas no contiguas: la fila incompleta cae al fondo con su hueco");

  /* el nivel multiplica */
  EstadoTetris n2 = con_lineas(21, 21);
  n2.nivel = 2;
  con(&n2, dd);
  CHECK(n2.puntaje == 120 + CAIDA_I, "nivel 2: 1 linea vale 40 x 3");

  /* subida de nivel */
  EstadoTetris sube = con_lineas(20, 21);
  sube.lineas_nivel = 8;
  con(&sube, dd);
  CHECK(sube.nivel == 1 && sube.lineas_nivel == 0, "10 lineas: sube de nivel");
  CHECK(sube.eventos & TETRIS_EVT_NIVEL, "subir de nivel avisa evento");
  CHECK(sube.puntaje == 100 + CAIDA_I, "la subida de nivel no afecta el puntaje de esa jugada");

  EstadoTetris tope = con_lineas(21, 21);
  tope.nivel = TETRIS_NIVEL_MAX;
  tope.lineas_nivel = 9;
  con(&tope, dd);
  CHECK(tope.nivel == TETRIS_NIVEL_MAX && !(tope.eventos & TETRIS_EVT_NIVEL),
        "el nivel no pasa del maximo");

  EstadoTetris lin = con_lineas(18, 21);
  lin.lineas = 65534;
  con(&lin, dd);
  CHECK(lin.lineas == 65535, "el contador de lineas se satura en 65535 y no da la vuelta");

  EstadoTetris cap = con_lineas(18, 21);
  cap.puntaje = 999990;
  con(&cap, dd);
  CHECK(cap.puntaje == TETRIS_PUNTAJE_MAX, "el puntaje no pasa de 999999");

  EstadoTetris cont = con_lineas(21, 21);
  con(&cont, dd);
  CHECK(ocupadas(&cont) == 3 && cont.piezas == 1,
        "1 linea: de la I solo quedan 3 celdas (la 4a completo la fila)");
}

static void pruebas_celda_visible(void) {
  EstadoTetris e = nuevo();
  e.tablero[TETRIS_FILAS_OCULTAS][0] = 3; /* primera fila visible */
  e.tablero[TETRIS_FILAS_TOTAL - 1][9] = 5; /* ultima fila visible */
  e.tablero[0][4] = 7;                     /* fila oculta */
  CHECK(tetris_celda(&e, 0, 0) == 3 && tetris_celda(&e, 9, 19) == 5,
        "celda visible: la fila 0 es la primera despues de las ocultas");
  CHECK(tetris_celda(&e, 4, -1) == 0 && tetris_celda(&e, 4, -2) == 0,
        "celda visible: las filas ocultas no se ven");
  CHECK(tetris_celda(&e, -1, 0) == 0 && tetris_celda(&e, 10, 0) == 0 &&
            tetris_celda(&e, 0, 20) == 0,
        "celda visible: fuera del tablero devuelve 0");
}

/* --------------------------------------------------------- fin de partida */
static void pruebas_fin(void) {
  EntradaTetris dd = nada();
  dd.caida_directa = 1;

  EstadoTetris b = con_pieza(TETRIS_O);
  b.px = 7;
  b.tablero[TETRIS_APARICION_Y + 1][4] = 1; /* toda pieza nueva ocupa la celda (4, 2) */
  con(&b, dd);
  CHECK(b.juego_terminado, "fin: si la pieza nueva no cabe, termina");
  CHECK(b.eventos & TETRIS_EVT_FIN, "fin: avisa evento");

  /* fijada por completo en las filas ocultas (por ejemplo tras un giro hacia arriba) */
  EstadoTetris l = con_pieza(TETRIS_O);
  l.px = 7; /* columnas 8 y 9, lejos de donde aparece la pieza siguiente */
  l.py = 0;
  l.py_max = 0;
  for (int y = TETRIS_FILAS_OCULTAS; y < TETRIS_FILAS_TOTAL; y++) {
    l.tablero[y][8] = 1;
    l.tablero[y][9] = 1;
  }
  for (int i = 0; i < 20 && !l.juego_terminado; i++)
    paso(&l);
  CHECK(l.juego_terminado && l.piezas == 1,
        "fin: una pieza que se fija por completo sobre el tablero termina la partida");

  EstadoTetris c = b;
  c.eventos = 0;
  EstadoTetris copia = c;
  EntradaTetris todo = {1, 0, 1, 1, 1, 1};
  for (int i = 0; i < 500; i++)
    con(&c, todo);
  copia.eventos = 0;
  CHECK(memcmp(&c, &copia, sizeof c) == 0, "fin: terminada la partida nada cambia");

  int termina = 1, pocas = 1;
  for (uint32_t s = 1; s <= 30; s++) {
    EstadoTetris p;
    tetris_inicializar(&p, s);
    int n = 0;
    while (!p.juego_terminado && n < 5000) {
      paso(&p);
      n++;
    }
    if (!p.juego_terminado)
      termina = 0;
    if (p.piezas > 40)
      pocas = 0;
  }
  CHECK(termina, "un jugador que no hace nada pierde");
  CHECK(pocas, "un jugador que no hace nada pierde en menos de 40 piezas");
}

/* ---------------------------------------------------------------- puntajes */
static void pruebas_puntajes(void) {
  uint32_t top[TETRIS_TOP_N] = {0, 0, 0, 0, 0};
  CHECK(!tetris_registrar_puntaje(top, 0), "un puntaje 0 no entra a la tabla");
  CHECK(tetris_registrar_puntaje(top, 100) && top[0] == 100, "el primero entra");
  tetris_registrar_puntaje(top, 300);
  tetris_registrar_puntaje(top, 200);
  tetris_registrar_puntaje(top, 50);
  tetris_registrar_puntaje(top, 400);
  CHECK(top[0] == 400 && top[1] == 300 && top[2] == 200 && top[3] == 100 && top[4] == 50,
        "la tabla queda ordenada de mayor a menor");
  CHECK(!tetris_registrar_puntaje(top, 50), "empatar con el ultimo no entra");
  CHECK(tetris_registrar_puntaje(top, 250) && top[2] == 250 && top[4] == 100,
        "uno nuevo se inserta en su lugar y sale el peor");
  CHECK(tetris_registrar_puntaje(top, 999999) && top[0] == 999999,
        "un nuevo record queda primero");
}

/* -------------------------------------------------------------------- bot */
static void pruebas_bot(void) {
  int juegos = 6, sobrevive = 0, lineas = 0, nivel_max = 0, tetris = 0, invariantes = 1;
  for (int g = 0; g < juegos; g++) {
    EstadoTetris e;
    tetris_inicializar(&e, (uint32_t)(1000 + g));
    BotTetris b;
    memset(&b, 0, sizeof b);
    int frames = 0;
    while (!e.juego_terminado && e.piezas < 600 && frames < 400000) {
      con(&e, bot_decidir(&b, &e));
      frames++;
      if (e.eventos & TETRIS_EVT_TETRIS)
        tetris++;
      if (!e.juego_terminado && tetris_colision(&e, e.tipo, e.rot, e.px, e.py))
        invariantes = 0;
    }
    if (e.piezas >= 600)
      sobrevive++;
    lineas += e.lineas;
    if (e.nivel > nivel_max)
      nivel_max = e.nivel;
    if (ocupadas(&e) != (int)(4 * e.piezas) - 10 * e.lineas)
      invariantes = 0;
  }
  CHECK(sobrevive >= 5, "jugador automatico: sobrevive 600 piezas en casi todas las partidas");
  CHECK(lineas >= 1300, "jugador automatico: limpia casi todas las lineas posibles (1300 de 1440)");
  CHECK(nivel_max >= 15, "jugador automatico: llega al nivel maximo");
  CHECK(tetris >= 1, "jugador automatico: logra al menos un tetris (4 lineas)");
  CHECK(invariantes, "jugador automatico: nunca hay choques y siempre se conservan las celdas");
}

/* ------------------------------------------------------------------- fuzz */
static void pruebas_fuzz(void) {
  int viola = 0, no_termina = 0, incoherente = 0;
  long cuadros = 0;
  for (int g = 0; g < 1500; g++) {
    EstadoTetris e;
    tetris_inicializar(&e, azar() + 1u);
    uint32_t puntaje_prev = 0, piezas_prev = 0;
    unsigned suma_lineas = 0;
    int fines = 0, t = 0;
    for (; t < 100000 && !e.juego_terminado; t++) {
      EntradaTetris in;
      in.izquierda = (uint8_t)(azar() % 3u == 0);
      in.derecha = (uint8_t)(azar() % 3u == 0);
      in.abajo = (uint8_t)(azar() % 4u == 0);
      in.caida_directa = (uint8_t)(azar() % 40u == 0);
      in.rotar_der = (uint8_t)(azar() % 6u == 0);
      in.rotar_izq = (uint8_t)(azar() % 9u == 0);
      tetris_actualizar(&e, in);
      for (int y = 0; y < TETRIS_FILAS_TOTAL; y++)
        for (int x = 0; x < TETRIS_COLUMNAS; x++)
          if (e.tablero[y][x] > 7)
            viola++;
      if (!e.juego_terminado && tetris_colision(&e, e.tipo, e.rot, e.px, e.py))
        viola++;
      /* la caja de la pieza puede quedar sobre la fila 0 si sus celdas caben */
      if (e.py < -2 || e.py >= TETRIS_FILAS_TOTAL || e.nivel > TETRIS_NIVEL_MAX ||
          e.puntaje > TETRIS_PUNTAJE_MAX || e.tipo >= 7 || e.siguiente >= 7 ||
          e.ultimas_lineas > 4)
        viola++;
      if (e.puntaje < puntaje_prev)
        incoherente++;
      puntaje_prev = e.puntaje;
      suma_lineas += e.ultimas_lineas;
      if (((e.eventos & TETRIS_EVT_FIJAR) != 0) != (e.piezas != piezas_prev))
        incoherente++;
      piezas_prev = e.piezas;
      if (ocupadas(&e) != (int)(4 * e.piezas) - 10 * (int)suma_lineas)
        incoherente++;
      if (e.eventos & TETRIS_EVT_FIN)
        fines++;
    }
    cuadros += t;
    if (suma_lineas != e.lineas)
      incoherente++;
    if (!e.juego_terminado)
      no_termina++;
    else if (fines != 1)
      incoherente++;
  }
  CHECK(viola == 0, "fuzz: tablero, pieza, nivel y puntaje siempre en rango");
  CHECK(incoherente == 0,
        "fuzz: celdas = 4 por pieza - 10 por linea, eventos y puntaje coherentes");
  CHECK(no_termina == 0, "fuzz: 1500 partidas al azar terminan");
  CHECK(cuadros > 1500, "fuzz: se jugaron cuadros de verdad");
}

/* ----------------------------------------------------------------- assets */
static int sin_transparente(const uint16_t *s, unsigned n) {
  for (unsigned i = 0; i < n; i++)
    if (s[i] == 0x0843)
      return 0;
  return 1;
}
static int con_opacos(const uint16_t *s, unsigned n) {
  for (unsigned i = 0; i < n; i++)
    if (s[i] != 0x0843)
      return 1;
  return 0;
}

static void pruebas_assets(void) {
  const uint16_t *bloques[8] = {BloqueI, BloqueO, BloqueT, BloqueS,
                                BloqueZ, BloqueJ, BloqueL, BloqueSombra};
  CHECK(BloqueI_W == 3 && BloqueI_H == 3 && BloqueO_W == 3 && BloqueO_H == 3 &&
            BloqueT_W == 3 && BloqueT_H == 3 && BloqueS_W == 3 && BloqueS_H == 3 &&
            BloqueZ_W == 3 && BloqueZ_H == 3 && BloqueJ_W == 3 && BloqueJ_H == 3 &&
            BloqueL_W == 3 && BloqueL_H == 3 && BloqueSombra_W == 3 &&
            BloqueSombra_H == 3,
        "assets: los 7 bloques y la sombra miden 3x3");
  int distintos = 1;
  for (int i = 0; i < 7; i++)
    for (int j = i + 1; j < 7; j++)
      if (bloques[i][4] == bloques[j][4])
        distintos = 0;
  CHECK(distintos, "assets: cada pieza tiene un color distinto");
  int opacos = 1;
  for (int i = 0; i < 7; i++)
    if (!sin_transparente(bloques[i], 9))
      opacos = 0;
  CHECK(opacos, "assets: los bloques son opacos");
  CHECK(BloqueSombra[4] == 0x0843 && BloqueSombra[0] != 0x0843,
        "assets: la sombra es un marco con el centro transparente");

  const uint16_t *digs[10] = {Dig0, Dig1, Dig2, Dig3, Dig4, Dig5, Dig6, Dig7, Dig8, Dig9};
  int digitos = Dig0_W == 3 && Dig0_H == 5 && Dig1_W == 3 && Dig1_H == 5 &&
                Dig2_W == 3 && Dig2_H == 5 && Dig3_W == 3 && Dig3_H == 5 &&
                Dig4_W == 3 && Dig4_H == 5 && Dig5_W == 3 && Dig5_H == 5 &&
                Dig6_W == 3 && Dig6_H == 5 && Dig7_W == 3 && Dig7_H == 5 &&
                Dig8_W == 3 && Dig8_H == 5 && Dig9_W == 3 && Dig9_H == 5;
  int con_pixeles = 1, distintos_dig = 1;
  for (int i = 0; i < 10; i++) {
    if (!con_opacos(digs[i], 15))
      con_pixeles = 0;
    for (int j = i + 1; j < 10; j++)
      if (memcmp(digs[i], digs[j], 15 * sizeof(uint16_t)) == 0)
        distintos_dig = 0;
  }
  CHECK(digitos, "assets: los 10 digitos miden 3x5");
  CHECK(con_pixeles && distintos_dig, "assets: los 10 digitos tienen pixeles y son distintos");

  CHECK(Cancha_W == 64 && Cancha_H == 64 && MenuTetris_W == 64 && MenuTetris_H == 64,
        "assets: la cancha y el fondo del menu cubren los 64x64");
  CHECK(sin_transparente(Cancha, 4096) && sin_transparente(MenuTetris, 4096),
        "assets: los fondos son opacos (no dejan ver el cuadro anterior)");
  /* el tablero de 10x20 bloques de 3 px cabe entre los marcos (x 3 y 34) */
  CHECK(Cancha[1 * 64 + 3] != Cancha[10 * 64 + 10] && Cancha[30 * 64 + 34] != Cancha[10 * 64 + 10],
        "assets: la cancha tiene el marco del tablero en las columnas 3 y 34");
  CHECK(Cursor_W == 5 && Cursor_H == 7, "assets: el cursor mide 5x7");
  CHECK(LabelPaused_H == 7 && LabelGameOver_H == 7 && LabelPaused_W == 35 &&
            LabelGameOver_W == 53,
        "assets: los rotulos miden 7 de alto y caben en la pantalla");
  CHECK(con_opacos(LabelPaused, 35 * 7) && con_opacos(LabelGameOver, 53 * 7),
        "assets: los rotulos no estan vacios");
}

int main(int argc, char **argv) {
  verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
  pruebas_formas();
  pruebas_inicio();
  pruebas_bolsa();
  pruebas_movimiento();
  pruebas_rotacion();
  pruebas_srs();
  pruebas_gravedad();
  pruebas_bloqueo();
  pruebas_caida_directa();
  pruebas_lineas();
  pruebas_celda_visible();
  pruebas_fin();
  pruebas_puntajes();
  pruebas_bot();
  pruebas_fuzz();
  pruebas_assets();
  printf("\n%d pruebas, %d correctas, %d fallidas\n", total, total - fallos, fallos);
  return fallos ? 1 : 0;
}
