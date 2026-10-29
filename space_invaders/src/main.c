// Space Invaders para KUBIX.
// Flujo de pantallas segun docs/diagramas/Diagrama logica juegos.png:
// menu -> "Press start" -> partida por oleadas -> pausa / fin de partida.
// La logica avanza en pasos fijos de TICK_MS. Las posiciones siguen la maqueta
// assets/spaceInvaders/SpaceInvadersPlaying.png.

#include <string.h>

#include "assets.h"
#include "hal.h"

typedef enum {
  SCREEN_STARTMENU,
  SCREEN_MESSAGE,
  SCREEN_READY,
  SCREEN_GAME,
  SCREEN_PAUSE,
  SCREEN_GAMEOVER
} AppScreen;

typedef enum {
  MENU_ONE_PLAYER,
  MENU_TWO_PLAYERS,
  MENU_MULTIPLAYER,
  MENU_COUNT
} MenuItem;

typedef enum { PHASE_PLAYING, PHASE_DYING, PHASE_WAVE_INTRO } GamePhase;

enum {
  SCREEN_SIZE = 64,
  TICK_MS = 16,
  MAX_TICKS_PER_FRAME = 4,
  FRAME_MS = 16,
  HUD_H = 7,
  ALIEN_ROWS = 4,
  ALIEN_COLS = 6,
  ALIEN_W = 7,
  ALIEN_H = 5,
  ALIEN_PITCH_X = 9,
  ALIEN_PITCH_Y = 7,
  FORMATION_X = 6,
  FORMATION_Y = 14,
  FORMATION_DROP = 3,
  MARCH_MIN_TICKS = 4,
  SHIP_W = 7,
  SHIP_H = 5,
  SHIP_Y = 55,
  SHIP_MIN_X = 1,
  SHIP_MAX_X = SCREEN_SIZE - SHIP_W - 1,
  START_LIVES = 3,
  SHOT_H = 3,
  SHOT_SPEED = 2, // px por paso
  DYING_TICKS = 70,
  ALIEN_SHOTS = 3,
  ALIEN_SHOT_W = 2,
  ALIEN_SHOT_H = 4,
  GROUND_Y = 62,
  SHIELD_COUNT = 4,
  SHIELD_W = 9,
  SHIELD_H = 6,
  SHIELD_Y = 46,
  SHIELD_FIRST_X = 4,
  SHIELD_STEP = 16,
  SAUCER_W = 9,
  SAUCER_H = 4,
  SAUCER_Y = 8,
  SAUCER_MOVE_TICKS = 2,
  EXPLOSIONS = 6,
  ALIEN_EXPLOSION_TICKS = 12,
  POINTS_POPUP_TICKS = 50,
  WAVE_INTRO_TICKS = 90,
  MAX_SCORE = 9999,
  LIFE_W = 5,
  LIFE_H = 3,
  DIGIT_W = 3,
  DIGIT_H = 5,
  GAMEOVER_WAIT = 45, // cuadros mostrando el final antes del menu
  SCORE_BAND_Y = 26,  // franja libre de GameOverMenu para el puntaje
  CURSOR_X = 6,
  CURSOR_Y = 32,
  CURSOR_W = 3,
  CURSOR_H = 5,
  CURSOR_STEP = 9,
  CHAR_W = 3, // fuente pequena 3x5: no dibuja bien la W, la M, la U ni el 7
  CHAR_H = 5,
  BIG_W = 5, // fuente 5x7
  BIG_H = 7,
  OVERLAY_H = 17
};

static const uint16_t TRANSPARENT = 0x0843;
static const uint16_t P2_COLOR = 0xFE47; // nave del jugador 2 en amarillo

typedef struct {
  int16_t x, y;
  uint8_t active;
} Shot;

typedef struct {
  int16_t x;
  uint8_t lives;
  uint16_t score;
  Shot shot;
} Ship;

typedef struct {
  int16_t x, y; // esquina de la casilla [0][0], aunque esa columna ya no exista
  int8_t dir;
  uint8_t frame;
  uint8_t alive[ALIEN_ROWS][ALIEN_COLS];
  uint8_t alive_count;
  uint16_t march_timer;
} Formation;

typedef struct {
  int16_t x;
  int8_t dir;
  uint8_t active;
  uint8_t move_timer;
  uint16_t points;
  uint16_t spawn_timer;
} Saucer;

