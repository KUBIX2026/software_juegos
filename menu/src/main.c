// Menu principal de KUBIX.
// Flujo segun docs/diagramas/Diagrama logica juegos.png: pantalla de
// espera ("PRESS A") -> selector de juegos -> se abre el juego elegido.
// En el simulador cada juego es un programa aparte: el menu lo ejecuta desde su
// carpeta build y vuelve a mostrarse cuando el juego termina (el
// backToGeneralMenu de cada juego cierra su programa).

#include <stdlib.h>
#include <string.h>

#include "assets.h"
#include "hal.h"

typedef enum { SCREEN_WAITING, SCREEN_SELECTOR, SCREEN_MESSAGE } AppScreen;

typedef struct {
  const char *name;
  const char *folder;
  const char *executable;
} GameEntry;

enum {
  SCREEN_SIZE = 64,
  FRAME_MS = 16,
  TILES_PER_PAGE = 4,
  TILE_SIZE = 28,
  ARROW_Y = 30, // a la altura de la franja blanca entre las filas de fichas
  PATH_LEN = 1024,
  CHAR_W = 3, // fuente pequena 3x5: no dibuja bien la W, la M, la U ni el 7
  CHAR_H = 5,
  BIG_W = 5, // fuente 5x7
  BIG_H = 7
};

static const uint16_t RING_COLOR = 0xCE79;     // borde gris de las fichas
static const uint16_t SELECTED_COLOR = 0x55DD; // borde azul de la ficha elegida
static const uint16_t WHITE = 0xFFFF;

// En el mismo orden que las fichas de GameSelector y GameSelector2.
static const GameEntry games[] = {
    {"ROAD FIGHTER", "roadFighter", "roadFighter"},
    {"SPACE INVADERS", "space_invaders", "space_invaders"},
    {"SNAKE", "snake", "snake"},
    {"PONG", "pong", "pong"},
    {"TETRIS", "Tetris", "tetris"},
    {"BREAKOUT", "breakout", "breakout"},
};
static const uint16_t *const pages[] = {GameSelector, GameSelector2};
enum {
  GAME_COUNT = sizeof(games) / sizeof(games[0]),
  PAGE_COUNT = sizeof(pages) / sizeof(pages[0])
};
static const uint8_t tile_x[TILES_PER_PAGE] = {3, 33, 3, 33};
static const uint8_t tile_y[TILES_PER_PAGE] = {3, 3, 33, 33};

// CMake define la raiz del repositorio; tambien se puede pasar como argumento.
#ifndef KUBIX_ROOT
#define KUBIX_ROOT ".."
#endif
#ifdef _WIN32
#define EXE_SUFFIX ".exe"
#else
#define EXE_SUFFIX ""
#endif

// Rectangulo de un color; alcanza para lineas de hasta SCREEN_SIZE pixeles.
static void fillRect(int x, int y, int w, int h, uint16_t color) {
  uint16_t pixels[SCREEN_SIZE];
  for (int i = 0; i < w * h; ++i)
    pixels[i] = color;
  sendSprite(pixels, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h);
}

// Borde de una ficha con las esquinas redondeadas, como en GameSelector.
static void drawRing(int x0, int y0, int size, uint16_t color) {
  int x1 = x0 + size - 1, y1 = y0 + size - 1;
  fillRect(x0 + 1, y0, size - 2, 1, color);
  fillRect(x0 + 1, y1, size - 2, 1, color);
  fillRect(x0, y0 + 1, 1, size - 2, color);
  fillRect(x1, y0 + 1, 1, size - 2, color);
  fillRect(x0 + 1, y0 + 1, 1, 1, color);
  fillRect(x1 - 1, y0 + 1, 1, 1, color);
  fillRect(x0 + 1, y1 - 1, 1, 1, color);
  fillRect(x1 - 1, y1 - 1, 1, 1, color);
}

// Mueve la seleccion por la cuadricula de 2x2; al salir por un costado pasa a
// la pagina vecina. Si en esa posicion no hay juego, va a la ficha de arriba
// de la misma columna, y si tampoco hay, se queda donde estaba.
static int moveSelection(int selected, int dx, int dy) {
  int page = selected / TILES_PER_PAGE;
  int slot = selected % TILES_PER_PAGE;
  int row = slot / 2 + dy, col = slot % 2 + dx;
  if (row < 0 || row > 1)
    return selected;
  if (col > 1) {
    if (page + 1 >= PAGE_COUNT)
      return selected;
    ++page;
    col = 0;
  } else if (col < 0) {
    if (page == 0)
      return selected;
    --page;
    col = 1;
  }
  int target = page * TILES_PER_PAGE + row * 2 + col;
  if (target >= GAME_COUNT)
    target = page * TILES_PER_PAGE + col;
  return target < GAME_COUNT ? target : selected;
}

// Busca el ejecutable en la carpeta build del juego (y en Release o Debug,
// donde lo deja Visual Studio).
static int findGame(const char *root, const GameEntry *game, char *exe,
                    char *dir) {
  static const char *const build_dirs[] = {"build", "build/Release",
                                           "build/Debug"};
  for (size_t i = 0; i < sizeof(build_dirs) / sizeof(build_dirs[0]); ++i) {
    snprintf(dir, PATH_LEN, "%s/%s/%s", root, game->folder, build_dirs[i]);
    snprintf(exe, 2 * PATH_LEN, "%s/%s%s", dir, game->executable, EXE_SUFFIX);
    FILE *file = fopen(exe, "rb");
    if (file) {
      fclose(file);
      return 1;
    }
  }
  return 0;
}

