// Snake para KUBIX.
// Flujo de pantallas segun docs/diagramas/Diagrama logica juegos.png:
// menu -> "Press start" -> partida -> pausa / fin de partida.
// El tablero es de 28x32 casillas de 2x2 px; los 8 px de la derecha son el
// panel lateral con el puntaje de cada jugador. Los bordes se traspasan.

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

// Ordenadas de a pares para que la opuesta de d sea d ^ 1.
typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Direction;

enum {
  SCREEN_SIZE = 64,
  CELL = 2,
  GRID_W = 28,
  GRID_H = 32,
  MAX_LENGTH = GRID_W * GRID_H,
  START_LENGTH = 4,
  TURN_QUEUE = 2,
  STEP_START_MS = 140,
  STEP_MIN_MS = 70,
  STEP_FASTER_MS = 3,
  FRAME_MS = 16,
  GAMEOVER_WAIT = 60, // cuadros mostrando el choque antes del resultado
  SIDEBAR_X = GRID_W * CELL,
  SIDEBAR_W = 8,
  SIDEBAR_H = 32,
  SAMPLE_DIGIT_Y = 13, // digito de muestra que traen los paneles laterales
  SAMPLE_DIGIT_H = 6,
  PANEL_SCORE_Y = 14,
  CURSOR_X = 7,
  CURSOR_Y = 32,
  CURSOR_W = 3,
  CURSOR_H = 5,
  CURSOR_STEP = 9,
  CHAR_W = 3, // fuente pequena 3x5: no dibuja bien la W, la M, la U ni el 7
  CHAR_H = 5,
  BIG_W = 5, // fuente 5x7
  BIG_H = 7,
  OVERLAY_H = 48
};

static const uint16_t TRANSPARENT = 0x0843;
static const uint16_t TEXT_COLOR = 0xFFFF;

typedef struct {
  uint8_t x, y;
} Cell;

typedef struct {
  Cell body[MAX_LENGTH]; // body[0] es la cabeza
  uint16_t length;
  Direction dir;
  Direction turns[TURN_QUEUE];
  uint8_t turn_count;
  uint16_t score;
  uint8_t alive;
  const uint16_t *head_sprite;
  const uint16_t *body_sprite;
} Snake;

typedef struct {
  Snake snakes[2];
  uint8_t players;
  Cell apple;
  uint16_t step_ms;
  uint32_t step_timer;
  uint32_t rng;
  uint8_t board_full;
} Game;

// Segundo control: WASD. Lo no asignado queda en SDLK_UNKNOWN.
static struct keys player2_keys = {
    .UP = SDLK_w, .DOWN = SDLK_s, .LEFT = SDLK_a, .RIGHT = SDLK_d};

// Digitos 3x5 propios (un bit por columna, el bit 2 es la izquierda).
static const uint8_t digit_rows[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};

// Todo en cero = negro. Cabe el recuadro mas grande que se dibuja.
static const uint16_t black_pixels[SIDEBAR_X * OVERLAY_H];

// Copias de los paneles laterales sin el digito de muestra, un panel vacio
// para el modo de un jugador y la flecha del menu recortada de MainMenuV2.
static uint16_t p1_panel[SIDEBAR_W * SIDEBAR_H];
static uint16_t p2_panel[SIDEBAR_W * SIDEBAR_H];
static uint16_t blank_panel[SIDEBAR_W * SIDEBAR_H];
static uint16_t menu_cursor[CURSOR_W * CURSOR_H];

static void preparePanel(uint16_t *panel, const uint16_t *source) {
  memcpy(panel, source, sizeof(uint16_t) * SIDEBAR_W * SIDEBAR_H);
  for (int i = SAMPLE_DIGIT_Y * SIDEBAR_W;
       i < (SAMPLE_DIGIT_Y + SAMPLE_DIGIT_H) * SIDEBAR_W; ++i)
    panel[i] = source[0];
}

static void prepareSprites(void) {
  preparePanel(p1_panel, SnakeLateral2Playing); // panel verde
  preparePanel(p2_panel, SnakeLateral1Playing); // panel azul
  for (int i = 0; i < SIDEBAR_W * SIDEBAR_H; ++i)
    blank_panel[i] = SnakeLateral2Playing[0];
  for (int row = 0; row < CURSOR_H; ++row)
    for (int col = 0; col < CURSOR_W; ++col) {
      uint16_t pixel =
          MainMenuV2[(CURSOR_Y + row) * SCREEN_SIZE + CURSOR_X + col];
      menu_cursor[row * CURSOR_W + col] = pixel ? pixel : TRANSPARENT;
    }
}

// En el simulador el teclado da dos controles (flechas y WASD). En la FPGA solo
// hay lectura del primero hasta que se documente el segundo control NES.
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

