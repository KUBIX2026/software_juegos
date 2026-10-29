// Breakout para KUBIX.
// Flujo de pantallas segun docs/diagramas/Diagrama logica juegos.png:
// menu -> "Press start" -> partida por muros -> pausa / fin de partida.
// La logica avanza en pasos fijos de TICK_MS. Las posiciones siguen la maqueta
// breakout/assets/BreakoutPlaying.png.

#include <stdlib.h>
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

typedef enum {
  PHASE_TURN_INTRO,  // "PLAYER n" al empezar el turno de cada jugador
  PHASE_SERVE,       // pelota sobre la paleta hasta que se lanza
  PHASE_PLAYING,
  PHASE_LOST,        // pelota perdida, un momento antes de seguir
  PHASE_LEVEL_INTRO  // "LEVEL n" con el muro nuevo
} GamePhase;

enum {
  SCREEN_SIZE = 64,
  TICK_MS = 16,
  MAX_TICKS_PER_FRAME = 4,
  FRAME_MS = 16,
  SUB = 256, // subpixeles por pixel
  CEILING_Y = 6, // debajo del marcador
  BLOCK_ROWS = 6,
  BLOCK_COLS = 6,
  BLOCK_COUNT = BLOCK_ROWS * BLOCK_COLS,
  BLOCK_W = 7,
  BLOCK_H = 2,
  BLOCK_X = 6,
  BLOCK_Y = 7,
  BLOCK_PITCH_X = 9,
  BLOCK_PITCH_Y = 3,
  PADDLE_W = 12,
  PADDLE_H = 2,
  PADDLE_Y = 60,
  PADDLE_SPEED = 384, // subpixeles por paso
  BALL_SIZE = 2,
  START_LIVES = 3,
  SPEED_START = 128,  // subpixeles por paso (0,5 px): empieza lenta
  SPEED_PER_HIT = 4,  // cada bloque roto la acelera un poco
  SPEED_PER_LEVEL = 16,
  SPEED_MAX = 384, // 1,5 px por paso: no alcanza a saltarse un bloque
  LOST_TICKS = 60,
  INTRO_TICKS = 90,
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
  OVERLAY_Y = 30, // los avisos van entre el muro y la paleta
  OVERLAY_H = 17
};

static const uint16_t TRANSPARENT = 0x0843;

typedef struct {
  int32_t x, y; // subpixeles
  int8_t sx, sy; // sentido en cada eje
  uint8_t angle; // indice de bounce_dir
} BallState;

typedef struct {
  uint16_t score;
  uint8_t lives;
  uint16_t level;
  uint8_t alive[BLOCK_ROWS][BLOCK_COLS];
  uint8_t blocks_left;
  uint16_t speed; // subpixeles por paso
} Player;

typedef struct {
  uint8_t players;
  uint8_t current; // jugador que tiene el turno
  Player player[2];
  BallState ball;
  int32_t paddle_x; // subpixeles
  GamePhase phase;
  uint16_t phase_timer;
  uint8_t held[2]; // botones del paso anterior, para lanzar con un toque nuevo
  uint16_t ticks;
  uint32_t rng;
} Game;

// Segundo control: J/L para moverse, K o I para lanzar.
static struct keys player2_keys = {
    .UP = SDLK_i, .LEFT = SDLK_j, .RIGHT = SDLK_l, .A = SDLK_k};

// Filas de arriba abajo, como en la maqueta.
static const uint16_t *const block_sprites[BLOCK_ROWS] = {
    RedBlock, OrangeBlock, YellowBlock, GreenBlock, BlueBlock, PurpleBlock};
static const uint16_t row_points[BLOCK_ROWS] = {60, 50, 40, 30, 20, 10};
// Direccion de la pelota por cada 256 de velocidad {x, y}: 15, 30, 45 y 60
// grados respecto de la vertical.
static const int16_t bounce_dir[4][2] = {
    {66, 247}, {128, 222}, {181, 181}, {222, 128}};
static const uint16_t *const paddle_sprites[2] = {PlayerBT, PlayerBT2};
static const uint16_t *const life_sprites[2] = {LifeIcon, LifeIcon2};
static const uint16_t *const digit_sprites[10] = {
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9};