// Ejecuta el juego desde su carpeta (Pong y Tetris cargan sonidos de
// ../assets) y espera a que termine, con la ventana del menu oculta.
static void runGame(const GameEntry *game, const char *exe, const char *dir) {
  char command[4 * PATH_LEN];
#ifdef _WIN32
  snprintf(command, sizeof(command), "cd /d \"%s\" && \"%s\"", dir, exe);
#else
  snprintf(command, sizeof(command), "cd '%s' && '%s'", dir, exe);
#endif
  SDL_Window *window = SDL_RenderGetWindow(global_renderer);
  SDL_HideWindow(window);
  int status = system(command);
  if (status != 0)
    printf("%s termino con estado %d\n", game->name, status);
  SDL_ShowWindow(window);
  SDL_RaiseWindow(window);
  SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}

static void drawCentered(const char *text, uint8_t y) {
  int width = (int)strlen(text) * (CHAR_W + 1) - 1;
  sendString((char *)text, (uint8_t)((SCREEN_SIZE - width) / 2), y, CHAR_W,
             CHAR_H);
}

static void drawBig(const char *text, uint8_t y) {
  int width = (int)strlen(text) * (BIG_W + 1) - 1;
  sendString((char *)text, (uint8_t)((SCREEN_SIZE - width) / 2), y, BIG_W,
             BIG_H);
}

static void drawSelector(int selected) {
  int page = selected / TILES_PER_PAGE;
  int slot = selected % TILES_PER_PAGE;
  sendBackground(pages[page]);
  // Todas las fichas con borde gris; la elegida con borde azul y una linea
  // blanca por dentro, como viene dibujada la de Pong en GameSelector.
  for (int i = 0; i < TILES_PER_PAGE; ++i)
    drawRing(tile_x[i], tile_y[i], TILE_SIZE, RING_COLOR);
  drawRing(tile_x[slot], tile_y[slot], TILE_SIZE, SELECTED_COLOR);
  drawRing(tile_x[slot] + 1, tile_y[slot] + 1, TILE_SIZE - 2, WHITE);

  // Flechas en el marco cuando hay otra pagina hacia ese lado.
  if (page + 1 < PAGE_COUNT) {
    fillRect(SCREEN_SIZE - 3, ARROW_Y, 1, 4, SELECTED_COLOR);
    fillRect(SCREEN_SIZE - 2, ARROW_Y + 1, 1, 2, SELECTED_COLOR);
  }
  if (page > 0) {
    fillRect(2, ARROW_Y, 1, 4, SELECTED_COLOR);
    fillRect(1, ARROW_Y + 1, 1, 2, SELECTED_COLOR);
  }
}

static void drawMessage(const GameEntry *game) {
  drawBig("NOT BUILT", 16);
  drawCentered(game->name, 30);
  drawCentered("B: BACK", 52);
}

int main(int argc, char *argv[]) {
  if (initScreen(argc, argv) != 0)
    return 1;
  SDL_SetWindowTitle(SDL_RenderGetWindow(global_renderer), "KUBIX");
  const char *root = argc > 1 ? argv[1] : KUBIX_ROOT;

  int running = 1;
  AppScreen current_screen = SCREEN_WAITING;
  int selected = 0;
  uint8_t previous = 0;

  while (running) {
    uint8_t controls = readControls(&running, &default_keys);
    // Solo el flanco de subida: mantener un boton no lo repite.
    uint8_t pressed = (uint8_t)(controls & ~previous);
    previous = controls;
    int confirm = (pressed & (NES_BIT_A | NES_BIT_START)) != 0;
    int back = (pressed & NES_BIT_B) != 0;

    switch (current_screen) {
    case SCREEN_WAITING:
      if (confirm)
        current_screen = SCREEN_SELECTOR;
      break;

    case SCREEN_SELECTOR:
      if (pressed & NES_BIT_LEFT)
        selected = moveSelection(selected, -1, 0);
      if (pressed & NES_BIT_RIGHT)
        selected = moveSelection(selected, 1, 0);
      if (pressed & NES_BIT_UP)
        selected = moveSelection(selected, 0, -1);
      if (pressed & NES_BIT_DOWN)
        selected = moveSelection(selected, 0, 1);
      if (back) {
        current_screen = SCREEN_WAITING;
      } else if (confirm) {
        char dir[PATH_LEN], exe[2 * PATH_LEN];
        if (findGame(root, &games[selected], exe, dir)) {
          runGame(&games[selected], exe, dir);
          previous = 0xFF; // lo que siga pulsado al volver no cuenta
        } else {
          current_screen = SCREEN_MESSAGE;
        }
      }
      break;

    case SCREEN_MESSAGE:
      if (back || confirm)
        current_screen = SCREEN_SELECTOR;
      break;
    }

    SDL_SetRenderDrawColor(global_renderer, 0, 0, 0, 255);
    SDL_RenderClear(global_renderer);

    switch (current_screen) {
    case SCREEN_WAITING:
      sendBackground(waitingScreen);
      break;
    case SCREEN_SELECTOR:
      drawSelector(selected);
      break;
    case SCREEN_MESSAGE:
      drawMessage(&games[selected]);
      break;
    }

    SDL_RenderPresent(global_renderer);
    SDL_Delay(FRAME_MS);
  }

  SDL_Quit();
  return 0;
}
