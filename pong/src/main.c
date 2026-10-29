// Pong para KUBIX.
// Flujo de pantallas segun docs/diagramas/Diagrama logica juegos.png:
// submenu (Singleplayer, Multiplayer, Tutorial, Scores, Exit) -> "Press start
// to play" -> partida -> fin con score. La logica de las reglas esta en
// pong_logica.c y no depende de SDL.

#include <string.h>

#include "assets.h"
#include "hal.h"
#include "pong_logica.h"

typedef enum {
  SCREEN_MENU,
  SCREEN_DIFFICULTY,
  SCREEN_NEED_CONTROLS,
  SCREEN_READY,
  SCREEN_GAME,
  SCREEN_PAUSE,
  SCREEN_GAMEOVER,
  SCREEN_TUTORIAL,
  SCREEN_SCORES
} AppScreen;

typedef enum {
  MENU_SINGLEPLAYER,
  MENU_MULTIPLAYER,
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
  SCORE_X_LEFT = 26,  // borde derecho del digito del jugador izquierdo
  SCORE_X_RIGHT = 36, // borde izquierdo del digito del jugador derecho
  SCORE_Y = 3,
  GAMEOVER_WAIT = 40 // cuadros sin aceptar botones al terminar (unos 1,2 s)
};

static const char *const menu_labels[MENU_COUNT] = {
    "SINGLEPLAYER", "MULTIPLAYER", "TUTORIAL", "SCORES", "EXIT"};
static const char *const difficulty_labels[3] = {"EASY", "NORMAL", "HARD"};

static const uint16_t *const digit_sprites[10] = {
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9};
static const uint8_t digit_widths[10] = {Num0_W, Num1_W, Num2_W, Num3_W, Num4_W,
                                         Num5_W, Num6_W, Num7_W, Num8_W, Num9_W};
static const uint8_t digit_height = Num0_H;

// Segundo control: teclas W/S. Lo no asignado queda en SDLK_UNKNOWN.
static struct keys player2_keys = {.UP = SDLK_w, .DOWN = SDLK_s};

static SoundID sound_tap = SOUND_INVALID_ID;
static SoundID sound_click = SOUND_INVALID_ID;

static int best_scores[PONG_TOP_N];

// En el simulador el teclado da dos controles (flechas y W/S). En la FPGA solo
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

static void playEvents(uint8_t events) {
  if (events & (PONG_EVT_PALETA | PONG_EVT_PARED))
    playSound(sound_tap);
  if (events & (PONG_EVT_PUNTO | PONG_EVT_FIN))
    playSound(sound_click);
}