static int sameCell(Cell a, Cell b) { return a.x == b.x && a.y == b.y; }

static int cellInSnake(const Snake *s, Cell c, uint16_t from) {
  for (uint16_t i = from; i < s->length; ++i)
    if (sameCell(s->body[i], c))
      return 1;
  return 0;
}

// Elige al azar una casilla libre. Devuelve 0 si el tablero esta lleno.
static int placeApple(Game *g) {
  uint8_t occupied[GRID_H][GRID_W];
  memset(occupied, 0, sizeof(occupied));
  for (int p = 0; p < g->players; ++p)
    for (uint16_t i = 0; i < g->snakes[p].length; ++i)
      occupied[g->snakes[p].body[i].y][g->snakes[p].body[i].x] = 1;

  uint16_t free_cells = 0;
  for (int y = 0; y < GRID_H; ++y)
    for (int x = 0; x < GRID_W; ++x)
      free_cells += !occupied[y][x];
  if (free_cells == 0)
    return 0;

  uint16_t pick = (uint16_t)(nextRandom(&g->rng) % free_cells);
  for (int y = 0; y < GRID_H; ++y)
    for (int x = 0; x < GRID_W; ++x)
      if (!occupied[y][x] && pick-- == 0) {
        g->apple = (Cell){(uint8_t)x, (uint8_t)y};
        return 1;
      }
  return 0;
}

static void initSnake(Snake *s, uint8_t x, uint8_t y, const uint16_t *head,
                      const uint16_t *body) {
  for (uint16_t i = 0; i < START_LENGTH; ++i)
    s->body[i] = (Cell){x, (uint8_t)(y + i)};
  s->length = START_LENGTH;
  s->dir = DIR_UP;
  s->turn_count = 0;
  s->score = 0;
  s->alive = 1;
  s->head_sprite = head;
  s->body_sprite = body;
}

static void initGame(Game *g, uint8_t players, uint32_t seed) {
  g->players = players;
  if (players == 1) {
    initSnake(&g->snakes[0], GRID_W / 2, GRID_H / 2, SnakeHead1, SnakeBody1);
  } else {
    initSnake(&g->snakes[0], GRID_W / 4, GRID_H / 2, SnakeHead1, SnakeBody1);
    initSnake(&g->snakes[1], GRID_W - GRID_W / 4, GRID_H / 2, SnakeHead2,
              SnakeBody2);
  }
  g->step_ms = STEP_START_MS;
  g->step_timer = 0;
  g->rng = seed | 1; // xorshift no sale nunca del cero
  g->board_full = 0;
  placeApple(g);
}

// Guarda hasta TURN_QUEUE giros para que dos toques rapidos no se pierdan.
// Se descartan los giros repetidos y los que darian media vuelta.
static void queueTurn(Snake *s, Direction d) {
  Direction last = s->turn_count ? s->turns[s->turn_count - 1] : s->dir;
  if (s->turn_count == TURN_QUEUE || d == last || d == (Direction)(last ^ 1))
    return;
  s->turns[s->turn_count++] = d;
}

static void readTurns(Snake *s, uint8_t pressed) {
  if (pressed & NES_BIT_UP)
    queueTurn(s, DIR_UP);
  if (pressed & NES_BIT_DOWN)
    queueTurn(s, DIR_DOWN);
  if (pressed & NES_BIT_LEFT)
    queueTurn(s, DIR_LEFT);
  if (pressed & NES_BIT_RIGHT)
    queueTurn(s, DIR_RIGHT);
}

static Cell nextHead(const Snake *s) {
  Cell c = s->body[0];
  switch (s->dir) {
  case DIR_UP:
    c.y = (uint8_t)((c.y + GRID_H - 1) % GRID_H);
    break;
  case DIR_DOWN:
    c.y = (uint8_t)((c.y + 1) % GRID_H);
    break;
  case DIR_LEFT:
    c.x = (uint8_t)((c.x + GRID_W - 1) % GRID_W);
    break;
  case DIR_RIGHT:
    c.x = (uint8_t)((c.x + 1) % GRID_W);
    break;
  }
  return c;
}

// Al crecer la cola se queda donde estaba; si no, avanza junto con la cabeza.
static void moveSnake(Snake *s, Cell head, int grow) {
  if (grow && s->length < MAX_LENGTH)
    ++s->length;
  memmove(&s->body[1], &s->body[0], sizeof(Cell) * (s->length - 1u));
  s->body[0] = head;
}

