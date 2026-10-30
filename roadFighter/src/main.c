#include <string.h>
#include "assets.h"
#include "hal.h"

typedef enum {
  SCREEN_PAUSE,
  SCREEN_STARTMENU,
  SCREEN_GAME,
  SCREEN_SETTINGS,
  SCREEN_TUTORIAL,
  SCREEN_SCORES
} AppScreen;

// Opciones del menu segun el diagrama. No hay Multiplayer: el juego es de un
// solo jugador.
typedef enum {
  MENU_SINGLEPLAYER,
  MENU_TUTORIAL,
  MENU_SCORES,
  MENU_EXIT,
  MENU_COUNT
} MenuItem;

enum {
  SCREEN_SIZE = 64,
  CAR_WIDTH = 8,
  CAR_HEIGHT = 12,
  PLAYER_Y = 50,
  PLAYER_MIN_Y = SCREEN_SIZE - CAR_HEIGHT - 5,
  LANE_COUNT = 4,
  TRAFFIC_COUNT = 18,
  PLAYER_MAX_SPEED = 10,
  TRAFFIC_MIN_SPEED = 3,
  DISTANCE_GOAL_METERS = 200,
  DISTANCE_UNITS_PER_METER = 60,
  DISTANCE_UNITS_PER_PIXEL = 15,
  TRAFFIC_AFTER_GOAL_UNITS = (SCREEN_SIZE + 21) * DISTANCE_UNITS_PER_PIXEL,
  MENU_CLEAR_Y = 29, // desde esta fila se borran los iconos dibujados en MainMenu
  MENU_LIST_Y = 31,  // primera opcion de la lista
  MENU_STEP = 8,
  MENU_CURSOR_X = 4,
  MENU_TEXT_X = 11,
  TOP_N = 5,         // tiempos que guarda la tabla de mejores
  FINISH_WAIT = 40,  // cuadros sin aceptar botones al llegar a la meta
  BLACK_H = 40,      // alto del recuadro negro mas grande que se dibuja
  CHAR_W = 3,        // fuente pequena 3x5: no dibuja bien la W, la M, la U ni el 7
  CHAR_H = 5,
  BIG_W = 5,         // fuente 5x7
  BIG_H = 7
};

typedef struct {
  const uint16_t *sprite;
  int16_t y;
  uint16_t road_position_units;
  uint8_t speed;
  uint8_t goal_percent;
  uint8_t lane;
  uint8_t width;
  uint8_t height;
  uint8_t hit;
} TrafficCar;

static const uint8_t lane_x[LANE_COUNT] = {5, 14, 23, 32};
static const uint8_t player_lane_step = 4;

static void scrollOverlay(const uint16_t source[SCREEN_SIZE * SCREEN_SIZE],
                          uint16_t destination[SCREEN_SIZE * SCREEN_SIZE],
                          unsigned offset) {
  offset %= SCREEN_SIZE;
  for (unsigned y = 0; y < SCREEN_SIZE; ++y) {
    unsigned source_y = (y + SCREEN_SIZE - offset) % SCREEN_SIZE;
    for (unsigned x = 0; x < SCREEN_SIZE; ++x)
      destination[y * SCREEN_SIZE + x] = source[source_y * SCREEN_SIZE + x];
  }
}

static void advanceScroll(uint8_t *offset, uint8_t speed,
                          uint8_t car_moving_forward) {
  if (car_moving_forward)
    *offset = (uint8_t)((*offset + speed) % SCREEN_SIZE);
  else
    *offset = (uint8_t)((*offset + SCREEN_SIZE - speed) % SCREEN_SIZE);
}