static AccionPong toAction(uint8_t controls) {
  int up = (controls & NES_BIT_UP) != 0;
  int down = (controls & NES_BIT_DOWN) != 0;
  if (up && !down)
    return PONG_ACCION_ARRIBA;
  if (down && !up)
    return PONG_ACCION_ABAJO;
  return PONG_ACCION_NINGUNA;
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

// Rectangulo oscuro (todo en cero = negro) para que el texto no se mezcle con
// el campo de juego. Cabe el panel mas grande que se dibuja.
static const uint16_t black_pixels[52 * 43];

static void drawPanel(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  sendSprite(black_pixels, x, y, w, h);
}

// Rotulo (sprite) centrado horizontalmente.
static void drawLabel(const uint16_t *sprite, uint8_t w, uint8_t h, uint8_t y) {
  sendSprite(sprite, (uint8_t)((SCREEN_SIZE - w) / 2), y, w, h);
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

static void drawDigit(int value, int right_aligned, uint8_t x) {
  if (value < 0 || value > 9)
    return;
  uint8_t width = digit_widths[value];
  sendSprite(digit_sprites[value], right_aligned ? (uint8_t)(x - width) : x,
             SCORE_Y, width, digit_height);
}

static void drawGame(const EstadoPong *e) {
  sendBackground(Escenario);
  drawDigit(e->puntos_izq, 1, SCORE_X_LEFT);
  drawDigit(e->puntos_der, 0, SCORE_X_RIGHT);
  sendSprite(Player1, PONG_PALETA_IZQ_X, (uint8_t)e->paleta_izq_y, Player1_W,
             Player1_H);
  sendSprite(Player2, PONG_PALETA_DER_X, (uint8_t)e->paleta_der_y, Player2_W,
             Player2_H);
  int16_t bx = pong_pelota_x(e);
  int16_t by = pong_pelota_y(e);
  if (bx >= 0 && by >= 0 && bx <= SCREEN_SIZE - Ball_W &&
      by <= SCREEN_SIZE - Ball_H)
    sendSprite(Ball, (uint8_t)bx, (uint8_t)by, Ball_W, Ball_H);
}

static void drawMenu(int selected) {
  sendBackground(MenuPong);
  drawList(menu_labels, MENU_COUNT, selected, 24);
}

static void drawDifficulty(int selected) {
  sendBackground(MenuPong);
  drawCentered("DIFFICULTY", 24);
  drawList(difficulty_labels, 3, selected, 33);
  drawCentered("A: OK  B: BACK", 58);
}

static void drawNeedControls(void) {
  sendBackground(MenuPong);
  drawCentered("NEED 2", 30);
  drawCentered("CONTROLLERS", 38);
  drawCentered("B: BACK", 54);
}

static void drawReady(void) {
  sendBackground(MenuPong);
  drawCentered("PRESS START", 30);
  drawCentered("TO PLAY", 38);
  drawCentered("B: MENU", 54);
}

static void drawTutorial(void) {
  drawBig("CONTROLS", 1);
  drawBig("UP DOWN:", 10);
  drawBig("MOVE", 19);
  drawBig("START:", 28);
  drawBig("PAUSE", 37);
  drawBig("P2: W S", 46);
  drawBig("B: BACK", 55);
}

static void drawScores(void) {
  char line[16];
  drawCentered("TOP 5", 3);
  for (int i = 0; i < PONG_TOP_N; ++i) {
    if (best_scores[i] > 0)
      snprintf(line, sizeof(line), "%d. %02d", i + 1, best_scores[i]);
    else
      snprintf(line, sizeof(line), "%d. --", i + 1);
    drawBig(line, (uint8_t)(12 + i * 9));
  }
  drawCentered("B: BACK", 58);
}

static void drawGameOver(const EstadoPong *e, int made_top) {
  drawGame(e); // el marcador final queda arriba, con los digitos grandes
  drawPanel(6, 20, 52, 43);
  if (e->modo == PONG_UN_JUGADOR) {
    if (e->ganador == 1)
      drawLabel(LabelYouWin, LabelYouWin_W, LabelYouWin_H, 23);
    else
      drawLabel(LabelYouLose, LabelYouLose_W, LabelYouLose_H, 23);
  } else if (e->ganador == 1) {
    drawLabel(LabelP1Wins, LabelP1Wins_W, LabelP1Wins_H, 23);
  } else {
    drawLabel(LabelP2Wins, LabelP2Wins_W, LabelP2Wins_H, 23);
  }
  if (made_top)
    drawCentered("NEW TOP 5", 34);
  drawCentered("START: REPLAY", 44);
  drawCentered("B: MENU", 53);
}

int main(int argc, char *argv[]) {
  if (initScreen(argc, argv) != 0)
    return 1;
  sound_tap = initSound("../../assets/pong/mixkit-game-ball-tap-2073.wav");
  sound_click =
      initSound("../../assets/pong/mixkit-video-game-retro-click-237.wav");

  int running = 1;
  AppScreen screen = SCREEN_MENU;
  int menu_selected = MENU_SINGLEPLAYER;
  int difficulty_selected = PONG_IA_NORMAL; // dificultad por defecto
  ModoPong mode = PONG_UN_JUGADOR;
  int made_top = 0;
  uint8_t previous_p1 = 0;
  int gameover_wait = 0;
  EstadoPong game;
  pong_inicializar(&game, SDL_GetTicks(), mode, PONG_IA_NORMAL);

  while (running) {
    uint8_t p1 = readControls(&running, &default_keys);
    uint8_t p2 = readControls(&running, &player2_keys);
    uint8_t pressed = (uint8_t)(p1 & ~previous_p1); // solo el flanco de subida
    previous_p1 = p1;
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
          mode = PONG_UN_JUGADOR;
          screen = SCREEN_DIFFICULTY;
          break;
        case MENU_MULTIPLAYER:
          mode = PONG_DOS_JUGADORES;
          screen = connectedControls() >= 2 ? SCREEN_READY
                                            : SCREEN_NEED_CONTROLS;
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

    case SCREEN_DIFFICULTY:
      if (up && difficulty_selected > 0)
        --difficulty_selected;
      if (down && difficulty_selected < 2)
        ++difficulty_selected;
      if (back)
        screen = SCREEN_MENU;
      else if (confirm)
        screen = SCREEN_READY;
      break;

    case SCREEN_NEED_CONTROLS:
    case SCREEN_TUTORIAL:
    case SCREEN_SCORES:
      if (back || confirm)
        screen = SCREEN_MENU;
      break;

    case SCREEN_READY:
      if (back) {
        screen = SCREEN_MENU;
      } else if (pressed & NES_BIT_START) {
        pong_inicializar(&game, SDL_GetTicks(), mode,
                         (NivelIA)difficulty_selected);
        screen = SCREEN_GAME;
      }
      break;

    case SCREEN_GAME:
      if (pressed & NES_BIT_START) {
        screen = SCREEN_PAUSE;
      } else {
        pong_actualizar(&game, toAction(p1), toAction(p2));
        playEvents(game.eventos);
        if (game.juego_terminado) {
          made_top = mode == PONG_UN_JUGADOR && pong_registrar_puntaje(best_scores, pong_puntaje(&game));
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
      // Un momento sin aceptar botones: si el jugador estaba pulsando Start al
      // terminar, no se salta de golpe la pantalla del resultado.
      if (gameover_wait > 0) {
        --gameover_wait;
      } else if (pressed & NES_BIT_START) {
        pong_inicializar(&game, SDL_GetTicks(), mode,
                         (NivelIA)difficulty_selected);
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
    case SCREEN_DIFFICULTY:
      drawDifficulty(difficulty_selected);
      break;
    case SCREEN_NEED_CONTROLS:
      drawNeedControls();
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