// Avanza una casilla. Devuelve 1 cuando la partida termina.
static int stepGame(Game *g) {
  Cell heads[2];
  int ate[2] = {0, 0};

  for (int p = 0; p < g->players; ++p) {
    Snake *s = &g->snakes[p];
    if (s->turn_count) {
      s->dir = s->turns[0];
      s->turns[0] = s->turns[1];
      --s->turn_count;
    }
    heads[p] = nextHead(s);
    ate[p] = sameCell(heads[p], g->apple);
  }
  // Primero se mueven las dos, asi una cabeza puede entrar a la casilla que
  // acaba de dejar una cola.
  for (int p = 0; p < g->players; ++p)
    moveSnake(&g->snakes[p], heads[p], ate[p]);

  int crashed = 0;
  for (int p = 0; p < g->players; ++p) {
    Snake *s = &g->snakes[p];
    if (cellInSnake(s, s->body[0], 1) ||
        (g->players == 2 && cellInSnake(&g->snakes[1 - p], s->body[0], 0))) {
      s->alive = 0;
      crashed = 1;
    }
  }

  int someone_ate = 0;
  for (int p = 0; p < g->players; ++p)
    if (ate[p]) {
      ++g->snakes[p].score;
      someone_ate = 1;
    }
  if (crashed)
    return 1;
  if (someone_ate) {
    g->step_ms = g->step_ms > STEP_MIN_MS + STEP_FASTER_MS
                     ? (uint16_t)(g->step_ms - STEP_FASTER_MS)
                     : STEP_MIN_MS;
    if (!placeApple(g)) {
      g->board_full = 1;
      return 1;
    }
  }
  return 0;
}

static int advanceGame(Game *g, uint32_t frame_delta) {
  g->step_timer += frame_delta;
  if (g->step_timer < g->step_ms)
    return 0;
  g->step_timer -= g->step_ms;
  if (g->step_timer > g->step_ms) // tras un cuadro muy lento no se acumulan pasos
    g->step_timer = 0;
  return stepGame(g);
}

// 0 = empate, 1 = gana P1, 2 = gana P2.
static int winner(const Game *g) {
  int alive1 = g->snakes[0].alive;
  int alive2 = g->snakes[1].alive;
  if (alive1 != alive2)
    return alive1 ? 1 : 2;
  if (alive1 && g->snakes[0].score != g->snakes[1].score)
    return g->snakes[0].score > g->snakes[1].score ? 1 : 2;
  return 0;
}

static void drawText(const char *text, uint8_t x, uint8_t y) {
  sendString((char *)text, x, y, CHAR_W, CHAR_H);
}

// Texto 3x5 centrado en un ancho que empieza en x = 0.
static void drawCentered(const char *text, uint8_t y, uint8_t area_w) {
  int width = (int)strlen(text) * (CHAR_W + 1) - 1;
  drawText(text, (uint8_t)((area_w - width) / 2), y);
}

// Texto en la fuente 5x7 (6 px por caracter), para lo que la 3x5 no resuelve.
static void drawBig(const char *text, uint8_t y, uint8_t area_w) {
  int width = (int)strlen(text) * (BIG_W + 1) - 1;
  sendString((char *)text, (uint8_t)((area_w - width) / 2), y, BIG_W, BIG_H);
}

static void drawDigit(int digit, uint8_t x, uint8_t y) {
  uint16_t pixels[3 * 5];
  for (int row = 0; row < 5; ++row)
    for (int col = 0; col < 3; ++col)
      pixels[row * 3 + col] =
          (digit_rows[digit][row] >> (2 - col)) & 1 ? TEXT_COLOR : TRANSPARENT;
  sendSprite(pixels, x, y, 3, 5);
}

// Hasta dos cifras centradas en el panel lateral.
static void drawPanelNumber(uint16_t value, uint8_t y) {
  if (value > 99)
    value = 99;
  if (value >= 10) {
    drawDigit(value / 10, SIDEBAR_X + 1, y);
    drawDigit(value % 10, SIDEBAR_X + 5, y);
  } else {
    drawDigit(value, SIDEBAR_X + 3, y);
  }
}

static void drawCell(const uint16_t *sprite, Cell c) {
  sendSprite(sprite, (uint8_t)(c.x * CELL), (uint8_t)(c.y * CELL), CELL, CELL);
}

static void drawSnake(const Snake *s) {
  for (uint16_t i = s->length - 1u; i > 0; --i)
    drawCell(s->body_sprite, s->body[i]);
  drawCell(s->head_sprite, s->body[0]);
}

