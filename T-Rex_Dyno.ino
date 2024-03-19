/* Includes */
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#include "assets/cacti.h"
#include "assets/game-over.h"
#include "assets/ground.h"
#include "assets/hearts.h"
#include "assets/ptero.h"
#include "assets/splash-screen.h"
#include "assets/trex-duck.h"
#include "assets/trex-up.h"

/* Hardware Connections */
#define JUMP_SENSOR 2
#define DUCK_SENSOR 3
#define LIFE_LED1 13
#define LIFE_LED2 12
#define LIFE_LED3 11
#define BUZZER 10

/* OLED reset pin; same as Arduino */
#define RESET_PIN -1

/* Game Balance Settings */
#define T_REX_JUMP_HEIGHT 25
#define T_REX_JUMP_SPEED 3
#define T_REX_JUMP_HOVER 4
#define T_REX_FALL_SPEED 2
#define T_REX_INVINCIBILITY 20

#define MIN_CACTI_RESPAWN_TIME 50
#define MAX_CACTI_RESPAWN_TIME 100
#define CACTI_SCROLL_SPEED 3

#define MIN_PTERO_RESPAWN_TIME 90
#define MAX_PTERO_RESPAWN_TIME 180
#define PTERO_SCROLL_SPEED 5

/* Display Settings */
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128

#define DAY_NIGHT_CYCLE 200

#define T_REX_POS_X 5
#define T_REX_POS_Y 30
#define T_REX_UP_WIDTH 22
#define T_REX_UP_HEIGHT 23
#define T_REX_DUCK_WIDTH 29
#define T_REX_DUCK_HEIGHT 15

#define CACTI_POS_Y 30
#define CACTI_WIDTH 27
#define CACTI_HEIGHT 26

#define PTERO_POS_Y 20
#define PTERO_WIDTH 23
#define PTERO_HEIGHT 20

#define MAIN_MENU_SCENE 0
#define OPTIONS_MENU_SCENE 1
#define GAME_SCENE 2

/* Defines and globals */
#define EEPROM_HI_SCORE 16  // 2 bytes
#define JUMP 0
#define DUCK 1
static uint16_t hiScore = 0;
static byte scene = MAIN_MENU_SCENE;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, RESET_PIN);

inline bool isJumpDetected() { return !digitalRead(JUMP_SENSOR); }
inline bool isDuckDetected() { return !digitalRead(DUCK_SENSOR); }
inline void playSound() {
  digitalWrite(BUZZER, HIGH);
  delay(80);
  digitalWrite(BUZZER, LOW);
}

struct Obstacle {
  int16_t x;
  int16_t y;

  const uint8_t* sprite;
  byte frame = 0;
};

bool isAABBCollision(int16_t x1, int16_t x2, int16_t y1, int16_t y2,
                     int16_t width1, int16_t width2, int16_t height1,
                     int16_t height2) {
  if (x1 < x2 + width2 && x1 + width1 > x2 && y1 < y2 + height2 &&
      y1 + height1 > y2)
    return true;
  return false;
}

void takeDamage(byte& lives) {
  lives--;
  switch (lives) {
    case 2:
      digitalWrite(LIFE_LED3, LOW);
      break;
    case 1:
      digitalWrite(LIFE_LED2, LOW);
      break;
    case 0:
      digitalWrite(LIFE_LED1, LOW);
      break;
  }
}

void increaseHealth(byte& lives) {
  switch (lives) {
    case 2:
      digitalWrite(LIFE_LED3, HIGH);
      break;
    case 1:
      digitalWrite(LIFE_LED2, HIGH);
      break;
  }
  lives++;
}

void waitForRelease(byte sensor_select) {
  if (sensor_select == JUMP) {
    /* wait for release */
    while (isJumpDetected()) {
      delay(100);
    }
  } else {
    /* wait for release */
    while (isDuckDetected()) {
      delay(100);
    }
  }
}

