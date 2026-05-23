#include <DHTesp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin Configuration
const int SOIL_PIN   = 13;   // D13 - potentiometer signal
const int LIGHT_PIN  = 5;    // D5  - photoresistor DO
const int RELAY_PIN  = 14;   // D14 - relay input
const int LED_PIN    = 18;   // D18 - status LED
const int BUZZER_PIN = 19;   // D19 - buzzer
const int DHT_PIN    = 15;   // D15 - DHT22 data

// LCD I2C: SDA = 21, SCL = 22
LiquidCrystal_I2C lcd(0x27, 20, 4);
DHTesp dhtSensor;

enum State {
  MONITORING,
  NEED_WATER_CHECK,
  WATERING,
  WAIT_AFTER_WATER,
  NIGHT_LOCKOUT,
  ERROR_STATE
};

State currentState = MONITORING;

// Thresholds / Config
const int SOIL_DRY_THRESHOLD = 3000;     // Higher = drier
const int SOIL_WET_THRESHOLD = 2200;     // Lower = wetter enough
const float MAX_SAFE_TEMP = 40.0;
const float MIN_SAFE_TEMP = 5.0;

const unsigned long WATERING_TIME_MS = 5000;
const unsigned long POST_WATER_WAIT_MS = 8000;
const unsigned long SENSOR_CHECK_INTERVAL = 1000;

// Runtime Variables
unsigned long stateStartTime = 0;
unsigned long lastLoopTime = 0;
unsigned long lastLcdUpdate = 0;

int soilValue = 0;
int lightValue = 0; // digital now: HIGH/LOW
float temperatureC = 0.0;
float humidity = 0.0;

bool sensorFault = false;

// Helper Functions
const char* stateToString(State s) {
  switch (s) {
    case MONITORING:       return "MONITORING";
    case NEED_WATER_CHECK: return "CHECK_WATER";
    case WATERING:         return "WATERING";
    case WAIT_AFTER_WATER: return "WAITING";
    case NIGHT_LOCKOUT:    return "NIGHT_LOCK";
    case ERROR_STATE:      return "ERROR";
    default:               return "UNKNOWN";
  }
}

void setState(State newState) {
  if (currentState == newState) return;

  currentState = newState;
  stateStartTime = millis();

  Serial.print("\n>>> STATE CHANGED TO: ");
  Serial.println(stateToString(currentState));
}

void readSensors() {
  soilValue = analogRead(SOIL_PIN);
  lightValue = digitalRead(LIGHT_PIN);

  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  temperatureC = data.temperature;
  humidity = data.humidity;

  sensorFault = false;

  if (soilValue < 0 || soilValue > 4095) {
    sensorFault = true;
  }

  //if state isnt low or high sensor has fault
  if (!(lightValue == LOW || lightValue == HIGH)) {
    sensorFault = true;
  }

  if (isnan(temperatureC) || isnan(humidity)) {
    sensorFault = true;
  }

  if (temperatureC < -10 || temperatureC > 80 || humidity < 0 || humidity > 100) {
    sensorFault = true;
  }
}

// If the light logic is backwards in your Wokwi project,
// change HIGH to LOW here.
bool isDaytime() {
  return lightValue == LOW;
}

bool isSoilDry() {
  return soilValue >= SOIL_DRY_THRESHOLD;
}

bool isSoilWetEnough() {
  return soilValue <= SOIL_WET_THRESHOLD;
}

bool isTemperatureSafe() {
  return (temperatureC >= MIN_SAFE_TEMP && temperatureC <= MAX_SAFE_TEMP);
}

int soilPercent() {
  // 0 = very wet, 100 = very dry
  return map(soilValue, 0, 4095, 0, 100);
}

void pumpOn() {
  digitalWrite(RELAY_PIN, HIGH);
}

void pumpOff() {
  digitalWrite(RELAY_PIN, LOW);
}

void ledOn() {
  digitalWrite(LED_PIN, HIGH);
}

void ledOff() {
  digitalWrite(LED_PIN, LOW);
}

void beepShort() {
  tone(BUZZER_PIN, 1000, 150);
}

void beepError() {
  tone(BUZZER_PIN, 300, 300);
}