// Todo en cero = negro. Cabe el recuadro mas grande que se dibuja.
static const uint16_t black_pixels[SCREEN_SIZE * OVERLAY_H];

// La flecha del menu se recorta de MainMenuV2.
static uint16_t menu_cursor[CURSOR_W * CURSOR_H];

static void prepareSprites(void) {
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

static int overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw,
                    int bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// ------------------------------------------------------------------ partida

static void resetWall(Player *pl) {
  memset(pl->alive, 1, sizeof(pl->alive));
  pl->blocks_left = BLOCK_COUNT;
  int speed = SPEED_START + (pl->level - 1) * SPEED_PER_LEVEL;
  pl->speed = (uint16_t)(speed > SPEED_MAX ? SPEED_MAX : speed);
}

static void ballOnPaddle(Game *g) {
  g->ball.x = g->paddle_x + (PADDLE_W - BALL_SIZE) / 2 * SUB;
  g->ball.y = (PADDLE_Y - BALL_SIZE) * SUB;
}

static void centerPaddle(Game *g) {
  g->paddle_x = (SCREEN_SIZE - PADDLE_W) / 2 * SUB;
  ballOnPaddle(g);
}

static void initGame(Game *g, uint8_t players, uint32_t seed) {
  memset(g, 0, sizeof(*g));
  g->players = players;
  g->rng = seed | 1; // xorshift no sale nunca del cero
  for (int p = 0; p < players; ++p) {
    g->player[p].lives = START_LIVES;
    g->player[p].level = 1;
    resetWall(&g->player[p]);
  }
  centerPaddle(g);
  g->phase = players == 2 ? PHASE_TURN_INTRO : PHASE_SERVE;
  g->phase_timer = INTRO_TICKS;
}

static void speedUp(Player *pl) {
  int speed = pl->speed + SPEED_PER_HIT;
  pl->speed = (uint16_t)(speed > SPEED_MAX ? SPEED_MAX : speed);
}

static void addScore(Player *pl, uint16_t points) {
  pl->score = pl->score + points > MAX_SCORE ? MAX_SCORE
                                             : (uint16_t)(pl->score + points);
}

// Cada bloque roto acelera un poco la pelota: empieza lenta y se va poniendo
// rapida a medida que se rompe el muro, sin saltos de velocidad.
static void hitBlock(Player *pl, int r, int c) {
  pl->alive[r][c] = 0;
  --pl->blocks_left;
  addScore(pl, row_points[r]);
  speedUp(pl);
}

// Cada bloque ocupa su casilla completa (9x3, con el espacio que lo rodea),
// asi la pelota no se cuela entre dos bloques vecinos. A los costados del
// muro queda un pasillo por donde la pelota puede subir.
static int findBlock(const Player *pl, int bx, int by, int *row, int *col) {
  for (int r = 0; r < BLOCK_ROWS; ++r)
    for (int c = 0; c < BLOCK_COLS; ++c)
      if (pl->alive[r][c] &&
          overlaps(bx, by, BALL_SIZE, BALL_SIZE,
                   BLOCK_X - 1 + c * BLOCK_PITCH_X, BLOCK_Y + r * BLOCK_PITCH_Y,
                   BLOCK_PITCH_X, BLOCK_PITCH_Y)) {
        *row = r;
        *col = c;
        return 1;
      }
  return 0;
}

static void movePaddle(Game *g, uint8_t controls) {
  int left = (controls & NES_BIT_LEFT) != 0;
  int right = (controls & NES_BIT_RIGHT) != 0;
  if (left && !right)
    g->paddle_x -= PADDLE_SPEED;
  if (right && !left)
    g->paddle_x += PADDLE_SPEED;
  if (g->paddle_x < 0)
    g->paddle_x = 0;
  if (g->paddle_x > (SCREEN_SIZE - PADDLE_W) * SUB)
    g->paddle_x = (SCREEN_SIZE - PADDLE_W) * SUB;
}

// El angulo depende de donde pega en la paleta: al centro sale casi vertical
// y en las puntas, abierta.
static void bounceOnPaddle(Game *g) {
  BallState *b = &g->ball;
  int off = b->x / SUB + BALL_SIZE / 2 - (g->paddle_x / SUB + PADDLE_W / 2);
  int mag = abs(off);
  b->angle = (uint8_t)(mag <= 1 ? 0 : mag <= 3 ? 1 : mag <= 5 ? 2 : 3);
  if (off != 0)
    b->sx = (int8_t)(off > 0 ? 1 : -1);
  b->sy = -1;
  b->y = (PADDLE_Y - BALL_SIZE) * SUB;
}

// Mueve la pelota un paso, primero en x y luego en y, para saber de que lado
// pego. Devuelve 1 si la pelota se perdio por abajo.
static int moveBall(Game *g) {
  BallState *b = &g->ball;
  Player *pl = &g->player[g->current];
  int vx = b->sx * bounce_dir[b->angle][0] * pl->speed / 256;
  int vy = b->sy * bounce_dir[b->angle][1] * pl->speed / 256;
  const int32_t max_x = (SCREEN_SIZE - BALL_SIZE) * SUB;
  int r, c;

  int32_t before = b->x;
  b->x += vx;
  if (b->x < 0) {
    b->x = -b->x;
    b->sx = 1;
  } else if (b->x > max_x) {
    b->x = 2 * max_x - b->x;
    b->sx = -1;
  }
  if (findBlock(pl, b->x / SUB, b->y / SUB, &r, &c)) {
    hitBlock(pl, r, c);
    b->x = before;
    b->sx = (int8_t)-b->sx;
  }

  before = b->y;
  int old_bottom = b->y / SUB + BALL_SIZE;
  b->y += vy;
  if (b->y < CEILING_Y * SUB) {
    b->y = 2 * CEILING_Y * SUB - b->y;
    b->sy = 1;
  }
  if (findBlock(pl, b->x / SUB, b->y / SUB, &r, &c)) {
    hitBlock(pl, r, c);
    b->y = before;
    b->sy = (int8_t)-b->sy;
  } else if (b->sy > 0) {
    int bx = b->x / SUB, px = g->paddle_x / SUB;
    int bottom = b->y / SUB + BALL_SIZE;
    if (old_bottom <= PADDLE_Y && bottom > PADDLE_Y &&
        overlaps(bx, 0, BALL_SIZE, 1, px, 0, PADDLE_W, 1))
      bounceOnPaddle(g);
  }
  return b->y / SUB >= SCREEN_SIZE;
}

static void launchBall(Game *g) {
  g->ball.angle = 2;
  g->ball.sx = (int8_t)(nextRandom(&g->rng) & 1 ? 1 : -1);
  g->ball.sy = -1;
  g->phase = PHASE_PLAYING;
}

// Tras perder la pelota: en 2 jugadores pasa el turno al otro si le quedan
// vidas. Devuelve 1 cuando nadie tiene vidas.
static int nextTurn(Game *g) {
  int other = 1 - g->current;
  if (g->players == 2 && g->player[other].lives > 0) {
    g->current = (uint8_t)other;
    g->phase = PHASE_TURN_INTRO;
    g->phase_timer = INTRO_TICKS;
  } else if (g->player[g->current].lives > 0) {
    g->phase = PHASE_SERVE;
  } else {
    return 1;
  }
  centerPaddle(g);
  return 0;
}

// Avanza un paso de logica. Devuelve 1 cuando la partida termina.
static int tickGame(Game *g, const uint8_t controls[2]) {
  ++g->ticks;
  Player *pl = &g->player[g->current];
  uint8_t held = controls[g->current];
  uint8_t pressed = (uint8_t)(held & ~g->held[g->current]);
  g->held[0] = controls[0];
  g->held[1] = controls[1];

  switch (g->phase) {
  case PHASE_TURN_INTRO:
  case PHASE_LEVEL_INTRO:
    if (--g->phase_timer == 0)
      g->phase = PHASE_SERVE;
    return 0;

  case PHASE_LOST:
    if (--g->phase_timer > 0)
      return 0;
    return nextTurn(g);

  case PHASE_SERVE:
    movePaddle(g, held);
    ballOnPaddle(g);
    if (pressed & (NES_BIT_A | NES_BIT_UP))
      launchBall(g);
    return 0;

  case PHASE_PLAYING:
    movePaddle(g, held);
    if (moveBall(g)) {
      --pl->lives;
      g->phase = PHASE_LOST;
      g->phase_timer = LOST_TICKS;
    } else if (pl->blocks_left == 0) {
      ++pl->level;
      resetWall(pl);
      ballOnPaddle(g);
      g->phase = PHASE_LEVEL_INTRO;
      g->phase_timer = INTRO_TICKS;
    }
    return 0;
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

static void drawHud(const Game *g) {
  const Player *p1 = &g->player[0];
  if (g->players == 1) {
    drawNumber(p1->score, 4, 1, 1);
    for (int i = 0; i < p1->lives && i < START_LIVES; ++i)
      sendSprite(LifeIcon, (uint8_t)(58 - i * 6), 2, LIFE_W, LIFE_H);
    return;
  }
  const Player *p2 = &g->player[1];
  drawNumber(p1->score, 4, 1, 1);
  sendSprite(life_sprites[0], 18, 2, LIFE_W, LIFE_H);
  drawNumber(p1->lives, 1, 24, 1);
  drawNumber(p2->lives, 1, 37, 1);
  sendSprite(life_sprites[1], 41, 2, LIFE_W, LIFE_H);
  drawNumber(p2->score, 4, 48, 1);
}

static void drawOverlay(const char *text) {
  sendSprite(black_pixels, 0, OVERLAY_Y, SCREEN_SIZE, 13);
  drawBig(text, OVERLAY_Y + 3);
}

// notices = 0 dibuja solo el tablero, para poner encima otro aviso.
static void drawGame(const Game *g, int notices) {
  const Player *pl = &g->player[g->current];
  sendBackground(Escenario);
  drawHud(g);

  for (int r = 0; r < BLOCK_ROWS; ++r)
    for (int c = 0; c < BLOCK_COLS; ++c)
      if (pl->alive[r][c])
        sendSprite(block_sprites[r], (uint8_t)(BLOCK_X + c * BLOCK_PITCH_X),
                   (uint8_t)(BLOCK_Y + r * BLOCK_PITCH_Y), BLOCK_W, BLOCK_H);
  sendSprite(paddle_sprites[g->current], (uint8_t)(g->paddle_x / SUB),
             PADDLE_Y, PADDLE_W, PADDLE_H);
  if (g->phase != PHASE_LOST && g->ball.y / SUB <= SCREEN_SIZE - BALL_SIZE)
    sendSprite(Ball, (uint8_t)(g->ball.x / SUB), (uint8_t)(g->ball.y / SUB),
               BALL_SIZE, BALL_SIZE);

  if (!notices)
    return;
  char text[12];
  switch (g->phase) {
  case PHASE_TURN_INTRO:
    snprintf(text, sizeof(text), "PLAYER %u", (unsigned)g->current + 1);
    drawOverlay(text);
    break;
  case PHASE_LEVEL_INTRO:
    snprintf(text, sizeof(text), "LEVEL %u", (unsigned)pl->level);
    drawOverlay(text);
    break;
  case PHASE_SERVE:
    drawCentered("PRESS A", 44);
    break;
  default:
    break;
  }
}

static void drawReady(void) {
  sendSprite(black_pixels, 0, OVERLAY_Y, SCREEN_SIZE, OVERLAY_H);
  drawCentered("PRESS START", OVERLAY_Y + 3);
  drawCentered("B: EXIT", OVERLAY_Y + 10);
}

static void drawGameOver(const Game *g) {
  sendBackground(GameOverMenu);
  if (g->players == 1) {
    drawNumber(g->player[0].score, 4, 24, SCORE_BAND_Y);
    return;
  }
  sendSprite(life_sprites[0], 8, SCORE_BAND_Y + 1, LIFE_W, LIFE_H);
  drawNumber(g->player[0].score, 4, 14, SCORE_BAND_Y);
  sendSprite(life_sprites[1], 35, SCORE_BAND_Y + 1, LIFE_W, LIFE_H);
  drawNumber(g->player[1].score, 4, 41, SCORE_BAND_Y);
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
      // el resultado si el jugador seguia pulsando A.
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
      drawGame(&game, 0);
      drawReady();
      break;
    case SCREEN_GAME:
      drawGame(&game, 1);
      break;
    case SCREEN_PAUSE:
      sendBackground(PauseMenu);
      break;
    case SCREEN_GAMEOVER:
      if (gameover_wait > 0)
        drawGame(&game, 0);
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