static void drawGame(uint8_t player_x, uint8_t player_y,
                     const TrafficCar traffic[],
                     uint32_t elapsed_seconds, uint16_t distance_meters,
                     uint8_t game_finished, uint8_t show_hint,
                     uint8_t player_crash_timer, const uint16_t *fence_layer,
                     const uint16_t *line_layer) {
  char time_text[16];
  char distance_text[8];

  sendBackground(SimplerRoad);
  sendSprite(fence_layer, 0, 0, SCREEN_SIZE, SCREEN_SIZE);
  sendSprite(line_layer, 0, 0, SCREEN_SIZE, SCREEN_SIZE);
  for (int i = 0; i < TRAFFIC_COUNT; ++i) {
    int16_t y = traffic[i].y;
    uint8_t height = traffic[i].height;
    const uint16_t *sprite = traffic[i].sprite;
    if (y < 0) {
      int16_t clipped_rows = (int16_t)-y;
      if (clipped_rows >= height)
        continue;
      sprite += (uint16_t)clipped_rows * traffic[i].width;
      height = (uint8_t)(height - clipped_rows);
      y = 0;
    }
    if (y >= SCREEN_SIZE)
      continue;
    sendSprite(sprite, lane_x[traffic[i].lane], (uint8_t)y, traffic[i].width,
               height);
  }
  if (player_crash_timer > 0)
    sendSprite(CarDestroyed, player_x + 1, player_y, 7, CAR_HEIGHT);
  else
    sendSprite(RedCar, player_x, player_y, CAR_WIDTH, CAR_HEIGHT);

  snprintf(time_text, sizeof(time_text), "%02u%02u",
           (unsigned)(elapsed_seconds / 60), (unsigned)(elapsed_seconds % 60));
  sendString(time_text, 45, 5, 3, 5);
  snprintf(distance_text, sizeof(distance_text), "%03u",
           (unsigned)distance_meters);
  sendString(distance_text, 48, 16, 3, 5);
  if (game_finished) {
    // Franja negra para que el texto se lea sobre las lineas de la carretera.
    // La fuente 3x5 del HAL no tiene el simbolo "!": se dibujaria como una "I".
    uint16_t banner[29 * 15];
    for (int i = 0; i < 29 * 15; ++i)
      banner[i] = 0x0000;
    sendSprite(banner, 10, 30, 29, show_hint ? 15 : 7);
    sendString("FINISH", 13, 31, 3, 5);
    if (show_hint)
      sendString("A: BACK", 11, 39, 3, 5);
  }
}

