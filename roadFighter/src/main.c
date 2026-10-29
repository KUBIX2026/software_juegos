#include <string.h>
#include "assets.h"
#include "hal.h"

typedef enum {
  SCREEN_PAUSE,
  SCREEN_STARTMENU,
  SCREEN_GAME,
  SCREEN_SETTINGS
} AppScreen;

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
  TRAFFIC_AFTER_GOAL_UNITS = (SCREEN_SIZE + 21) * DISTANCE_UNITS_PER_PIXEL
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
                     uint8_t game_finished, uint8_t player_crash_timer,
                     const uint16_t *fence_layer, const uint16_t *line_layer) {
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
    uint16_t banner[25 * 7];
    for (int i = 0; i < 25 * 7; ++i)
      banner[i] = 0x0000;
    sendSprite(banner, 12, 30, 25, 7);
    sendString("FINISH", 13, 31, 3, 5);
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

int main(int argc, char *argv[]) {
  if (initScreen(argc, argv) != 0)
    return 1;

  int running = 1;
  uint8_t player_x = lane_x[1];
  uint8_t player_y = PLAYER_Y;
  uint8_t player_speed = 0;
  uint8_t player_moving_forward = 1;
  uint8_t frame_movement_pixels = 0;
  uint32_t player_velocity_milli = 0;
  uint32_t velocity_sum_milli = 0;
  uint32_t player_movement_remainder = 0;
  unsigned collisions = 0;
  uint8_t fence_scroll = 0;
  uint8_t line_scroll = 0;
  uint8_t game_finished = 0;
  uint8_t player_crash_timer = 0;
  uint32_t player_distance_units = 0;
  uint32_t distance_progress_remainder = 0;
  uint32_t elapsed_ms = 0;
  uint32_t last_frame_ticks = SDL_GetTicks();
  uint16_t moving_fences[SCREEN_SIZE * SCREEN_SIZE];
  uint16_t moving_lines[SCREEN_SIZE * SCREEN_SIZE];
  AppScreen current_screen = SCREEN_GAME;

  // FEATURE: I want the cars to be at a random position.
  TrafficCar traffic[TRAFFIC_COUNT] = {
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
  for (int i = 0; i < TRAFFIC_COUNT; ++i) {
    traffic[i].road_position_units =
        (uint16_t)(DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER *
                   traffic[i].goal_percent / 100);
    traffic[i].speed = TRAFFIC_MIN_SPEED;
    traffic[i].y = (int16_t)(PLAYER_Y - traffic[i].road_position_units /
                                            DISTANCE_UNITS_PER_PIXEL);
  }
  scrollOverlay(fences, moving_fences, 0);
  scrollOverlay(roadLines, moving_lines, 0);

  while (running) {
    uint32_t frame_ticks = SDL_GetTicks();
    uint32_t frame_delta = frame_ticks - last_frame_ticks;
    last_frame_ticks = frame_ticks;
    uint8_t controls = readControls(&running, &default_keys);

    switch (current_screen) {
    case SCREEN_STARTMENU:
      if (controls & NES_BIT_A)
        current_screen = SCREEN_GAME;
      break;

      break;
    case SCREEN_PAUSE:
      if (controls & NES_BIT_A) {
        current_screen = SCREEN_GAME;
      } else if (controls & NES_BIT_B) {
        current_screen = SCREEN_STARTMENU;
      }
      break;

    case SCREEN_GAME:
      if (controls & NES_BIT_START) {
        current_screen = SCREEN_PAUSE;
      } else {
        if (!game_finished) {
          elapsed_ms += frame_delta;
          updatePlayer(controls, &player_x, &player_y, &player_speed,
                       &player_moving_forward, frame_delta,
                       &player_velocity_milli,
                       &player_movement_remainder,
                       &velocity_sum_milli,
                       &frame_movement_pixels);
          if (player_moving_forward) {
            uint32_t goal_units =
                DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER;
            distance_progress_remainder +=
                velocity_sum_milli * frame_delta *
                DISTANCE_UNITS_PER_PIXEL;
            player_distance_units += distance_progress_remainder / 2000000;
            distance_progress_remainder %= 2000000;
            if (player_distance_units > goal_units)
              player_distance_units = goal_units;
          }

          if (player_distance_units >=
              DISTANCE_GOAL_METERS * DISTANCE_UNITS_PER_METER) {
            game_finished = 1;
          }

          advanceScroll(&fence_scroll, frame_movement_pixels, 1);
          advanceScroll(&line_scroll, frame_movement_pixels, 1);
        }
        updateTraffic(traffic, player_x, player_y, player_distance_units,
                      &collisions, &player_crash_timer, !game_finished);
        scrollOverlay(fences, moving_fences, fence_scroll);
        scrollOverlay(roadLines, moving_lines, line_scroll);
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
      sendBackground(MainMenu);
      break;
    case SCREEN_PAUSE:
      sendBackground(PauseMenu);
      break;
    case SCREEN_GAME:
      drawGame(player_x, player_y, traffic, elapsed_ms / 1000,
               player_distance_units / DISTANCE_UNITS_PER_METER, game_finished,
               player_crash_timer, moving_fences, moving_lines);
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