void waitForPressAndRelease(byte sensor_select) {
  if (sensor_select == JUMP) {
    /* wait for press */
    while (!isJumpDetected()) {
      delay(100);
    }

    /* wait for release */
    while (isJumpDetected()) {
      delay(100);
    }
  } else if (sensor_select == DUCK) {
    /* wait for press */
    while (!isDuckDetected()) {
      delay(100);
    }

    /* wait for release */
    while (isDuckDetected()) {
      delay(100);
    }
  }
}

void gameLoop() {
  /* game variables */
  bool gameOver = false;
  uint16_t score = 0;
  byte frame_time = 100;
  uint16_t score_milestone = 100;
  uint16_t weather_milestone = DAY_NIGHT_CYCLE;
  bool weather = false;
  byte lives = 3;

  digitalWrite(LIFE_LED1, HIGH);
  digitalWrite(LIFE_LED2, HIGH);
  digitalWrite(LIFE_LED3, HIGH);

  /* trex states */
  bool grounded = true;
  bool falling = false;
  bool ducking = false;
  bool blinking = false;
  int16_t posY = T_REX_POS_Y;
  byte time_damage_taken = T_REX_INVINCIBILITY;
  byte t_rex_jump_speed = T_REX_JUMP_SPEED;
  byte t_rex_fall_speed = T_REX_FALL_SPEED;
  byte t_rex_jump_hover = 0;

  /* animation frames */
  byte t_rex_up_frame = 0;
  const uint8_t* t_rex_up_anim[] = {trex_up1_bitmap, trex_up2_bitmap,
                                    trex_up3_bitmap};
  byte t_rex_duck_frame = 0;
  const uint8_t* t_rex_duck_anim[] = {trex_duck1_bitmap, trex_duck2_bitmap};

  byte ground_sprite = 0;
  const uint8_t* ground_sprites[] = {ground1_bitmap, ground2_bitmap,
                                     ground3_bitmap, ground4_bitmap,
                                     ground5_bitmap};
  const uint8_t* cacti_sprites[] = {cacti1_bitmap, cacti2_bitmap, cacti3_bitmap,
                                    cacti4_bitmap};

  const uint8_t* ptero_anim[] = {ptero1_bitmap, ptero2_bitmap};

  byte cacti_scroll_speed = CACTI_SCROLL_SPEED;
  byte ptero_scroll_speed = PTERO_SCROLL_SPEED;

  Obstacle cacti[3];
  byte time_taken_cacti_spawned = 0;
  byte respawn_time_cacti =
      random(MIN_CACTI_RESPAWN_TIME + 30, MAX_CACTI_RESPAWN_TIME);

  Obstacle ptero[3];
  byte time_taken_ptero_spawned = 0;
  byte respawn_time_ptero =
      random(MIN_PTERO_RESPAWN_TIME + 50, MAX_PTERO_RESPAWN_TIME);

  Obstacle heart;

  /* hide obstacles at start */
  for (int i = 0; i < 3; i++) {
    cacti[i].x = -100;
    ptero[i].x = -100;
  }
  heart.x = -100;
  heart.y = CACTI_POS_Y;
  heart.sprite = hearts_bitmap;

  while (true) {
    if (lives == 0) {
      gameOver = true;
    }

    if (gameOver) {
      if (score > hiScore) hiScore = score;
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(15, SCREEN_HEIGHT / 2 - 15);
      display.print("G A M E  O V E R");
      display.drawBitmap(SCREEN_WIDTH / 2 - 10, SCREEN_HEIGHT / 2,
                         game_over_bitmap, 19, 14, WHITE);
      display.display();

      /* saves high score to EEPROM */
      EEPROM.put(EEPROM_HI_SCORE, hiScore);

      waitForPressAndRelease(JUMP);
      display.invertDisplay(false);
      break;
    }

    /* check collisions */
    for (int i = 0; i < 3; i++) {
      if (ducking) {
        if (isAABBCollision(T_REX_POS_X, cacti[i].x, posY, cacti[i].y,
                            T_REX_DUCK_WIDTH - 3, CACTI_WIDTH - 7,
                            T_REX_DUCK_HEIGHT - 3, CACTI_HEIGHT - 4) ||
            isAABBCollision(T_REX_POS_X, ptero[i].x, posY, ptero[i].y,
                            T_REX_DUCK_WIDTH, PTERO_WIDTH, T_REX_DUCK_HEIGHT,
                            PTERO_HEIGHT - 3)) {
          if (time_damage_taken >= T_REX_INVINCIBILITY) {
            takeDamage(lives);
            time_damage_taken = 0;
            break;
          }
        }
      } else {
        if (heart.x > 0)
          if (isAABBCollision(T_REX_POS_X, heart.x, posY, heart.y,
                              T_REX_UP_WIDTH, 15, T_REX_UP_HEIGHT, 15)) {
            if (lives < 3) {
              increaseHealth(lives);
              heart.x = -SCREEN_WIDTH;
            }
          }

        if (isAABBCollision(T_REX_POS_X, cacti[i].x, posY, cacti[i].y,
                            T_REX_UP_WIDTH - 6, CACTI_WIDTH - 7,
                            T_REX_UP_HEIGHT - 3, CACTI_HEIGHT - 4) ||
            isAABBCollision(T_REX_POS_X, ptero[i].x, posY, ptero[i].y,
                            T_REX_UP_WIDTH, PTERO_WIDTH, T_REX_UP_WIDTH,
                            PTERO_HEIGHT - 3)) {
          if (time_damage_taken >= T_REX_INVINCIBILITY) {
            takeDamage(lives);
            time_damage_taken = 0;
            break;
          }
        }
      }
    }
    time_damage_taken++;

    /* apply velocity on y-axis */
    if (!grounded) {
      if (falling) {
        posY += t_rex_fall_speed;
        if (ducking) {
          if (posY >= T_REX_POS_Y + 8) {
            falling = false;
            grounded = true;
            posY = T_REX_POS_Y + 8;
          }
        } else {
          if (posY >= T_REX_POS_Y) {
            falling = false;
            grounded = true;
            posY = T_REX_POS_Y;
          }
        }

      } else {
        if (T_REX_POS_Y - posY >= T_REX_JUMP_HEIGHT) {
          if (t_rex_jump_hover >= T_REX_JUMP_HOVER) {
            falling = true;
            posY = T_REX_POS_Y - T_REX_JUMP_HEIGHT;
            t_rex_jump_hover = 0;
          } else {
            t_rex_jump_hover++;
          }
        } else
          posY -= t_rex_jump_speed;
      }
    }

    /* handle jump if not in air */
    else if (grounded && isJumpDetected()) {
      grounded = false;
      t_rex_up_frame = 0;
      playSound();
    }

    /* handle duck if not ducking */
    if (!ducking && isDuckDetected()) {
      ducking = true;
      t_rex_duck_frame = 0;
      posY += 8;
      playSound();
    } else if (ducking && !isDuckDetected()) {
      ducking = false;
      posY -= 8;
    }

    /* move cacti left */
    for (int i = 0; i < 3; i++) {
      if (cacti[i].x < -CACTI_WIDTH) continue;
      cacti[i].x -= cacti_scroll_speed;
    }

    /* move ptero left */
    for (int i = 0; i < 3; i++) {
      if (ptero[i].x < -PTERO_WIDTH) continue;
      ptero[i].x -= ptero_scroll_speed;
    }

    /* move heart left */
    if (heart.x > 0) {
      heart.x -= cacti_scroll_speed;
    }

    /* spawn cacti */
    time_taken_cacti_spawned++;
    if (time_taken_cacti_spawned >= respawn_time_cacti) {
      for (int i = 0; i < 3; i++) {
        if (cacti[i].x >= -CACTI_WIDTH) continue;
        cacti[i].x = SCREEN_WIDTH;
        cacti[i].y = CACTI_POS_Y;
        cacti[i].sprite = cacti_sprites[random(0, 4)];
        break;
      }
      time_taken_cacti_spawned = 0;
      respawn_time_cacti =
          random(MIN_CACTI_RESPAWN_TIME, MAX_CACTI_RESPAWN_TIME);
    }

    /* spawn ptero */
    time_taken_ptero_spawned++;
    if (time_taken_ptero_spawned >= respawn_time_ptero) {
      for (int i = 0; i < 3; i++) {
        if (ptero[i].x >= -PTERO_WIDTH) continue;
        ptero[i].x = SCREEN_WIDTH;
        ptero[i].y = PTERO_POS_Y;
        ptero[i].frame = 0;
        break;
      }
      time_taken_ptero_spawned = 0;
      respawn_time_ptero =
          random(MIN_PTERO_RESPAWN_TIME, MAX_PTERO_RESPAWN_TIME);
    }

    /* increase score */
    if (score < 0xFFFE) score++;

    /* increase game speed */
    if (score >= score_milestone) {
      frame_time -= 10;
      score_milestone *= 2;
      cacti_scroll_speed++;
      ptero_scroll_speed++;
      t_rex_jump_speed++;
      t_rex_fall_speed++;
    }

    /* revert colors to simulate time passing */
    if (score >= weather_milestone) {
      weather_milestone += DAY_NIGHT_CYCLE;
      weather = !weather;
      heart.x = SCREEN_WIDTH;
      display.invertDisplay(weather);
    }

    /* render cycle */
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    /* render score */
    display.setCursor(50, 0);
    display.print("HI ");
    display.print(hiScore);
    display.print(' ');
    display.print(score);

    /* render life */
    for (int i = 0; i < lives; i++) {
      display.drawBitmap(i * 15, 0, hearts_bitmap, 15, 15, WHITE);
    }

    /* render heart */
    if (heart.x > 0)
      display.drawBitmap(heart.x, heart.y, hearts_bitmap, 15, 15, WHITE);

    /* render ground */
    display.drawBitmap(0, 52, ground_sprites[random(0, 5)], SCREEN_WIDTH, 12,
                       WHITE);
    ground_sprite = ground_sprite + 1 >= 5 ? 0 : ground_sprite + 1;

    /* render t-rex */
    byte color = WHITE;
    if (time_damage_taken <= T_REX_INVINCIBILITY) {
      if (blinking) {
        color = BLACK;
      }
      blinking = !blinking;
    }
    if (ducking) {
      display.drawBitmap(T_REX_POS_X, posY, t_rex_duck_anim[t_rex_duck_frame],
                         T_REX_DUCK_WIDTH, T_REX_DUCK_HEIGHT, color);
      t_rex_duck_frame = t_rex_duck_frame + 1 >= 2 ? 0 : t_rex_duck_frame + 1;
    } else if (!grounded)
      display.drawBitmap(T_REX_POS_X, posY, t_rex_up_anim[0], T_REX_UP_WIDTH,
                         T_REX_UP_HEIGHT, color);
    else {
      display.drawBitmap(T_REX_POS_X, posY, t_rex_up_anim[t_rex_up_frame], 22,
                         23, color);
      t_rex_up_frame = t_rex_up_frame + 1 >= 3 ? 0 : t_rex_up_frame + 1;
    }

    /* render cacti */
    for (int i = 0; i < 3; i++) {
      if (cacti[i].x < -CACTI_WIDTH) continue;
      display.drawBitmap(cacti[i].x, cacti[i].y, cacti[i].sprite, CACTI_WIDTH,
                         CACTI_HEIGHT, WHITE);
    }

    /* render ptero */
    for (int i = 0; i < 3; i++) {
      if (ptero[i].x < -PTERO_WIDTH) continue;
      display.drawBitmap(ptero[i].x, ptero[i].y, ptero_anim[ptero[i].frame],
                         PTERO_WIDTH, PTERO_HEIGHT, WHITE);
      ptero[i].frame = ptero[i].frame + 1 >= 2 ? 0 : ptero[i].frame + 1;
    }
    display.display();
    delay(frame_time);
  }
}

