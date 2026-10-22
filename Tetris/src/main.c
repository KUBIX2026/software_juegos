// Tetris para KUBIX.
// Flujo de pantallas segun docs/diagramas/Diagrama menu e inicio de juego.png:
// submenu (Singleplayer, Tutorial, Scores, Exit) -> "Press start to play" ->
// partida -> fin con score. Multiplayer no aplica: Tetris es de un jugador.
// Las reglas estan en tetris_logica.c y no dependen de SDL.

#include <string.h>

#include "assets.h"
#include "hal.h"
#include "tetris_logica.h"

typedef enum {
  SCREEN_MENU,
  SCREEN_READY,
  SCREEN_GAME,
  SCREEN_PAUSE,
  SCREEN_GAMEOVER,
  SCREEN_TUTORIAL,
  SCREEN_SCORES
} AppScreen;

typedef enum {
  MENU_SINGLEPLAYER,
  MENU_TUTORIAL,
  MENU_SCORES,
  MENU_EXIT,
  MENU_COUNT
} MenuItem;

enum {
  SCREEN_SIZE = 64,
  CHAR_W = 3, // fuente pequena 3x5: no dibuja bien la W ni el 7
  CHAR_H = 5,
  BIG_W = 5, // fuente 5x7
  BIG_H = 7,
  LINE_STEP = 8,
  CELL = 3,    // lado de un bloque en pixeles
  BOARD_X = 4, // esquina del tablero dentro de Cancha
  BOARD_Y = 2,
  NEXT_X = 43, // zona de 14x14 donde se dibuja la pieza siguiente
  NEXT_Y = 9,
  NEXT_SIZE = 14,
  SCORE_X = 38,
  SCORE_Y = 32,
  LEVEL_X = 46,
  LEVEL_Y = 46,
  LINES_X = 44,
  LINES_Y = 58,
  PANEL_W = 56,
  PANEL_H = 45,
  GAMEOVER_WAIT = 40 // unos 1,2 segundos
};

static const char *const menu_labels[MENU_COUNT] = {"SINGLEPLAYER", "TUTORIAL",
                                                    "SCORES", "EXIT"};

static const uint16_t *const block_sprites[TETRIS_PIEZAS] = {
    BloqueI, BloqueO, BloqueT, BloqueS, BloqueZ, BloqueJ, BloqueL};
static const uint16_t *const digit_sprites[10] = {Dig0, Dig1, Dig2, Dig3, Dig4,
                                                  Dig5, Dig6, Dig7, Dig8, Dig9};

static SoundID sound_rotate = SOUND_INVALID_ID;
static SoundID sound_lock = SOUND_INVALID_ID;
static SoundID sound_line = SOUND_INVALID_ID;
static SoundID sound_four = SOUND_INVALID_ID;
static SoundID sound_level = SOUND_INVALID_ID;
static SoundID sound_end = SOUND_INVALID_ID;

static uint32_t best_scores[TETRIS_TOP_N];

// Punto de enganche con el menu general de KUBIX: hoy Tetris corre solo, asi
// que volver al menu general termina el programa.
static void backToGeneralMenu(int *running) { *running = 0; }

static void loadSounds(void) {
  sound_rotate = initSound("../assets/tetris_rotar.wav");
  sound_lock = initSound("../assets/tetris_fijar.wav");
  sound_line = initSound("../assets/tetris_linea.wav");
  sound_four = initSound("../assets/tetris_cuatro.wav");
  sound_level = initSound("../assets/tetris_nivel.wav");
  sound_end = initSound("../assets/tetris_fin.wav");
}