typedef struct {
  int16_t x, y;
  uint16_t points; // 0 = explosion de alien; si no, puntos ganados con el ovni
  uint8_t timer;
} Explosion;

typedef struct {
  uint8_t players;
  Ship ships[2];
  Formation aliens;
  Shot alien_shots[ALIEN_SHOTS];
  uint8_t alien_shot_timer;
  uint16_t alien_fire_timer;
  uint16_t shields[SHIELD_COUNT][SHIELD_W * SHIELD_H];
  Saucer saucer;
  Explosion explosions[EXPLOSIONS];
  uint16_t wave;
  GamePhase phase;
  uint16_t phase_timer;
  uint8_t dying_ship;
  uint16_t ticks;
  uint32_t rng;
} Game;

// Segundo control: J/L para moverse, I o K para disparar.
static struct keys player2_keys = {
    .UP = SDLK_i, .LEFT = SDLK_j, .RIGHT = SDLK_l, .A = SDLK_k};

static const int16_t ship_start_x[2][2] = {{28, 0}, {16, 40}};
static const uint8_t row_kind[ALIEN_ROWS] = {0, 1, 2, 2};
static const uint16_t kind_points[3] = {30, 20, 10};
static const uint16_t saucer_points[4] = {50, 100, 150, 300};
static const uint16_t *const alien_sprites[3][2] = {
    {AlienA1, AlienA2}, {AlienB1, AlienB2}, {AlienC1, AlienC2}};
static const uint16_t *const digit_sprites[10] = {
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9};

// Todo en cero = negro. Cabe el recuadro mas grande que se dibuja.
static const uint16_t black_pixels[SCREEN_SIZE * OVERLAY_H];

// La nave y el icono de vida del jugador 2 son copias pintadas de otro color,
// y la flecha del menu se recorta de MainMenuV2.
static uint16_t p2_ship[SHIP_W * SHIP_H];
static uint16_t p2_life[LIFE_W * LIFE_H];
static uint16_t menu_cursor[CURSOR_W * CURSOR_H];
static const uint16_t *const ship_sprites[2] = {Player, p2_ship};
static const uint16_t *const life_sprites[2] = {LifeIcon, p2_life};

static void recolor(uint16_t *dst, const uint16_t *src, int count, uint16_t from,
                    uint16_t to) {
  for (int i = 0; i < count; ++i)
    dst[i] = src[i] == from ? to : src[i];
}

static void prepareSprites(void) {
  recolor(p2_ship, Player, SHIP_W * SHIP_H, Player[3], P2_COLOR); // [3] = punta
  recolor(p2_life, LifeIcon, LIFE_W * LIFE_H, LifeIcon[2], P2_COLOR);
  for (int row = 0; row < CURSOR_H; ++row)
    for (int col = 0; col < CURSOR_W; ++col) {
      uint16_t pixel =
          MainMenuV2[(CURSOR_Y + row) * SCREEN_SIZE + CURSOR_X + col];
      menu_cursor[row * CURSOR_W + col] = pixel ? pixel : TRANSPARENT;
    }
}

// En el simulador el teclado da dos controles. En la FPGA solo hay lectura del
// primero hasta que se documente el segundo control NES.
static int connectedControls(void) {
#ifndef FPGA
  return 2;
#else
  return 1;
#endif
}

// Punto de enganche con el menu general de KUBIX: menu/ ejecuta este juego
// como programa aparte, asi que volver al menu general termina el programa.
static void backToGeneralMenu(int *running) { *running = 0; }

