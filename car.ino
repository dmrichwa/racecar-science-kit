#include <LiquidCrystal.h>
#include <avr/dtostrf.h>

// LCD //
/** Config **/
const int rs = 6, en = 5, d4 = 4, d5 = 3, d6 = 2, d7 = 1; // Pins for LCD screen
const int refreshRate = 250; // How many milliseconds between refreshing LCD screen
const int screenRate = 8; // How long (in units of refresh rate) between scrolling through the screens
const int screens = 2; // How many screens there are
/** Config **/

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int screen = 1; // current screen
int screenCounter = 0; // current screen counter

float c_time = 0;
float c_distance = 0;
float c_speed = 0;
const char time_str[] PROGMEM = " Time: %ss       ";
const char dist_str[] PROGMEM = " Dist: %sin       ";
const char speed_str[] PROGMEM = " Speed: %sin/s     ";
const char restart_str[] PROGMEM = "Green to restart";
const char empty_str[] PROGMEM = "                ";

const char inst_size_1[] PROGMEM = "  Press button  ";
const char inst_size_2[] PROGMEM = " for wheel size ";
const char inst_start_1[] PROGMEM = "  Press green   ";
const char inst_start_2[] PROGMEM = " button to start";
// LCD //

// Pins //
const int ENCODER = 13;  // encoder interrupt pin

// LED pins 
const int LED1 = A2;  // blue LED; corresponds to wheel 1
const int LED2 = A3;  // yellow LED
const int LED3 = A0;  // red LED; A0 for orange car, A4 for others (due to broken pin)
const int RESET_LED = A1;  // green LED

const int blink_time = 500;  // time in milliseconds of how long before toggling a blinking LED
unsigned long blink_timer = millis();  // last time LEDs blinked
volatile int blink_on = 0;  // whether to set blinking LEDs to on or off, 0 = off, 1 = on

const int push_time = 250;  // time in milliseconds of how long to illuminate LED when pushing (only used for starting countdown)
unsigned long push_timer = millis();
volatile int push_state = 0;  // 0 = not yet pushed, 1 = pushed and off, 2 = on, 3 = off and done

const int countdown_time = 1000;  // time in milliseconds of how long to illuminate LED before moving to the next (countdown)
unsigned long countdown_timer = millis();
volatile int countdown_led = 0;  // 0 = LED3, 1 = LED2, 2 = LED1, 3 = RESET_LED, 4 = DONE

// buttons
const int BUTTON_RESTART = 11;  // green button
const int BUTTON_WHEEL1 = 10;  // blue button
const int BUTTON_WHEEL2 = 9;  // yellow button
const int BUTTON_WHEEL3 = 8;  // red button
// Pins //

// Information //
enum prog_state {
  RESET,  // initial state, resets info on green button press, select wheel size
  WAITING, // wheel button pressed, waiting for green button to be pressed
  READY,  // green button pressed, car ready to start recording
  RECORDING,  // car moving, recording info
  STOPPED  // car stopped, display info on screen
};

prog_state state = RESET;

volatile float counter = 0;  // encoder counter
volatile float last_counter_update;  // time that the encoder was last updated (used to check when car stops moving)
const float counter_threshold = 500.0;  // time that the encoder has to not update for the car to be considered stopped
const float minimum_counter = 20.0;  // the minimum amount of counter ticks for the car to be considered stopped (prevents issues with launcher)

volatile float timer = 0.0;  // elapsed time of current run
volatile float start_time = 0.0;  // start time of current run

const float DISKSLOTS = 20.0;  // number of slots in encoder disk

// wheel sizes
// NB: Slightly smaller than actual sizes because they get squished from the weight of the car
const float WHEEL1 = 65.0;  // small wheels, blue
const float WHEEL2 = 85.0;  // medium wheels, yellow
const float WHEEL3 = 94.0;  // big wheels, red

volatile float currentWheel = 0;  // size of wheel on car
volatile float currentDistance = 0;  // current distance
volatile float avg_speed = 0;  // average speed
// Information //


// Encoder moved
void ISR_count() {
  if (state == RECORDING) {  // only increment if car is moving
    counter += 0.5;  // increment encoder counter; divide by 2 because interrupt is triggered twice as often
    last_counter_update = millis();  // update last time encoder was updated
  }
}