// Un sonido por jugada: si se limpiaron lineas suena eso en lugar del golpe de
// la pieza al fijarse; mover la pieza no suena (pasa casi cada cuadro).
static void playEvents(uint8_t events) {
  if (events & TETRIS_EVT_ROTAR)
    playSound(sound_rotate);
  if (events & TETRIS_EVT_TETRIS)
    playSound(sound_four);
  else if (events & TETRIS_EVT_LINEA)
    playSound(sound_line);
  else if (events & TETRIS_EVT_FIJAR)
    playSound(sound_lock);
  if (events & TETRIS_EVT_NIVEL)
    playSound(sound_level);
  if (events & TETRIS_EVT_FIN)
    playSound(sound_end);
}

// Teclas mantenidas (izquierda, derecha, abajo) y pulsos (caida directa y
// rotaciones, solo en el cuadro en que se presionan).
static EntradaTetris toInput(uint8_t held, uint8_t pressed) {
  EntradaTetris in;
  in.izquierda = (held & NES_BIT_LEFT) != 0;
  in.derecha = (held & NES_BIT_RIGHT) != 0;
  in.abajo = (held & NES_BIT_DOWN) != 0;
  in.caida_directa = (pressed & NES_BIT_UP) != 0;
  in.rotar_der = (pressed & NES_BIT_A) != 0;
  in.rotar_izq = (pressed & NES_BIT_B) != 0;
  return in;
}

static void clearScreen(void) {
  SDL_SetRenderDrawColor(global_renderer, 0, 0, 0, 255);
  SDL_RenderClear(global_renderer);
}

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

static void drawBigAt(const char *text, uint8_t x, uint8_t y) {
  sendString((char *)text, x, y, BIG_W, BIG_H);
}

// Rotulo (sprite) centrado horizontalmente.
static void drawLabel(const uint16_t *sprite, uint8_t w, uint8_t h, uint8_t y) {
  sendSprite(sprite, (uint8_t)((SCREEN_SIZE - w) / 2), y, w, h);
}

// Rectangulo oscuro (todo en cero = negro) para que el texto no se mezcle con
// el tablero. Cabe el panel mas grande que se dibuja.
static const uint16_t black_pixels[PANEL_W * PANEL_H];

static void drawPanel(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  sendSprite(black_pixels, x, y, w, h);
}

// Numero con ceros a la izquierda, con los digitos de 3x5 como sprites.
static void drawNumber(uint32_t value, int digits, uint8_t x, uint8_t y) {
  for (int i = digits - 1; i >= 0; --i) {
    sendSprite(digit_sprites[value % 10], (uint8_t)(x + i * (Dig0_W + 1)), y,
               Dig0_W, Dig0_H);
    value /= 10;
  }
}

static void drawList(const char *const labels[], int count, int selected,
                     uint8_t first_y) {
  for (int i = 0; i < count; ++i) {
    uint8_t y = (uint8_t)(first_y + i * LINE_STEP);
    if (i == selected)
      sendSprite(Cursor, 4, (uint8_t)(y - 1), Cursor_W, Cursor_H);
    drawText(labels[i], 12, y);
  }
}

// Dibuja una pieza celda por celda; py cuenta desde la primera fila oculta.
static void drawPiece(const uint16_t *sprite, uint8_t type, uint8_t rot, int px,
                      int py) {
  const int8_t *cells = tetris_forma(type, rot);
  for (int i = 0; i < 4; ++i) {
    int row = py + cells[2 * i + 1] - TETRIS_FILAS_OCULTAS;
    int col = px + cells[2 * i];
    if (row < 0)
      continue; // todavia en la zona oculta de aparicion
    sendSprite(sprite, (uint8_t)(BOARD_X + col * CELL),
               (uint8_t)(BOARD_Y + row * CELL), BloqueI_W, BloqueI_H);
  }
}