void render_SplashScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);

  display.drawBitmap(0, 0, splash_screen_bitmap, SCREEN_WIDTH, SCREEN_HEIGHT,
                     WHITE);

  display.display();
  while (!isJumpDetected()) {
    delay(100);
  }

  playSound();
  /* wait for release */
  while (isJumpDetected()) {
    delay(100);
  }
}

void render_Choices(const char** choices, int16_t start_pos, byte no_choices,
                    byte selected) {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  for (int i = 0; i < no_choices; i++) {
    display.setCursor(start_pos, i * SCREEN_HEIGHT / 2);
    if (i == selected) display.print(">");
    display.print(choices[i]);
  }
  display.display();
}

void render_OptionsMenu() {
  byte selected = 0;
  const char* choices[] = {"RESETEAZA SCOR", "INAPOI"};

  render_Choices(choices, 0, 2, selected);
  display.display();
  while (true) {
    if (isDuckDetected()) {
      playSound();
      waitForRelease(DUCK);
      selected = !selected;
      render_Choices(choices, 0, 2, selected);
      display.display();
    }
    if (isJumpDetected()) {
      playSound();
      waitForRelease(JUMP);

      if (!selected) {
        scene = MAIN_MENU_SCENE;
        /* put 0 in EEPROM memory to reset high score */
        hiScore = 0;
        EEPROM.put(EEPROM_HI_SCORE, &hiScore);
      } else {
        scene = MAIN_MENU_SCENE;
      }
      return;
    }
    delay(100);
  }
}