static uint32_t nextRandom(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static uint32_t randomBelow(Game *g, uint32_t n) {
  return nextRandom(&g->rng) % n;
}

static int overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw,
                    int bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static int shipActive(const Game *g, int p) {
  return p < g->players && g->ships[p].lives > 0;
}

static int shieldX(int s) { return SHIELD_FIRST_X + s * SHIELD_STEP; }

// ------------------------------------------------------------------ tiempos

// La formacion acelera a medida que pierde aliens y en cada oleada.
static uint16_t marchInterval(const Game *g) {
  int speedup = g->wave > 5 ? 4 : g->wave - 1;
  int ticks = MARCH_MIN_TICKS + g->aliens.alive_count - speedup;
  return (uint16_t)(ticks < MARCH_MIN_TICKS ? MARCH_MIN_TICKS : ticks);
}

static uint16_t fireDelay(Game *g) {
  int slowest = 70 - 8 * (g->wave > 6 ? 5 : g->wave - 1);
  return (uint16_t)(slowest / 3 + (int)randomBelow(g, (uint32_t)slowest));
}

static uint16_t saucerDelay(Game *g) {
  return (uint16_t)(900 + randomBelow(g, 600));
}

// ------------------------------------------------------------------ partida

static void addExplosion(Game *g, int x, int y, uint16_t points) {
  for (int i = 0; i < EXPLOSIONS; ++i)
    if (g->explosions[i].timer == 0) {
      g->explosions[i] = (Explosion){
          (int16_t)x, (int16_t)y, points,
          (uint8_t)(points ? POINTS_POPUP_TICKS : ALIEN_EXPLOSION_TICKS)};
      return;
    }
}

static void addScore(Ship *s, uint16_t points) {
  s->score = s->score + points > MAX_SCORE ? MAX_SCORE
                                           : (uint16_t)(s->score + points);
}

static void startWave(Game *g) {
  Formation *f = &g->aliens;
  int lower = g->wave > 4 ? 3 : g->wave - 1; // cada oleada empieza mas abajo
  f->x = FORMATION_X;
  f->y = (int16_t)(FORMATION_Y + lower * 2);
  f->dir = 1;
  f->frame = 0;
  memset(f->alive, 1, sizeof(f->alive));
  f->alive_count = ALIEN_ROWS * ALIEN_COLS;
  f->march_timer = marchInterval(g);

  for (int s = 0; s < SHIELD_COUNT; ++s)
    memcpy(g->shields[s], Bunker, sizeof(g->shields[s]));
  for (int p = 0; p < 2; ++p) {
    g->ships[p].x = ship_start_x[g->players - 1][p];
    g->ships[p].shot.active = 0;
  }
  memset(g->alien_shots, 0, sizeof(g->alien_shots));
  memset(g->explosions, 0, sizeof(g->explosions));
  g->saucer.active = 0;
  g->saucer.spawn_timer = saucerDelay(g);
  g->alien_fire_timer = fireDelay(g);
}

static void initGame(Game *g, uint8_t players, uint32_t seed) {
  memset(g, 0, sizeof(*g));
  g->players = players;
  g->rng = seed | 1; // xorshift no sale nunca del cero
  g->wave = 1;
  for (int p = 0; p < players; ++p)
    g->ships[p].lives = START_LIVES;
  startWave(g);
  g->phase = PHASE_PLAYING;
}

// Rompe la barrera alrededor del punto del impacto (coordenadas locales).
static void damageShield(Game *g, int s, int lx, int ly) {
  static const int8_t crater[5][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  for (int i = 0; i < 7; ++i) {
    int dx = i < 5 ? crater[i][0] : (int)randomBelow(g, 5) - 2;
    int dy = i < 5 ? crater[i][1] : (int)randomBelow(g, 3) - 1;
    int x = lx + dx, y = ly + dy;
    if (x >= 0 && x < SHIELD_W && y >= 0 && y < SHIELD_H)
      g->shields[s][y * SHIELD_W + x] = TRANSPARENT;
  }
}

// Devuelve 1 si el punto toca una parte sana de alguna barrera y la rompe.
static int hitShield(Game *g, int x, int y) {
  if (y < SHIELD_Y || y >= SHIELD_Y + SHIELD_H)
    return 0;
  for (int s = 0; s < SHIELD_COUNT; ++s) {
    int lx = x - shieldX(s), ly = y - SHIELD_Y;
    if (lx < 0 || lx >= SHIELD_W)
      continue;
    if (g->shields[s][ly * SHIELD_W + lx] == TRANSPARENT)
      return 0;
    damageShield(g, s, lx, ly);
    return 1;
  }
  return 0;
}

// Los aliens que bajan hasta las barreras se las van comiendo.
static void aliensEatShields(Game *g) {
  const Formation *f = &g->aliens;
  for (int r = 0; r < ALIEN_ROWS; ++r)
    for (int c = 0; c < ALIEN_COLS; ++c) {
      if (!f->alive[r][c])
        continue;
      int ax = f->x + c * ALIEN_PITCH_X, ay = f->y + r * ALIEN_PITCH_Y;
      for (int s = 0; s < SHIELD_COUNT; ++s) {
        if (!overlaps(ax, ay, ALIEN_W, ALIEN_H, shieldX(s), SHIELD_Y, SHIELD_W,
                      SHIELD_H))
          continue;
        for (int y = 0; y < SHIELD_H; ++y)
          for (int x = 0; x < SHIELD_W; ++x)
            if (overlaps(ax, ay, ALIEN_W, ALIEN_H, shieldX(s) + x,
                         SHIELD_Y + y, 1, 1))
              g->shields[s][y * SHIELD_W + x] = TRANSPARENT;
      }
    }
}

static uint16_t hitAlien(Game *g, int x, int y) {
  Formation *f = &g->aliens;
  int dx = x - f->x, dy = y - f->y;
  if (dx < 0 || dy < 0)
    return 0;
  int c = dx / ALIEN_PITCH_X, r = dy / ALIEN_PITCH_Y;
  if (c >= ALIEN_COLS || r >= ALIEN_ROWS || dx % ALIEN_PITCH_X >= ALIEN_W ||
      dy % ALIEN_PITCH_Y >= ALIEN_H || !f->alive[r][c])
    return 0;
  f->alive[r][c] = 0;
  --f->alive_count;
  addExplosion(g, f->x + c * ALIEN_PITCH_X, f->y + r * ALIEN_PITCH_Y, 0);
  return kind_points[row_kind[r]];
}

static uint16_t hitSaucer(Game *g, int x, int y) {
  Saucer *u = &g->saucer;
  if (!u->active ||
      !overlaps(x, y, 1, 1, u->x, SAUCER_Y, SAUCER_W, SAUCER_H))
    return 0;
  u->active = 0;
  u->spawn_timer = saucerDelay(g);
  addExplosion(g, u->x, SAUCER_Y, u->points);
  return u->points;
}

static void moveShipShot(Game *g, Ship *s) {
  Shot *shot = &s->shot;
  --shot->y;
  if (shot->y < HUD_H) {
    shot->active = 0;
    return;
  }
  uint16_t points = hitAlien(g, shot->x, shot->y);
  if (!points)
    points = hitSaucer(g, shot->x, shot->y);
  if (points) {
    addScore(s, points);
    shot->active = 0;
    return;
  }
  if (hitShield(g, shot->x, shot->y)) {
    shot->active = 0;
    return;
  }
  for (int i = 0; i < ALIEN_SHOTS; ++i) {
    Shot *a = &g->alien_shots[i];
    if (a->active && overlaps(shot->x, shot->y, 1, SHOT_H, a->x, a->y,
                              ALIEN_SHOT_W, ALIEN_SHOT_H)) {
      a->active = 0;
      shot->active = 0;
      return;
    }
  }
}

static void updateShip(Game *g, int p, uint8_t controls) {
  Ship *s = &g->ships[p];
  if (!shipActive(g, p))
    return;
  int left = (controls & NES_BIT_LEFT) != 0;
  int right = (controls & NES_BIT_RIGHT) != 0;
  if (left && !right && s->x > SHIP_MIN_X)
    --s->x;
  if (right && !left && s->x < SHIP_MAX_X)
    ++s->x;
  // Un disparo a la vez; mantener el boton dispara apenas se libera.
  if ((controls & (NES_BIT_A | NES_BIT_UP)) && !s->shot.active)
    s->shot = (Shot){(int16_t)(s->x + SHIP_W / 2), SHIP_Y - SHOT_H, 1};
  for (int i = 0; i < SHOT_SPEED && s->shot.active; ++i)
    moveShipShot(g, s);
}

// Avanza la formacion un pixel; al tocar un borde baja y cambia de sentido.
static void marchFormation(Game *g) {
  Formation *f = &g->aliens;
  int first = ALIEN_COLS, last = -1;
  for (int c = 0; c < ALIEN_COLS; ++c)
    for (int r = 0; r < ALIEN_ROWS; ++r)
      if (f->alive[r][c]) {
        if (c < first)
          first = c;
        if (c > last)
          last = c;
      }
  int left = f->x + first * ALIEN_PITCH_X;
  int right = f->x + last * ALIEN_PITCH_X + ALIEN_W;
  if ((f->dir > 0 && right >= SCREEN_SIZE - 1) || (f->dir < 0 && left <= 1)) {
    f->y += FORMATION_DROP;
    f->dir = (int8_t)-f->dir;
  } else {
    f->x += f->dir;
  }
  f->frame ^= 1;
  aliensEatShields(g);
}

static int aliensLanded(const Game *g) {
  const Formation *f = &g->aliens;
  for (int r = ALIEN_ROWS - 1; r >= 0; --r)
    for (int c = 0; c < ALIEN_COLS; ++c)
      if (f->alive[r][c])
        return f->y + r * ALIEN_PITCH_Y + ALIEN_H > SHIP_Y;
  return 0;
}

// Dispara desde el alien mas bajo de una columna; a veces apunta a una nave.
static void updateAlienFire(Game *g) {
  if (--g->alien_fire_timer > 0)
    return;
  g->alien_fire_timer = fireDelay(g);
  int slot = -1;
  for (int i = 0; i < ALIEN_SHOTS; ++i)
    if (!g->alien_shots[i].active)
      slot = i;
  if (slot < 0)
    return;

  const Formation *f = &g->aliens;
  int columns[ALIEN_COLS], count = 0, aimed = -1;
  for (int c = 0; c < ALIEN_COLS; ++c)
    for (int r = 0; r < ALIEN_ROWS; ++r)
      if (f->alive[r][c]) {
        columns[count++] = c;
        break;
      }
  if (count == 0)
    return;
  int target = (int)randomBelow(g, (uint32_t)g->players);
  if (!shipActive(g, target))
    target = 1 - target;
  if (randomBelow(g, 3) == 0 && shipActive(g, target)) {
    int c = (g->ships[target].x + SHIP_W / 2 - f->x) / ALIEN_PITCH_X;
    for (int i = 0; i < count; ++i)
      if (columns[i] == c)
        aimed = c;
  }
  int c = aimed >= 0 ? aimed : columns[randomBelow(g, (uint32_t)count)];
  int r = ALIEN_ROWS - 1;
  while (!f->alive[r][c])
    --r;
  g->alien_shots[slot] =
      (Shot){(int16_t)(f->x + c * ALIEN_PITCH_X + ALIEN_W / 2 - 1),
             (int16_t)(f->y + r * ALIEN_PITCH_Y + ALIEN_H), 1};
}

static void killShip(Game *g, int p) {
  --g->ships[p].lives;
  g->ships[p].shot.active = 0;
  memset(g->alien_shots, 0, sizeof(g->alien_shots));
  g->dying_ship = (uint8_t)p;
  g->phase = PHASE_DYING;
  g->phase_timer = DYING_TICKS;
}

static void updateAlienShots(Game *g) {
  if (++g->alien_shot_timer < (g->wave == 1 ? 3 : 2))
    return;
  g->alien_shot_timer = 0;
  for (int i = 0; i < ALIEN_SHOTS; ++i) {
    Shot *a = &g->alien_shots[i];
    if (!a->active)
      continue;
    ++a->y;
    int bottom = a->y + ALIEN_SHOT_H - 1;
    if (bottom >= GROUND_Y || hitShield(g, a->x, bottom) ||
        hitShield(g, a->x + 1, bottom)) {
      a->active = 0;
      continue;
    }
    for (int p = 0; p < g->players; ++p)
      if (shipActive(g, p) &&
          overlaps(a->x, a->y, ALIEN_SHOT_W, ALIEN_SHOT_H, g->ships[p].x + 1,
                   SHIP_Y + 1, SHIP_W - 2, SHIP_H - 1)) {
        killShip(g, p);
        return;
      }
  }
}

static void updateSaucer(Game *g) {
  Saucer *u = &g->saucer;
  if (!u->active) {
    if (--u->spawn_timer == 0) {
      u->active = 1;
      u->dir = randomBelow(g, 2) ? 1 : -1;
      u->x = (int16_t)(u->dir > 0 ? -SAUCER_W : SCREEN_SIZE);
      u->points = saucer_points[randomBelow(g, 4)];
      u->move_timer = 0;
    }
    return;
  }
  if (++u->move_timer < SAUCER_MOVE_TICKS)
    return;
  u->move_timer = 0;
  u->x += u->dir;
  if (u->x < -SAUCER_W || u->x > SCREEN_SIZE) {
    u->active = 0;
    u->spawn_timer = saucerDelay(g);
  }
}

// Avanza un paso de logica. Devuelve 1 cuando la partida termina.
static int tickGame(Game *g, const uint8_t controls[2]) {
  ++g->ticks;
  for (int i = 0; i < EXPLOSIONS; ++i)
    if (g->explosions[i].timer)
      --g->explosions[i].timer;

  if (g->phase == PHASE_WAVE_INTRO) {
    if (--g->phase_timer == 0)
      g->phase = PHASE_PLAYING;
    return 0;
  }
  if (g->phase == PHASE_DYING) {
    if (--g->phase_timer > 0)
      return 0;
    g->ships[g->dying_ship].x = ship_start_x[g->players - 1][g->dying_ship];
    g->phase = PHASE_PLAYING;
    return !shipActive(g, 0) && !shipActive(g, 1);
  }

  for (int p = 0; p < g->players; ++p)
    updateShip(g, p, controls[p]);
  if (--g->aliens.march_timer == 0) {
    marchFormation(g);
    if (aliensLanded(g))
      return 1;
    g->aliens.march_timer = marchInterval(g);
  }
  updateAlienFire(g);
  updateAlienShots(g);
  updateSaucer(g);

  // Si una nave acaba de caer, la oleada nueva espera a que termine la explosion.
  if (g->aliens.alive_count == 0 && g->phase == PHASE_PLAYING) {
    ++g->wave;
    startWave(g);
    g->phase = PHASE_WAVE_INTRO;
    g->phase_timer = WAVE_INTRO_TICKS;
  }
  return 0;
}

// ------------------------------------------------------------------ dibujo

static void drawText(const char *text, uint8_t x, uint8_t y) {
  sendString((char *)text, x, y, CHAR_W, CHAR_H);
}

static void drawCentered(const char *text, uint8_t y) {
  int width = (int)strlen(text) * (CHAR_W + 1) - 1;
  drawText(text, (uint8_t)((SCREEN_SIZE - width) / 2), y);
}

// Texto en la fuente 5x7 (6 px por caracter), para lo que la 3x5 no resuelve.
static void drawBig(const char *text, uint8_t y) {
  int width = (int)strlen(text) * (BIG_W + 1) - 1;
  sendString((char *)text, (uint8_t)((SCREEN_SIZE - width) / 2), y, BIG_W,
             BIG_H);
}

static int numberDigits(uint16_t value) {
  int count = 1;
  while (value >= 10) {
    value /= 10;
    ++count;
  }
  return count;
}

// Numero con los digitos Num0..Num9, rellenado con ceros hasta min_digits.
static void drawNumber(uint16_t value, int min_digits, int x, int y) {
  int count = numberDigits(value);
  if (count < min_digits)
    count = min_digits;
  for (int i = count - 1; i >= 0; --i) {
    sendSprite(digit_sprites[value % 10], (uint8_t)(x + i * (DIGIT_W + 1)),
               (uint8_t)y, DIGIT_W, DIGIT_H);
    value /= 10;
  }
}

// sendSprite no recorta: el ovni entra y sale por los costados.
static void drawClipped(const uint16_t *sprite, int x, int y, int w, int h) {
  uint16_t visible[SAUCER_W * SAUCER_H];
  int from = x < 0 ? -x : 0;
  int to = x + w > SCREEN_SIZE ? SCREEN_SIZE - x : w;
  if (to <= from)
    return;
  for (int row = 0; row < h; ++row)
    for (int col = from; col < to; ++col)
      visible[row * (to - from) + col - from] = sprite[row * w + col];
  sendSprite(visible, (uint8_t)(x + from), (uint8_t)y, (uint8_t)(to - from),
             (uint8_t)h);
}

static void drawHud(const Game *g) {
  if (g->players == 1) {
    drawNumber(g->ships[0].score, 4, 1, 1);
    for (int i = 0; i < g->ships[0].lives && i < START_LIVES; ++i)
      sendSprite(LifeIcon, (uint8_t)(58 - i * 6), 2, LIFE_W, LIFE_H);
    return;
  }
  drawNumber(g->ships[0].score, 4, 1, 1);
  sendSprite(life_sprites[0], 18, 2, LIFE_W, LIFE_H);
  drawNumber(g->ships[0].lives, 1, 24, 1);
  drawNumber(g->ships[1].lives, 1, 37, 1);
  sendSprite(life_sprites[1], 41, 2, LIFE_W, LIFE_H);
  drawNumber(g->ships[1].score, 4, 48, 1);
}

static void drawGame(const Game *g) {
  const Formation *f = &g->aliens;
  sendBackground(Escenario);
  drawHud(g);

  for (int s = 0; s < SHIELD_COUNT; ++s)
    sendSprite(g->shields[s], (uint8_t)shieldX(s), SHIELD_Y, SHIELD_W,
               SHIELD_H);
  for (int r = 0; r < ALIEN_ROWS; ++r)
    for (int c = 0; c < ALIEN_COLS; ++c)
      if (f->alive[r][c])
        sendSprite(alien_sprites[row_kind[r]][f->frame],
                   (uint8_t)(f->x + c * ALIEN_PITCH_X),
                   (uint8_t)(f->y + r * ALIEN_PITCH_Y), ALIEN_W, ALIEN_H);
  if (g->saucer.active)
    drawClipped(Ufo, g->saucer.x, SAUCER_Y, SAUCER_W, SAUCER_H);

  for (int i = 0; i < EXPLOSIONS; ++i) {
    const Explosion *e = &g->explosions[i];
    if (!e->timer)
      continue;
    if (e->points) {
      int width = numberDigits(e->points) * (DIGIT_W + 1) - 1;
      int x = e->x < 0 ? 0 : e->x;
      if (x > SCREEN_SIZE - width)
        x = SCREEN_SIZE - width;
      drawNumber(e->points, 1, x, e->y);
    } else {
      sendSprite(AlienExplosion, (uint8_t)e->x, (uint8_t)e->y, ALIEN_W,
                 ALIEN_H);
    }
  }

  for (int p = 0; p < g->players; ++p) {
    const Ship *s = &g->ships[p];
    if (g->phase == PHASE_DYING && g->dying_ship == p) {
      const uint16_t *boom =
          (g->ticks / 6) % 2 ? PlayerExplosion2 : PlayerExplosion1;
      sendSprite(boom, (uint8_t)s->x, SHIP_Y, SHIP_W, SHIP_H);
    } else if (s->lives > 0) {
      sendSprite(ship_sprites[p], (uint8_t)s->x, SHIP_Y, SHIP_W, SHIP_H);
    }
    if (s->shot.active)
      sendSprite(PlayerBullet, (uint8_t)s->shot.x, (uint8_t)s->shot.y, 1,
                 SHOT_H);
  }
  const uint16_t *alien_shot = (g->ticks / 4) % 2 ? AlienBullet2 : AlienBullet1;
  for (int i = 0; i < ALIEN_SHOTS; ++i)
    if (g->alien_shots[i].active)
      sendSprite(alien_shot, (uint8_t)g->alien_shots[i].x,
                 (uint8_t)g->alien_shots[i].y, ALIEN_SHOT_W, ALIEN_SHOT_H);

  if (g->phase == PHASE_WAVE_INTRO) {
    char text[12];
    snprintf(text, sizeof(text), "WAVE %u", (unsigned)g->wave);
    sendSprite(black_pixels, 0, 25, SCREEN_SIZE, 13);
    drawBig(text, 28);
  }
}

static void drawReady(void) {
  sendSprite(black_pixels, 6, 24, SCREEN_SIZE - 12, OVERLAY_H);
  drawCentered("PRESS START", 27);
  drawCentered("B: EXIT", 34);
}

static void drawGameOver(const Game *g) {
  sendBackground(GameOverMenu);
  if (g->players == 1) {
    drawNumber(g->ships[0].score, 4, 24, SCORE_BAND_Y);
    return;
  }
  sendSprite(life_sprites[0], 8, SCORE_BAND_Y + 1, LIFE_W, LIFE_H);
  drawNumber(g->ships[0].score, 4, 14, SCORE_BAND_Y);
  sendSprite(life_sprites[1], 35, SCORE_BAND_Y + 1, LIFE_W, LIFE_H);
  drawNumber(g->ships[1].score, 4, 41, SCORE_BAND_Y);
}

static void drawMenu(int selected) {
  sendBackground(MainMenuV2);
  // El fondo trae la flecha dibujada en la primera opcion: se tapa y se
  // dibuja en la opcion elegida.
  sendSprite(black_pixels, CURSOR_X, CURSOR_Y, CURSOR_W, CURSOR_H);
  sendSprite(menu_cursor, CURSOR_X, (uint8_t)(CURSOR_Y + selected * CURSOR_STEP),
             CURSOR_W, CURSOR_H);
}

static void drawMessage(const char *const lines[2]) {
  drawBig(lines[0], 20);
  drawBig(lines[1], 30);
  drawCentered("B: BACK", 52);
}

int main(int argc, char *argv[]) {
  if (initScreen(argc, argv) != 0)
    return 1;
  prepareSprites();

  static Game game;
  static const char *const multiplayer_message[2] = {"COMING", "SOON"};
  static const char *const controls_message[2] = {"NEED 2", "CONTROLS"};
  const char *const *message = multiplayer_message;

  int running = 1;
  AppScreen current_screen = SCREEN_STARTMENU;
  int menu_selected = MENU_ONE_PLAYER;
  int gameover_wait = 0;
  uint32_t tick_accumulator = 0;
  uint8_t previous_p1 = 0;
  uint32_t last_frame_ticks = SDL_GetTicks();

  while (running) {
    uint32_t frame_ticks = SDL_GetTicks();
    uint32_t frame_delta = frame_ticks - last_frame_ticks;
    last_frame_ticks = frame_ticks;
    uint8_t p1 = readControls(&running, &default_keys);
    uint8_t p2 = readControls(&running, &player2_keys);
    // En los menus solo cuenta el flanco de subida: mantener un boton no lo
    // repite. En la partida se usa el estado mantenido.
    uint8_t pressed = (uint8_t)(p1 & ~previous_p1);
    previous_p1 = p1;
    int confirm = (pressed & (NES_BIT_A | NES_BIT_START)) != 0;
    int back = (pressed & NES_BIT_B) != 0;

    switch (current_screen) {
    case SCREEN_STARTMENU:
      if ((pressed & NES_BIT_UP) && menu_selected > 0)
        --menu_selected;
      if ((pressed & NES_BIT_DOWN) && menu_selected < MENU_COUNT - 1)
        ++menu_selected;
      if (back) {
        backToGeneralMenu(&running);
      } else if (confirm) {
        if (menu_selected == MENU_MULTIPLAYER) {
          message = multiplayer_message;
          current_screen = SCREEN_MESSAGE;
        } else if (menu_selected == MENU_TWO_PLAYERS &&
                   connectedControls() < 2) {
          message = controls_message;
          current_screen = SCREEN_MESSAGE;
        } else {
          initGame(&game, menu_selected == MENU_TWO_PLAYERS ? 2 : 1,
                   frame_ticks);
          current_screen = SCREEN_READY;
        }
      }
      break;

    case SCREEN_MESSAGE:
      if (back || confirm)
        current_screen = SCREEN_STARTMENU;
      break;

    case SCREEN_READY:
      if (back) {
        current_screen = SCREEN_STARTMENU;
      } else if (pressed & NES_BIT_START) {
        tick_accumulator = 0;
        current_screen = SCREEN_GAME;
      }
      break;

    case SCREEN_GAME: {
      if (pressed & NES_BIT_START) {
        current_screen = SCREEN_PAUSE;
        break;
      }
      const uint8_t controls[2] = {p1, p2};
      tick_accumulator += frame_delta;
      if (tick_accumulator > MAX_TICKS_PER_FRAME * TICK_MS)
        tick_accumulator = MAX_TICKS_PER_FRAME * TICK_MS;
      while (tick_accumulator >= TICK_MS) {
        tick_accumulator -= TICK_MS;
        if (tickGame(&game, controls)) {
          gameover_wait = GAMEOVER_WAIT;
          current_screen = SCREEN_GAMEOVER;
          break;
        }
      }
      break;
    }

    case SCREEN_PAUSE:
      if (confirm)
        current_screen = SCREEN_GAME;
      else if (back)
        current_screen = SCREEN_STARTMENU;
      break;

    case SCREEN_GAMEOVER:
      // Un momento sin aceptar botones para que se vea el final y no se salte
      // el resultado si el jugador seguia disparando con A.
      if (gameover_wait > 0) {
        --gameover_wait;
      } else if (confirm) {
        initGame(&game, game.players, frame_ticks);
        current_screen = SCREEN_READY;
      } else if (back) {
        current_screen = SCREEN_STARTMENU;
      }
      break;
    }

    SDL_SetRenderDrawColor(global_renderer, 0, 0, 0, 255);
    SDL_RenderClear(global_renderer);

    switch (current_screen) {
    case SCREEN_STARTMENU:
      drawMenu(menu_selected);
      break;
    case SCREEN_MESSAGE:
      drawMessage(message);
      break;
    case SCREEN_READY:
      drawGame(&game);
      drawReady();
      break;
    case SCREEN_GAME:
      drawGame(&game);
      break;
    case SCREEN_PAUSE:
      sendBackground(PauseMenu);
      break;
    case SCREEN_GAMEOVER:
      if (gameover_wait > 0)
        drawGame(&game);
      else
        drawGameOver(&game);
      break;
    }

    SDL_RenderPresent(global_renderer);
    SDL_Delay(FRAME_MS);
  }

  SDL_Quit();
  return 0;
}