// La pieza siguiente, centrada dentro de su recuadro.
static void drawNext(uint8_t type) {
  const int8_t *cells = tetris_forma(type, 0);
  int min_x = 4, max_x = 0, min_y = 4, max_y = 0;
  for (int i = 0; i < 4; ++i) {
    if (cells[2 * i] < min_x)
      min_x = cells[2 * i];
    if (cells[2 * i] > max_x)
      max_x = cells[2 * i];
    if (cells[2 * i + 1] < min_y)
      min_y = cells[2 * i + 1];
    if (cells[2 * i + 1] > max_y)
      max_y = cells[2 * i + 1];
  }
  int width = (max_x - min_x + 1) * CELL;
  int height = (max_y - min_y + 1) * CELL;
  int x0 = NEXT_X + (NEXT_SIZE - width) / 2 - min_x * CELL;
  int y0 = NEXT_Y + (NEXT_SIZE - height) / 2 - min_y * CELL;
  for (int i = 0; i < 4; ++i)
    sendSprite(block_sprites[type], (uint8_t)(x0 + cells[2 * i] * CELL),
               (uint8_t)(y0 + cells[2 * i + 1] * CELL), BloqueI_W, BloqueI_H);
}

static void drawGame(const EstadoTetris *e) {
  sendBackground(Cancha);
  for (int row = 0; row < TETRIS_FILAS; ++row)
    for (int col = 0; col < TETRIS_COLUMNAS; ++col) {
      uint8_t cell = tetris_celda(e, col, row);
      if (cell)
        sendSprite(block_sprites[cell - 1], (uint8_t)(BOARD_X + col * CELL),
                   (uint8_t)(BOARD_Y + row * CELL), BloqueI_W, BloqueI_H);
    }
  if (!e->juego_terminado) {
    int shadow = tetris_sombra_py(e);
    if (shadow != e->py)
      drawPiece(BloqueSombra, e->tipo, e->rot, e->px, shadow);
    drawPiece(block_sprites[e->tipo], e->tipo, e->rot, e->px, e->py);
  }
  drawNext(e->siguiente);
  drawNumber(e->puntaje, 6, SCORE_X, SCORE_Y);
  drawNumber((uint32_t)e->nivel + 1, 2, LEVEL_X, LEVEL_Y);
  drawNumber(e->lineas > 999 ? 999 : e->lineas, 3, LINES_X, LINES_Y);
}

static void drawMenu(int selected) {
  sendBackground(MenuTetris);
  drawList(menu_labels, MENU_COUNT, selected, 24);
}

static void drawReady(void) {
  sendBackground(MenuTetris);
  drawCentered("PRESS START", 30);
  drawCentered("TO PLAY", 38);
  drawCentered("B: MENU", 46);
}

static void drawTutorial(void) {
  drawBig("L/R: MOVE", 0);
  drawBig("DOWN: SOFT", 8);
  drawBig("UP: DROP", 16);
  drawBig("A: TURN R", 24);
  drawBig("B: TURN L", 32);
  drawBig("ENTER:", 40);
  drawBig("PAUSE", 48);
  drawCentered("A OR B: BACK", 58);
}

static void drawScores(void) {
  drawCentered("TOP 5", 3);
  for (int i = 0; i < TETRIS_TOP_N; ++i) {
    uint8_t y = (uint8_t)(12 + i * 9);
    char rank[3] = {(char)('1' + i), '.', 0};
    drawBigAt(rank, 8, y);
    if (best_scores[i] > 0)
      drawNumber(best_scores[i], 6, 26, (uint8_t)(y + 1));
    else
      drawBigAt("------", 26, y);
  }
  drawCentered("B: BACK", 58);
}

static void drawGameOver(const EstadoTetris *e, int made_top) {
  drawGame(e);
  drawPanel(4, 18, PANEL_W, PANEL_H);
  drawLabel(LabelGameOver, LabelGameOver_W, LabelGameOver_H, 21);
  drawCentered("SCORE", 31);
  drawNumber(e->puntaje, 6, (uint8_t)((SCREEN_SIZE - 23) / 2), 37);
  if (made_top)
    drawCentered("NEW TOP 5", 44);
  drawCentered("START: REPLAY", 50);
  drawCentered("B: MENU", 56);
}

