#include <Wire.h>
#include <U8g2lib.h>

// Initialize the OLED display using Hardware I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Pin definitions
#define JOY_X 35   // Analog pin for Joystick X-axis
#define JOY_Y 34   // Analog pin for Joystick Y-axis
#define JOY_SW 27  // Digital pin for Joystick Switch (button)
#define BUZZER 13  // Digital pin for the piezo buzzer

// Game grid configurations
#define GRID_SIZE 4   // Each grid unit is 4x4 pixels
#define GRID_W 32     // Total grid width (128 pixels / 4 = 32 columns)
#define GRID_H 16     // Total grid height (64 pixels / 4 = 16 rows)

#define MAX_SNAKE 120 // Maximum possible length of the snake

// Structure to hold coordinates for each snake segment
struct Segment {
  int x;
  int y;
};

Segment snake[MAX_SNAKE]; // Array to store all segments of the snake
int snakeLength = 5;      // Initial snake length

// Current movement direction
int dirX = 1;
int dirY = 0;

// Next movement direction (buffered to prevent rapid double-turns)
int nextDirX = 1;
int nextDirY = 0;

// Food coordinates
int foodX;
int foodY;

bool gameOver = false;

// Calibration variables to store the joystick's center resting position
int centerX = 0;
int centerY = 0;

unsigned long lastMove = 0; // Tracks the timestamp of the last snake movement
int gameSpeed = 90;         // Delay between moves in milliseconds (lower means faster)

int score = 0;

// Reads the joystick multiple times at startup to find its baseline resting values
void calibrateJoystick() {
  long sx = 0;
  long sy = 0;

  for (int i = 0; i < 25; i++) {
    sx += analogRead(JOY_X);
    sy += analogRead(JOY_Y);
    delay(10);
  }

  centerX = sx / 25; // Average X resting value
  centerY = sy / 25; // Average Y resting value
}

// Generates a beep sound using the ESP32 LEDC PWM peripheral
void beep(int freq, int duration) {
  ledcWriteTone(0, freq); // Play tone on channel 0
  delay(duration);
  ledcWriteTone(0, 0);    // Stop tone
}

// Displays the startup/intro screen
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

// Spawns food at a random grid position ensuring it doesn't land on the snake
void spawnFood() {
  bool valid = false;

  while (!valid) {
    valid = true;

    foodX = random(0, GRID_W);
    foodY = random(0, GRID_H);

    // Check if the food coordinates overlap with any part of the snake
    for (int i = 0; i < snakeLength; i++) {
      if (snake[i].x == foodX && snake[i].y == foodY) {
        valid = false; // Re-run the loop if it overlaps
      }
    }
  }
}

// Resets game variables back to their initial state for a new game
void resetGame() {
  snakeLength = 5;

  // Set starting positions for the initial 5 segments
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

// Reads analog joystick values and updates the buffered direction variables
void readJoystick() {
  int xVal = analogRead(JOY_X);
  int yVal = analogRead(JOY_Y);

  // Calculate deviation from the calibrated center
  int dx = xVal - centerX;
  int dy = yVal - centerY;

  int threshold = 700; // Deadzone threshold to avoid accidental inputs

  // Determine which axis has a stronger tilt
  if (abs(dx) > abs(dy)) {
    // Inverted horizontal control logic
    if (dx > threshold && dirX != 1) {
      nextDirX = -1;
      nextDirY = 0;
    }
    else if (dx < -threshold && dirX != -1) {
      nextDirX = 1;
      nextDirY = 0;
    }
  } else {
    // Inverted vertical control logic
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

// Updates the positions of all snake segments and handles collisions
void moveSnake() {
  dirX = nextDirX;
  dirY = nextDirY;

  // Move each body segment forward to the position of the preceding segment
  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }

  // Update head position based on current direction
  snake[0].x += dirX;
  snake[0].y += dirY;

  // Handle screen wrapping (teleporting to the opposite side)
  if (snake[0].x < 0) snake[0].x = GRID_W - 1;
  if (snake[0].x >= GRID_W) snake[0].x = 0;

  if (snake[0].y < 0) snake[0].y = GRID_H - 1;
  if (snake[0].y >= GRID_H) snake[0].y = 0;

  // Check self-collision (if head hits any body segment)
  for (int i = 1; i < snakeLength; i++) {
    if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
      gameOver = true;
      return;
    }
  }

  // Check if the head has reached/eaten the food
  if (snake[0].x == foodX && snake[0].y == foodY) {
    if (snakeLength < MAX_SNAKE) {
      snakeLength++;
    }

    score++;

    // Progressively increase game speed (decrease delay) down to a limit of 40ms
    if (gameSpeed > 40) {
      gameSpeed -= 2;
    }

    beep(1200, 30); // Success beep sound

    // Duplicate the tail segment temporarily to accommodate growth smoothly
    snake[snakeLength - 1] = snake[snakeLength - 2];

    spawnFood();
  }
}

// Renders the snake on the OLED screen buffer
void drawSnake() {
  for (int i = 0; i < snakeLength; i++) {
    // Translate grid positions to absolute pixel coordinates
    int px = snake[i].x * GRID_SIZE;
    int py = snake[i].y * GRID_SIZE;

    if (i == 0) {
      u8g2.drawBox(px, py, GRID_SIZE, GRID_SIZE);    // Draw head as a solid square
    } else {
      u8g2.drawFrame(px, py, GRID_SIZE, GRID_SIZE);  // Draw body segments as hollow squares
    }
  }
}

// Renders the food as a small circle
void drawFood() {
  int px = foodX * GRID_SIZE;
  int py = foodY * GRID_SIZE;

  u8g2.drawDisc(px + 2, py + 2, 2); // Center of the disc offset by 2 pixels
}

// Displays the HUD (Heads-Up Display) with the current score
void drawHUD() {
  u8g2.setFont(u8g2_font_5x8_tf);
  char buf[20];
  sprintf(buf, "Score:%d", score);
  u8g2.drawStr(2, 63, buf);
}

// Plays a losing audio effect and displays the "Game Over" screen
void gameOverScreen() {
  // Descending frequency sweep sound effect
  for (int i = 1000; i > 200; i -= 80) {
    ledcWriteTone(0, i);
    delay(20);
  }
  ledcWriteTone(0, 0); // Turn off buzzer

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

  pinMode(JOY_SW, INPUT_PULLUP); // Setup switch pin with internal pull-up resistor
  pinMode(BUZZER, OUTPUT);

  // Configure ESP32 ADC resolution and attenuation for joystick reading
  analogReadResolution(12); // 12-bit resolution (0 - 4095)
  analogSetAttenuation(ADC_11db);

  // Attach buzzer pin to PWM channel 0 with standard frequency (ESP32 3.x core syntax)
  ledcAttach(BUZZER, 5000, 8);

  // Seed the random generator using noise from an unconnected analog pin
  randomSeed(analogRead(32));

  calibrateJoystick();
  introScreen();
  resetGame();
}

void loop() {
  readJoystick();

  // Non-blocking timer to control the game ticks/ticks speed
  if (millis() - lastMove > gameSpeed) {
    lastMove = millis();
    moveSnake();
  }

  // Clear, redraw, and send the frame buffer to the OLED display
  u8g2.clearBuffer();
  drawSnake();
  drawFood();
  drawHUD();
  u8g2.sendBuffer();

  // Trigger game over routine if collision occurred
  if (gameOver) {
    gameOverScreen();
  }

  // Restart game instantly if the joystick button is pressed
  if (!digitalRead(JOY_SW)) {
    resetGame();
    delay(250); // Simple debounce delay
  }
}