void printSensorData() {
  Serial.println("----- Sensor Readings -----");

  Serial.printf("Soil raw: %d\n", soilValue);
  Serial.printf("Soild %% dry: %d\n", soilPercent());
  Serial.printf("Light DO: %d\n", lightValue);
  Serial.printf("Temperature: %.2f C\n", temperatureC);
  Serial.printf("Humidity: %.2f %%\n", humidity);
  Serial.printf("Daytime? %s\n", isDaytime() ? "YES" : "NO");
  Serial.printf("Soil Dry? %s\n", isSoilDry() ? "YES" : "NO");
  Serial.printf("Temp Safe? %s\n", isTemperatureSafe() ? "YES" : "NO");
  Serial.printf("Sensor Fault? %s\n", sensorFault ? "YES" : "NO");
  Serial.printf("State: %s\n", stateToString(currentState));

  Serial.println("---------------------------");
}

void updateLCD() {
  if (millis() - lastLcdUpdate < 500) return;
  lastLcdUpdate = millis();

  lcd.setCursor(0, 0);
  lcd.print("Soil:");
  lcd.print(soilValue);
  lcd.print(" ");
  lcd.print(soilPercent());
  lcd.print("%     ");

  lcd.setCursor(0, 1);
  lcd.print("Light:");
  lcd.print(isDaytime() ? "DAY  " : "NIGHT");
  lcd.print(" Temp:");
  lcd.print(temperatureC, 1);
  lcd.print("C   ");

  lcd.setCursor(0, 2);
  lcd.print("Hum:");
  lcd.print(humidity, 0);
  lcd.print("% ");
  lcd.print("Pump:");
  lcd.print(digitalRead(RELAY_PIN) ? "ON " : "OFF");
  lcd.print("   ");

  lcd.setCursor(0, 3);
  lcd.print("State:");
  lcd.print(stateToString(currentState));
  lcd.print("        ");
}

void setup() {
  Serial.begin(115200);

  pinMode(SOIL_PIN, INPUT);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Plant System");
  lcd.setCursor(0, 1);
  lcd.print("Booting...");
  delay(1200);
  lcd.clear();

  Serial.println("Smart Plant System Starting...");
  setState(MONITORING);
}

void loop() {
  if (millis() - lastLoopTime < SENSOR_CHECK_INTERVAL) {
    updateLCD();
    return;
  }
  lastLoopTime = millis();

  readSensors();
  printSensorData();

  if (sensorFault) {
    setState(ERROR_STATE);
  }

  switch (currentState) {
    case MONITORING:
      pumpOff();
      ledOff();

      if (sensorFault) {
        setState(ERROR_STATE);
      } else if (!isDaytime()) {
        setState(NIGHT_LOCKOUT);
      } else if (isSoilDry()) {
        setState(NEED_WATER_CHECK);
      }
      break;

    case NEED_WATER_CHECK:
      pumpOff();
      ledOn();

      if (sensorFault) {
        setState(ERROR_STATE);
      } else if (!isDaytime()) {
        setState(NIGHT_LOCKOUT);
      } else if (!isSoilDry()) {
        setState(MONITORING);
      } else if (isSoilDry() && isDaytime() && isTemperatureSafe()) {
        beepShort();
        setState(WATERING);
      } else {
        setState(ERROR_STATE);
      }
      break;

    case WATERING:
      ledOn();
      pumpOn();

      if (sensorFault) {
        pumpOff();
        setState(ERROR_STATE);
      } else if (!isDaytime()) {
        pumpOff();
        setState(NIGHT_LOCKOUT);
      } else if (isSoilWetEnough()) {
        pumpOff();
        setState(WAIT_AFTER_WATER);
      } else if (millis() - stateStartTime >= WATERING_TIME_MS) {
        pumpOff();
        setState(WAIT_AFTER_WATER);
      }
      break;

    case WAIT_AFTER_WATER:
      pumpOff();
      ledOff();

      if (sensorFault) {
        setState(ERROR_STATE);
      } else if (millis() - stateStartTime >= POST_WATER_WAIT_MS) {
        if (!isDaytime()) {
          setState(NIGHT_LOCKOUT);
        } else {
          setState(MONITORING);
        }
      }
      break;

    case NIGHT_LOCKOUT:
      pumpOff();
      ledOff();

      if (sensorFault) {
        setState(ERROR_STATE);
      } else if (isDaytime()) {
        setState(MONITORING);
      }
      break;

    case ERROR_STATE:
      pumpOff();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));

      if ((millis() / 2000) % 2 == 0) {
        beepError();
      }

      if (!sensorFault && isTemperatureSafe()) {
        if (isDaytime()) {
          setState(MONITORING);
        } else {
          setState(NIGHT_LOCKOUT);
        }
      }
      break;
  }

  updateLCD();
}