static int carsOverlap(uint8_t ax, int ay, uint8_t aw, uint8_t ah, uint8_t bx,
                       int by, uint8_t bw, uint8_t bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void updateTraffic(TrafficCar traffic[], uint8_t player_x,
                          uint8_t player_y, uint32_t player_distance_units,
                          unsigned *collisions, uint8_t *player_crash_timer,
                          uint8_t check_collisions) {
  uint8_t recovering = check_collisions && *player_crash_timer > 0;
  if (check_collisions && *player_crash_timer > 0)
    --*player_crash_timer;

  for (int i = 0; i < TRAFFIC_COUNT; ++i) {
    uint32_t traffic_end_units =
        DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER +
        TRAFFIC_AFTER_GOAL_UNITS;
    if (traffic[i].road_position_units < traffic_end_units) {
      uint32_t next_position =
          traffic[i].road_position_units + traffic[i].speed;
      traffic[i].road_position_units =
          (uint16_t)(next_position > traffic_end_units ? traffic_end_units
                                                       : next_position);
    }
    uint32_t car_distance_units = traffic[i].road_position_units;
    int32_t distance_ahead =
        (int32_t)car_distance_units - (int32_t)player_distance_units;
    traffic[i].y =
        (int16_t)(PLAYER_Y - distance_ahead / DISTANCE_UNITS_PER_PIXEL);

    if (check_collisions && !traffic[i].hit && !recovering &&
        carsOverlap(player_x, player_y, CAR_WIDTH, CAR_HEIGHT,
                    lane_x[traffic[i].lane], (int)traffic[i].y,
                    traffic[i].width, traffic[i].height)) {
      ++*collisions;
      *player_crash_timer = 30;
      traffic[i].hit = 1;
      recovering = 1;
    }
  }
}

static void updatePlayer(uint8_t controls, uint8_t *x, uint8_t *y,
                         uint8_t *speed, uint8_t *moving_forward,
                         uint32_t frame_delta,
                         uint32_t *velocity_milli,
                         uint32_t *movement_remainder,
                         uint32_t *velocity_sum_milli,
                         uint8_t *frame_movement_pixels) {
  if ((controls & NES_BIT_LEFT) && !(controls & NES_BIT_RIGHT))
    *x -= player_lane_step;
  if ((controls & NES_BIT_RIGHT) && !(controls & NES_BIT_LEFT))
    *x += player_lane_step;
  if (*x < lane_x[0])
    *x = lane_x[0];
  if (*x > lane_x[LANE_COUNT - 1])
    *x = lane_x[LANE_COUNT - 1];

  uint32_t previous_velocity_milli = *velocity_milli;
  if ((controls & NES_BIT_UP) && !(controls & NES_BIT_DOWN)) {
    *moving_forward = 1;
    uint32_t max_velocity_milli = PLAYER_MAX_SPEED * 1000;
    *velocity_milli = *velocity_milli + frame_delta > max_velocity_milli
                          ? max_velocity_milli
                          : *velocity_milli + frame_delta;
  } else if ((controls & NES_BIT_DOWN) && !(controls & NES_BIT_UP)) {
    *moving_forward = 1;
    *velocity_milli = *velocity_milli > frame_delta
                          ? *velocity_milli - frame_delta
                          : 0;
  }

  *speed = (uint8_t)((*velocity_milli + 500) / 1000);
  *velocity_sum_milli = previous_velocity_milli + *velocity_milli;
  uint32_t movement_numerator = *movement_remainder +
                                (*velocity_sum_milli * frame_delta);
  uint32_t movement_pixels = movement_numerator / 2000000;
  *movement_remainder = movement_numerator % 2000000;
  *frame_movement_pixels =
      (uint8_t)(movement_pixels > UINT8_MAX ? UINT8_MAX : movement_pixels);
  uint8_t available_up = *y > PLAYER_MIN_Y ? *y - PLAYER_MIN_Y : 0;
  if (movement_pixels > available_up)
    movement_pixels = available_up;
  *y = (uint8_t)(*y - movement_pixels);
  *moving_forward = 1;
}

// FEATURE: I want the cars to be at a random position.
static const TrafficCar traffic_template[TRAFFIC_COUNT] = {
    {.sprite = BlueCar,
     .goal_percent = 5,
     .lane = 0,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = YellowCar,
     .goal_percent = 10,
     .lane = 2,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = Truck,
     .goal_percent = 15,
     .lane = 1,
     .width = 9,
     .height = 21},
    {.sprite = YellowCar,
     .goal_percent = 20,
     .lane = 0,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 25,
     .lane = 2,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 30,
     .lane = 1,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = Truck,
     .goal_percent = 35,
     .lane = 0,
     .width = 9,
     .height = 21},
    {.sprite = YellowCar,
     .goal_percent = 40,
     .lane = 2,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 45,
     .lane = 1,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 50,
     .lane = 0,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = Truck,
     .goal_percent = 55,
     .lane = 2,
     .width = 9,
     .height = 21},
    {.sprite = YellowCar,
     .goal_percent = 60,
     .lane = 1,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 65,
     .lane = 0,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 70,
     .lane = 2,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = Truck,
     .goal_percent = 75,
     .lane = 1,
     .width = 9,
     .height = 21},
    {.sprite = YellowCar,
     .goal_percent = 80,
     .lane = 0,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 85,
     .lane = 2,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
    {.sprite = BlueCar,
     .goal_percent = 90,
     .lane = 1,
     .width = CAR_WIDTH,
     .height = CAR_HEIGHT},
};

// Todo el estado de una carrera: asi se puede reiniciar desde el menu.
typedef struct {
  uint8_t player_x;
  uint8_t player_y;
  uint8_t player_speed;
  uint8_t player_moving_forward;
  uint8_t frame_movement_pixels;
  uint32_t player_velocity_milli;
  uint32_t velocity_sum_milli;
  uint32_t player_movement_remainder;
  unsigned collisions;
  uint8_t fence_scroll;
  uint8_t line_scroll;
  uint8_t game_finished;
  uint8_t player_crash_timer;
  uint32_t player_distance_units;
  uint32_t distance_progress_remainder;
  uint32_t elapsed_ms;
  uint16_t moving_fences[SCREEN_SIZE * SCREEN_SIZE];
  uint16_t moving_lines[SCREEN_SIZE * SCREEN_SIZE];
  TrafficCar traffic[TRAFFIC_COUNT];
} Race;

static void initRace(Race *r) {
  memset(r, 0, sizeof(*r));
  r->player_x = lane_x[1];
  r->player_y = PLAYER_Y;
  r->player_moving_forward = 1;
  memcpy(r->traffic, traffic_template, sizeof(r->traffic));
  for (int i = 0; i < TRAFFIC_COUNT; ++i) {
    r->traffic[i].road_position_units =
        (uint16_t)(DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER *
                   r->traffic[i].goal_percent / 100);
    r->traffic[i].speed = TRAFFIC_MIN_SPEED;
    r->traffic[i].y = (int16_t)(PLAYER_Y - r->traffic[i].road_position_units /
                                               DISTANCE_UNITS_PER_PIXEL);
  }
  scrollOverlay(fences, r->moving_fences, 0);
  scrollOverlay(roadLines, r->moving_lines, 0);
}

// Punto de enganche con el menu general de KUBIX: menu/ ejecuta este juego
// como programa aparte, asi que volver al menu general termina el programa.
static void backToGeneralMenu(int *running) { *running = 0; }

static const char *const menu_labels[MENU_COUNT] = {"SINGLEPLAYER", "TUTORIAL",
                                                    "SCORES", "EXIT"};

// Los 5 mejores tiempos (segundos, de menor a mayor); 0 = casilla vacia.
static uint16_t top_times[TOP_N];

// Sonidos (WAV en assets/, se leen desde build/). Un id invalido no suena.
static SoundID snd_menu, snd_ok, snd_start, snd_engine, snd_crash, snd_finish;

static void loadSounds(void) {
  snd_menu = initSound("../assets/roadfighter_menu.wav");
  snd_ok = initSound("../assets/roadfighter_aceptar.wav");
  snd_start = initSound("../assets/roadfighter_arranque.wav");
  snd_engine = initSound("../assets/roadfighter_motor.wav");
  snd_crash = initSound("../assets/roadfighter_choque.wav");
  snd_finish = initSound("../assets/roadfighter_meta.wav");
}

// Inserta el tiempo en la tabla; 1 si entro al top.
static int registerTime(uint16_t top[TOP_N], uint16_t seconds) {
  int i = TOP_N - 1;
  if (top[i] != 0 && seconds >= top[i])
    return 0;
  while (i > 0 && (top[i - 1] == 0 || top[i - 1] > seconds)) {
    top[i] = top[i - 1];
    --i;
  }
  top[i] = seconds;
  return 1;
}

// Todo en cero = negro.
static const uint16_t black_pixels[SCREEN_SIZE * BLACK_H];

// Flecha del menu (naranja, como el titulo); 0x0843 es el color transparente.
#define OR 0xFC00
#define TR 0x0843
static const uint16_t menu_cursor[3 * 5] = {OR, TR, TR, OR, OR, TR, OR, OR,
                                            OR, OR, OR, TR, OR, TR, TR};
#undef OR
#undef TR

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

static void drawMenu(int selected) {
  sendBackground(MainMenu);
  // El fondo trae tres iconos de modos de juego: se borran y se escribe la
  // lista del diagrama (la misma de Pong y Tetris), con la flecha en la elegida.
  sendSprite(black_pixels, 0, MENU_CLEAR_Y, SCREEN_SIZE,
             SCREEN_SIZE - MENU_CLEAR_Y);
  for (int i = 0; i < MENU_COUNT; ++i) {
    uint8_t y = (uint8_t)(MENU_LIST_Y + i * MENU_STEP);
    if (i == selected)
      sendSprite(menu_cursor, MENU_CURSOR_X, y, 3, 5);
    drawText(menu_labels[i], MENU_TEXT_X, y);
  }
}

static void drawTutorial(void) {
  drawBig("CONTROLS", 1);
  drawBig("UP: GAS", 10);
  drawBig("DOWN: SLOW", 19);
  drawBig("L R: LANE", 28);
  drawBig("START:", 37);
  drawBig("PAUSE", 46);
  drawBig("B: BACK", 55);
}

static void drawScores(void) {
  char line[16];
  drawCentered("TOP 5", 3);
  for (int i = 0; i < TOP_N; ++i) {
    if (top_times[i] > 0)
      snprintf(line, sizeof(line), "%d. %02u:%02u", i + 1,
               (unsigned)(top_times[i] / 60), (unsigned)(top_times[i] % 60));
    else
      snprintf(line, sizeof(line), "%d. --:--", i + 1);
    drawBig(line, (uint8_t)(12 + i * 9));
  }
  drawCentered("B: BACK", 58);
}

int main(int argc, char *argv[]) {
  if (initScreen(argc, argv) != 0)
    return 1;
  loadSounds();

  int running = 1;
  static Race race;
  AppScreen current_screen = SCREEN_STARTMENU;
  int menu_selected = MENU_SINGLEPLAYER;
  int finish_wait = 0;
  int engine_wait = 0;
  uint8_t previous = 0;
  uint32_t last_frame_ticks = SDL_GetTicks();
  initRace(&race);

  while (running) {
    uint32_t frame_ticks = SDL_GetTicks();
    uint32_t frame_delta = frame_ticks - last_frame_ticks;
    last_frame_ticks = frame_ticks;
    uint8_t controls = readControls(&running, &default_keys);
    // En los menus solo cuenta el flanco de subida: mantener un boton no lo
    // repite. En la carrera se usa el estado mantenido.
    uint8_t pressed = (uint8_t)(controls & ~previous);
    previous = controls;
    int confirm = (pressed & (NES_BIT_A | NES_BIT_START)) != 0;
    int back = (pressed & NES_BIT_B) != 0;

    switch (current_screen) {
    case SCREEN_STARTMENU:
      if ((pressed & NES_BIT_UP) && menu_selected > 0) {
        --menu_selected;
        playSound(snd_menu);
      }
      if ((pressed & NES_BIT_DOWN) && menu_selected < MENU_COUNT - 1) {
        ++menu_selected;
        playSound(snd_menu);
      }
      if (back) {
        backToGeneralMenu(&running);
      } else if (confirm) {
        playSound(menu_selected == MENU_SINGLEPLAYER ? snd_start : snd_ok);
        switch (menu_selected) {
        case MENU_SINGLEPLAYER:
          initRace(&race);
          finish_wait = 0;
          engine_wait = 0;
          current_screen = SCREEN_GAME;
          break;
        case MENU_TUTORIAL:
          current_screen = SCREEN_TUTORIAL;
          break;
        case MENU_SCORES:
          current_screen = SCREEN_SCORES;
          break;
        default:
          backToGeneralMenu(&running);
          break;
        }
      }
      break;

    case SCREEN_TUTORIAL:
    case SCREEN_SCORES:
      if (back || confirm) {
        playSound(snd_menu);
        current_screen = SCREEN_STARTMENU;
      }
      break;

    case SCREEN_PAUSE:
      if (pressed & NES_BIT_A) {
        current_screen = SCREEN_GAME;
      } else if (back) {
        current_screen = SCREEN_STARTMENU;
      }
      break;

    case SCREEN_GAME:
      if (race.game_finished && finish_wait > 0)
        --finish_wait;
      if (race.game_finished && finish_wait == 0 &&
          (pressed & (NES_BIT_A | NES_BIT_B))) {
        current_screen = SCREEN_STARTMENU;
      } else if (controls & NES_BIT_START) {
        current_screen = SCREEN_PAUSE;
      } else {
        if (!race.game_finished) {
          race.elapsed_ms += frame_delta;
          updatePlayer(controls, &race.player_x, &race.player_y,
                       &race.player_speed, &race.player_moving_forward,
                       frame_delta, &race.player_velocity_milli,
                       &race.player_movement_remainder,
                       &race.velocity_sum_milli, &race.frame_movement_pixels);
          // El motor late mas seguido cuanto mas rapido va el auto.
          if (race.player_speed > 0 && race.player_crash_timer == 0 &&
              --engine_wait <= 0) {
            playSound(snd_engine);
            engine_wait = race.player_speed >= 9 ? 3 : 12 - race.player_speed;
          }
          if (race.player_moving_forward) {
            uint32_t goal_units =
                DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER;
            race.distance_progress_remainder +=
                race.velocity_sum_milli * frame_delta *
                DISTANCE_UNITS_PER_PIXEL;
            race.player_distance_units +=
                race.distance_progress_remainder / 2000000;
            race.distance_progress_remainder %= 2000000;
            if (race.player_distance_units > goal_units)
              race.player_distance_units = goal_units;
          }

          if (race.player_distance_units >=
              DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER) {
            race.game_finished = 1;
            playSound(snd_finish);
            registerTime(top_times, (uint16_t)(race.elapsed_ms / 1000));
            finish_wait = FINISH_WAIT;
          }

          advanceScroll(&race.fence_scroll, race.frame_movement_pixels, 1);
          advanceScroll(&race.line_scroll, race.frame_movement_pixels, 1);
        }
        unsigned collisions_before = race.collisions;
        updateTraffic(race.traffic, race.player_x, race.player_y,
                      race.player_distance_units, &race.collisions,
                      &race.player_crash_timer, !race.game_finished);
        if (race.collisions > collisions_before)
          playSound(snd_crash);
        scrollOverlay(fences, race.moving_fences, race.fence_scroll);
        scrollOverlay(roadLines, race.moving_lines, race.line_scroll);
      }
      break;

    case SCREEN_SETTINGS:
      if (controls & NES_BIT_SELECT)
        current_screen = SCREEN_PAUSE;
      break;
    }

    SDL_SetRenderDrawColor(global_renderer, 0, 0, 0, 255);
    SDL_RenderClear(global_renderer);

    switch (current_screen) {
    case SCREEN_STARTMENU:
      drawMenu(menu_selected);
      break;
    case SCREEN_TUTORIAL:
      drawTutorial();
      break;
    case SCREEN_SCORES:
      drawScores();
      break;
    case SCREEN_PAUSE:
      sendBackground(PauseMenu);
      break;
    case SCREEN_GAME:
      drawGame(race.player_x, race.player_y, race.traffic,
               race.elapsed_ms / 1000,
               race.player_distance_units / DISTANCE_UNITS_PER_METER,
               race.game_finished, race.game_finished && finish_wait == 0,
               race.player_crash_timer, race.moving_fences,
               race.moving_lines);
      break;
    case SCREEN_SETTINGS:
      sendBackground(Road);
      break;
    }

    SDL_RenderPresent(global_renderer);
    SDL_Delay(30);
  }

  SDL_Quit();
  return 0;
}