// Green button pushed
void ISR_BUTTON_RESTART() {
  if (state == WAITING) {  // ready to start recording
    state = READY;
    start_time = millis();
    push_timer = millis();
    push_state = 1;
    countdown_timer = millis(); // - countdown_time? lets countdown start immediately
    digitalWrite(RESET_LED, HIGH);  // turn the LEDs off, except the RESET LED to on for a brief moment
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
  }
  else if (state != READY) {  // reset car
    state = RESET;  // change state to reset
    counter = 0;  // reset measurements
    last_counter_update = 0;
    currentDistance = 0;
    timer = 0;
    start_time = 0;
    avg_speed = 0;
    countdown_timer = millis();
    countdown_led = 0;
    push_timer = millis();
    push_state = 0;
    blink_timer = millis();
    blink_on = 0;
    
    digitalWrite(RESET_LED, LOW); // turn the LEDs off
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
  }
}

// Blue, yellow, or red button pushed
void ISR_BUTTON_WHEEL() {
  if (state == RESET) {  // only allow changing wheel size when car first starts up
    state = WAITING;  // advance to waiting for green button state
    digitalWrite(RESET_LED, LOW); // turn off green LED
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
    if (digitalRead(BUTTON_WHEEL1) == 1) {  // wheel 1 selected
      currentWheel = WHEEL1;  // update current wheel size
      digitalWrite(LED1, HIGH);
    }
    else if (digitalRead(BUTTON_WHEEL2) == 1) {
      currentWheel = WHEEL2;
      digitalWrite(LED2, HIGH);
    }
    else if (digitalRead(BUTTON_WHEEL3) == 1) {
      currentWheel = WHEEL3;
      digitalWrite(LED3, HIGH);
    }
  }  
}  

// Calculate distance and speed
void distance (float currentWheelDiameter) {
  float circumference = currentWheelDiameter * 3.14159;
  currentDistance = (circumference * (counter / DISKSLOTS)) / (10 * 2.54);  // divide by 10*2.54 to convert from meters to inches
  avg_speed = currentDistance / timer;
  if (timer == 0.0) {  // divide by 0 errors
    avg_speed = 0.0;
  }
}

void setup() {
  pinMode(RESET_LED, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  digitalWrite(RESET_LED, LOW);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);

  attachInterrupt(digitalPinToInterrupt(ENCODER), ISR_count, RISING);  // increase encoder counter when speed sensor pin changes between low and high
  attachInterrupt(digitalPinToInterrupt(BUTTON_RESTART), ISR_BUTTON_RESTART, RISING);  // restart car when green button pushed
  attachInterrupt(digitalPinToInterrupt(BUTTON_WHEEL1), ISR_BUTTON_WHEEL, RISING);  // change wheel size when wheel size button pushed
  attachInterrupt(digitalPinToInterrupt(BUTTON_WHEEL2), ISR_BUTTON_WHEEL, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_WHEEL3), ISR_BUTTON_WHEEL, RISING);

  lcd.begin(16, 2);
}