int main(int argc, char *argv[]) {
  if (initScreen(argc, argv) != 0)
    return 1;
  loadSounds();

  int running = 1;
  AppScreen screen = SCREEN_MENU;
  int menu_selected = MENU_SINGLEPLAYER;
  int made_top = 0;
  uint8_t previous = 0;
  int gameover_wait = 0; // cuadros sin aceptar botones al perder
  EstadoTetris game;
  tetris_inicializar(&game, SDL_GetTicks());

  while (running) {
    uint8_t held = readControls(&running, &default_keys);
    uint8_t pressed = (uint8_t)(held & ~previous); // solo el flanco de subida
    previous = held;
    int confirm = (pressed & (NES_BIT_A | NES_BIT_START)) != 0;
    int back = (pressed & NES_BIT_B) != 0;
    int up = (pressed & NES_BIT_UP) != 0;
    int down = (pressed & NES_BIT_DOWN) != 0;

    switch (screen) {
    case SCREEN_MENU:
      if (up && menu_selected > 0)
        --menu_selected;
      if (down && menu_selected < MENU_COUNT - 1)
        ++menu_selected;
      if (back) {
        backToGeneralMenu(&running);
      } else if (confirm) {
        switch (menu_selected) {
        case MENU_SINGLEPLAYER:
          screen = SCREEN_READY;
          break;
        case MENU_TUTORIAL:
          screen = SCREEN_TUTORIAL;
          break;
        case MENU_SCORES:
          screen = SCREEN_SCORES;
          break;
        default:
          backToGeneralMenu(&running);
          break;
        }
      }
      break;

    case SCREEN_TUTORIAL:
    case SCREEN_SCORES:
      if (back || confirm)
        screen = SCREEN_MENU;
      break;

    case SCREEN_READY:
      if (back) {
        screen = SCREEN_MENU;
      } else if (pressed & NES_BIT_START) {
        tetris_inicializar(&game, SDL_GetTicks());
        screen = SCREEN_GAME;
      }
      break;

    case SCREEN_GAME:
      if (pressed & NES_BIT_START) {
        screen = SCREEN_PAUSE;
      } else {
        tetris_actualizar(&game, toInput(held, pressed));
        playEvents(game.eventos);
        if (game.juego_terminado) {
          made_top = tetris_registrar_puntaje(best_scores, game.puntaje);
          gameover_wait = GAMEOVER_WAIT;
          screen = SCREEN_GAMEOVER;
        }
      }
      break;

    case SCREEN_PAUSE:
      if (pressed & NES_BIT_START)
        screen = SCREEN_GAME;
      else if (back)
        screen = SCREEN_MENU;
      break;

    case SCREEN_GAMEOVER:
      // Un momento sin aceptar botones: si el jugador estaba rotando con B al
      // perder, no se salta de golpe la pantalla del resultado.
      if (gameover_wait > 0) {
        --gameover_wait;
      } else if (pressed & NES_BIT_START) {
        tetris_inicializar(&game, SDL_GetTicks());
        screen = SCREEN_GAME;
      } else if (back) {
        screen = SCREEN_MENU;
      }
      break;
    }

    clearScreen();
    switch (screen) {
    case SCREEN_MENU:
      drawMenu(menu_selected);
      break;
    case SCREEN_READY:
      drawReady();
      break;
    case SCREEN_GAME:
      drawGame(&game);
      break;
    case SCREEN_PAUSE:
      drawGame(&game);
      drawPanel(10, 25, 44, 13);
      drawLabel(LabelPaused, LabelPaused_W, LabelPaused_H, 28);
      break;
    case SCREEN_GAMEOVER:
      drawGameOver(&game, made_top);
      break;
    case SCREEN_TUTORIAL:
      drawTutorial();
      break;
    case SCREEN_SCORES:
      drawScores();
      break;
    }
    SDL_RenderPresent(global_renderer);
    SDL_Delay(30);
  }

  SDL_Quit();
  return 0;
}
