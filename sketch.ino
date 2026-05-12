#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define JOY_X 35
#define JOY_Y 34
#define JOY_SW 27
#define BUZZER 13

#define GRID_SIZE 4
#define GRID_W 32
#define GRID_H 16

#define MAX_SNAKE 120

struct Segment {
  int x;
  int y;
};

Segment snake[MAX_SNAKE];

int snakeLength = 5;

int dirX = 1;
int dirY = 0;

int nextDirX = 1;
int nextDirY = 0;

int foodX;
int foodY;

bool gameOver = false;

int centerX = 0;
int centerY = 0;

unsigned long lastMove = 0;
int gameSpeed = 90;

int score = 0;

void calibrateJoystick() {
  long sx = 0;
  long sy = 0;

  for (int i = 0; i < 25; i++) {
    sx += analogRead(JOY_X);
    sy += analogRead(JOY_Y);
    delay(10);
  }

  centerX = sx / 25;
  centerY = sy / 25;
}

void beep(int freq, int duration) {
  ledcWriteTone(0, freq);
  delay(duration);
  ledcWriteTone(0, 0);
}

void introScreen() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_logisoso20_tf);
  u8g2.drawStr(18, 30, "SNAKE");

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(24, 52, "ESP32 EDITION");

  u8g2.sendBuffer();

  beep(900, 80);
  delay(700);
}

void spawnFood() {
  bool valid = false;

  while (!valid) {
    valid = true;

    foodX = random(0, GRID_W);
    foodY = random(0, GRID_H);

    for (int i = 0; i < snakeLength; i++) {
      if (snake[i].x == foodX && snake[i].y == foodY) {
        valid = false;
      }
    }
  }
}

void resetGame() {
  snakeLength = 5;

  snake[0] = {10, 8};
  snake[1] = {9, 8};
  snake[2] = {8, 8};
  snake[3] = {7, 8};
  snake[4] = {6, 8};

  dirX = 1;
  dirY = 0;

  nextDirX = 1;
  nextDirY = 0;

  score = 0;

  gameSpeed = 90;

  gameOver = false;

  spawnFood();
}

void readJoystick() {

  int xVal = analogRead(JOY_X);
  int yVal = analogRead(JOY_Y);

  int dx = xVal - centerX;
  int dy = yVal - centerY;

  int threshold = 700;

  if (abs(dx) > abs(dy)) {

    // INVERTIDO HORIZONTAL
    if (dx > threshold && dirX != 1) {
      nextDirX = -1;
      nextDirY = 0;
    }

    else if (dx < -threshold && dirX != -1) {
      nextDirX = 1;
      nextDirY = 0;
    }

  } else {

    // INVERTIDO VERTICAL
    if (dy > threshold && dirY != 1) {
      nextDirX = 0;
      nextDirY = -1;
    }

    else if (dy < -threshold && dirY != -1) {
      nextDirX = 0;
      nextDirY = 1;
    }
  }
}

void moveSnake() {

  dirX = nextDirX;
  dirY = nextDirY;

  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }

  snake[0].x += dirX;
  snake[0].y += dirY;

  if (snake[0].x < 0) snake[0].x = GRID_W - 1;
  if (snake[0].x >= GRID_W) snake[0].x = 0;

  if (snake[0].y < 0) snake[0].y = GRID_H - 1;
  if (snake[0].y >= GRID_H) snake[0].y = 0;

  for (int i = 1; i < snakeLength; i++) {
    if (snake[0].x == snake[i].x &&
        snake[0].y == snake[i].y) {

      gameOver = true;
      return;
    }
  }

  if (snake[0].x == foodX &&
      snake[0].y == foodY) {

    if (snakeLength < MAX_SNAKE) {
      snakeLength++;
    }

    score++;

    if (gameSpeed > 40) {
      gameSpeed -= 2;
    }

    beep(1200, 30);

    snake[snakeLength - 1] = snake[snakeLength - 2];

    spawnFood();
  }
}

void drawSnake() {

  for (int i = 0; i < snakeLength; i++) {

    int px = snake[i].x * GRID_SIZE;
    int py = snake[i].y * GRID_SIZE;

    if (i == 0) {
      u8g2.drawBox(px, py, GRID_SIZE, GRID_SIZE);
    } else {
      u8g2.drawFrame(px, py, GRID_SIZE, GRID_SIZE);
    }
  }
}

void drawFood() {

  int px = foodX * GRID_SIZE;
  int py = foodY * GRID_SIZE;

  u8g2.drawDisc(px + 2, py + 2, 2);
}

void drawHUD() {

  u8g2.setFont(u8g2_font_5x8_tf);

  char buf[20];

  sprintf(buf, "Score:%d", score);

  u8g2.drawStr(2, 63, buf);
}

void gameOverScreen() {

  for (int i = 1000; i > 200; i -= 80) {
    ledcWriteTone(0, i);
    delay(20);
  }

  ledcWriteTone(0, 0);

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_logisoso16_tf);
  u8g2.drawStr(10, 28, "GAME");

  u8g2.drawStr(12, 54, "OVER");

  u8g2.sendBuffer();

  delay(1500);

  resetGame();
}

void setup() {

  u8g2.begin();

  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  ledcAttach(BUZZER, 5000, 8);

  randomSeed(analogRead(32));

  calibrateJoystick();

  introScreen();

  resetGame();
}

void loop() {

  readJoystick();

  if (millis() - lastMove > gameSpeed) {

    lastMove = millis();

    moveSnake();
  }

  u8g2.clearBuffer();

  drawSnake();
  drawFood();
  drawHUD();

  u8g2.sendBuffer();

  if (gameOver) {
    gameOverScreen();
  }

  if (!digitalRead(JOY_SW)) {
    resetGame();
    delay(250);
  }
}