void loop() {
  // blink LEDs
  if (millis() - blink_timer >= blink_time) {
    blink_timer = millis();
    blink_on ^= 1;
  }
  switch (state) {
    case RESET:
      digitalWrite(LED1, blink_on);  // sometimes the LEDs get stuck if you hit the wheel size button right as this triggers
      // fixed by forcing them off in the next state
      digitalWrite(LED2, blink_on);
      digitalWrite(LED3, blink_on);
      digitalWrite(RESET_LED, LOW);  // this LED should never be on in the reset state -- fixes race condition from resetting the button
      break;
    case WAITING:
      if (currentWheel == WHEEL1) {  // clear out the previous LED code due to race conditions
        digitalWrite(LED1, HIGH);
        digitalWrite(LED2, LOW);
        digitalWrite(LED3, LOW);
      }
      else if (currentWheel == WHEEL2) {
        digitalWrite(LED1, LOW);
        digitalWrite(LED2, HIGH);
        digitalWrite(LED3, LOW);
      }
      else if (currentWheel == WHEEL3) {
        digitalWrite(LED1, LOW);
        digitalWrite(LED2, LOW);
        digitalWrite(LED3, HIGH);
      }
      digitalWrite(RESET_LED, blink_on);
      break;
    case STOPPED:
      digitalWrite(RESET_LED, blink_on);
      break;
  }

  // push LEDs
  if (push_state == 1) {  // force the green LED on here to prevent race condition from case WAITING:
    push_state = 2;
    digitalWrite(RESET_LED, HIGH);
  }
  if (push_state == 2 && millis() - push_timer >= push_time) {
    push_state = 3;
    digitalWrite(RESET_LED, LOW);
  }

  // countdown LEDs
  if (state == READY && (countdown_led == 0 || millis() - countdown_timer >= countdown_time)) {
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
    countdown_timer = millis();
    if (countdown_led == 0) {
      digitalWrite(LED3, HIGH);
      countdown_led += 1;
    }
    else if (countdown_led == 1) {
      digitalWrite(LED2, HIGH);
      countdown_led += 1;
    }
    else if (countdown_led == 2) {
      digitalWrite(LED1, HIGH);
      countdown_led += 1;
    }
    else if (countdown_led == 3) {
      digitalWrite(RESET_LED, HIGH);
      countdown_led += 1;
    }
    else if (countdown_led == 4) {
      countdown_led += 1;
    }
  }
  
  if (state == READY && countdown_led >= 4) {  // ready to launch state
    start_time = millis();  // update start time
    state = RECORDING;  // move to car moving state
    screen = 1;  // reset screens to first
    screenCounter = 0;
    digitalWrite(RESET_LED, HIGH);
    digitalWrite(LED1, LOW);  // turn off wheel size LEDs
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
  }

  if (state == RECORDING) {  // car moving state
    if (counter == 0.0) {  // start the timer once the car has moved
      start_time = millis();
    }
    timer = (millis() - start_time) / 1000;  // update timer
    distance(currentWheel);  // update distance and speed
    
    if ((counter >= minimum_counter) && (millis() - last_counter_update >= counter_threshold)) {  // check if the car is moving and has moved from its initial position
      state = STOPPED;  // car stopped, reset state
      digitalWrite(RESET_LED, LOW);
    }
  }
  
  // update LCD
  updateInfo(timer, currentDistance, avg_speed);
  if (millis() % refreshRate == 0) {
    refreshLCD();
  }
}

// update internal LCD information
void updateInfo(float _time, float _distance, float _speed) {
  c_time = _time;
  c_distance = _distance;
  c_speed = _speed;
}

// refresh LCD screen
void refreshLCD() {
  // cycle screens
  screenCounter++;
  if (screenCounter >= screenRate) {
    screenCounter = 0;
    screen++;
    if (screen > screens) {
      screen = 1;
    }
  }

  // build output strings
  char str1[24] = "";
  char str2[24] = "";
  char fstr1[8];
  char fstr2[8];
  if (screen == 1) {
    dtostrf(c_time, 4, 1, fstr1); // convert float to string since sprintf does not support floats
    dtostrf(c_distance, 4, 1, fstr2);
    sprintf_P(str1, time_str, fstr1);
    sprintf_P(str2, dist_str, fstr2);
  }
  else {
    dtostrf(c_speed, 4, 1, fstr1);
    sprintf_P(str1, speed_str, fstr1);
    if (state == STOPPED) {  // only print restart instructions once the car has stopped
      sprintf_P(str2, restart_str, NULL);
    }
    else {
      sprintf_P(str2, empty_str, NULL);
    }
  }

  // redraw LCD
  lcd.setCursor(0, 0);
  if (state == RESET) {
    lcd.print(inst_size_1);
  }
  else if (state == WAITING) {
    lcd.print(inst_start_1);
  }
  else if (state == READY) {
    switch(countdown_led) {
      case 0:
        lcd.print(" 3...           ");  // should only be 0 if race condition between hitting green button and reaching this code
        break;
      case 1:
        lcd.print(" 3...           ");
        break;
      case 2:
        lcd.print(" 2..            ");
        break;
      case 3:
        lcd.print(" 1.             ");
        break;
      case 4:
        lcd.print(" GO!            ");
        break;
      default:
        lcd.print(str1);
    }
  }
  else {
    lcd.print(str1);
  }
  lcd.setCursor(0, 1);
  if (state == RESET) {
    lcd.print(inst_size_2);
  }
  else if (state == WAITING) {
    lcd.print(inst_start_2);
  }
  else if (state == READY && countdown_led <= 4) {
    lcd.print(empty_str);
  }
  else {
    lcd.print(str2);
  }
}