static void drawGame(const Game *g, uint16_t best_score) {
  sendBackground(Escenario);
  drawCell(Apple, g->apple);
  for (int p = 0; p < g->players; ++p)
    drawSnake(&g->snakes[p]);

  sendSprite(p1_panel, SIDEBAR_X, 0, SIDEBAR_W, SIDEBAR_H);
  drawPanelNumber(g->snakes[0].score, PANEL_SCORE_Y);
  if (g->players == 2) {
    sendSprite(p2_panel, SIDEBAR_X, SIDEBAR_H, SIDEBAR_W, SIDEBAR_H);
    drawPanelNumber(g->snakes[1].score, SIDEBAR_H + PANEL_SCORE_Y);
  } else {
    sendSprite(blank_panel, SIDEBAR_X, SIDEBAR_H, SIDEBAR_W, SIDEBAR_H);
    drawText("HI", SIDEBAR_X + 1, SIDEBAR_H + PANEL_SCORE_Y - 7);
    drawPanelNumber(best_score, SIDEBAR_H + PANEL_SCORE_Y);
  }
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
  drawBig(lines[0], 20, SCREEN_SIZE);
  drawBig(lines[1], 30, SCREEN_SIZE);
  drawCentered("B: BACK", 52, SCREEN_SIZE);
}

static void drawReady(void) {
  sendSprite(black_pixels, 4, 6, SIDEBAR_X - 8, 17);
  drawCentered("PRESS START", 9, SIDEBAR_X);
  drawCentered("B: EXIT", 16, SIDEBAR_X);
}

static void drawGameOver(const Game *g, int made_best) {
  char line[16];
  sendSprite(black_pixels, 0, 8, SIDEBAR_X, OVERLAY_H);
  if (g->players == 1) {
    drawBig(g->board_full ? "YOU WIN" : "GAME OVER", 11, SIDEBAR_X);
    snprintf(line, sizeof(line), "SCORE %u", (unsigned)g->snakes[0].score);
    drawBig(line, 21, SIDEBAR_X);
    if (made_best)
      drawBig("NEW BEST", 31, SIDEBAR_X);
  } else {
    static const char *const results[3] = {"DRAW", "P1 WINS", "P2 WINS"};
    drawBig(results[winner(g)], 16, SIDEBAR_X);
  }
  drawCentered("START: REPLAY", 42, SIDEBAR_X);
  drawCentered("B: EXIT", 49, SIDEBAR_X);
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
  uint16_t best_score = 0;
  int made_best = 0;
  int gameover_wait = 0;
  uint8_t previous_p1 = 0;
  uint8_t previous_p2 = 0;
  uint32_t last_frame_ticks = SDL_GetTicks();

  while (running) {
    uint32_t frame_ticks = SDL_GetTicks();
    uint32_t frame_delta = frame_ticks - last_frame_ticks;
    last_frame_ticks = frame_ticks;
    uint8_t p1 = readControls(&running, &default_keys);
    uint8_t p2 = readControls(&running, &player2_keys);
    // Solo el flanco de subida: mantener un boton no lo repite.
    uint8_t pressed = (uint8_t)(p1 & ~previous_p1);
    uint8_t pressed2 = (uint8_t)(p2 & ~previous_p2);
    previous_p1 = p1;
    previous_p2 = p2;
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
      if (back)
        current_screen = SCREEN_STARTMENU;
      else if (pressed & NES_BIT_START)
        current_screen = SCREEN_GAME;
      break;

    case SCREEN_GAME:
      if (pressed & NES_BIT_START) {
        current_screen = SCREEN_PAUSE;
        break;
      }
      readTurns(&game.snakes[0], pressed);
      if (game.players == 2)
        readTurns(&game.snakes[1], pressed2);
      if (advanceGame(&game, frame_delta)) {
        made_best = game.players == 1 && game.snakes[0].score > best_score;
        if (made_best)
          best_score = game.snakes[0].score;
        gameover_wait = GAMEOVER_WAIT;
        current_screen = SCREEN_GAMEOVER;
      }
      break;

    case SCREEN_PAUSE:
      if (confirm)
        current_screen = SCREEN_GAME;
      else if (back)
        current_screen = SCREEN_STARTMENU;
      break;

    case SCREEN_GAMEOVER:
      // Un momento sin aceptar botones para que se vea el choque y no se
      // salte el resultado si el jugador seguia pulsando.
      if (gameover_wait > 0) {
        --gameover_wait;
      } else if (pressed & NES_BIT_START) {
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
      drawGame(&game, best_score);
      drawReady();
      break;
    case SCREEN_GAME:
      drawGame(&game, best_score);
      break;
    case SCREEN_PAUSE:
      sendBackground(PauseMenu);
      break;
    case SCREEN_GAMEOVER:
      drawGame(&game, best_score);
      if (gameover_wait == 0)
        drawGameOver(&game, made_best);
      break;
    }

    SDL_RenderPresent(global_renderer);
    SDL_Delay(FRAME_MS);
  }

  SDL_Quit();
  return 0;
}