void render_MainMenu() {
  byte selected = 0;
  const char* choices[] = {"JOC NOU", "SETARI"};

  render_Choices(choices, SCREEN_WIDTH / 2, 2, selected);
  display.drawBitmap(0, 0, main_menu_avatar_bitmap, 50, 50, WHITE);
  display.display();
  while (true) {
    if (isDuckDetected()) {
      playSound();
      waitForRelease(DUCK);
      selected = !selected;
      render_Choices(choices, SCREEN_WIDTH / 2, 2, selected);
      display.drawBitmap(0, 0, main_menu_avatar_bitmap, 50, 50, WHITE);
      display.display();
    }
    if (isJumpDetected()) {
      playSound();
      waitForRelease(JUMP);

      if (!selected) {
        scene = GAME_SCENE;
      } else {
        scene = OPTIONS_MENU_SCENE;
      }
      return;
    }
    delay(100);
  }
}

void setup() {
  pinMode(JUMP_SENSOR, INPUT_PULLUP);
  pinMode(DUCK_SENSOR, INPUT_PULLUP);
  pinMode(LIFE_LED1, OUTPUT);
  pinMode(LIFE_LED2, OUTPUT);
  pinMode(LIFE_LED3, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  Serial.begin(250000);

  /* error on init then print message and hang */
  if (!display.begin(SSD1306_SWITCHCAPVCC,
                     0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  randomSeed(analogRead(A0));
  EEPROM.get(EEPROM_HI_SCORE, hiScore);

  /* reset high score to avoid overflow */
  if (hiScore == 0xFFFF) hiScore = 0;

  render_SplashScreen();
}

void loop() {
  scene = MAIN_MENU_SCENE;

  /* return to main menu */
  render_MainMenu();
  while (scene != GAME_SCENE) {
    if (scene == MAIN_MENU_SCENE) {
      render_MainMenu();
    } else if (scene == OPTIONS_MENU_SCENE) {
      render_OptionsMenu();
    }
  }

  /* start game */
  gameLoop();
}
