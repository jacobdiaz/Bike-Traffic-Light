#include <Adafruit_NeoPixel.h>
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "click_sound.h"         // click sound, as a data array

// ---- LED + button pins ----
#define LED_PIN     5
#define BUTTON_PIN  4
#define NUM_LEDS    60
#define MAX_BRIGHTNESS 150       // lower to 50 if the strip flickers / board resets

// ---- I2S amp pins (MAX98357A) ----
#define I2S_BCLK    26           // D26 -> BCLK
#define I2S_LRC     22           // D22 -> LRC
#define I2S_DOUT    25           // D25 -> DIN

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

AudioOutputI2S *audioOut = nullptr;   // created once, reused
AudioFileSourcePROGMEM *clickFile = nullptr;
AudioGeneratorWAV *clickWav = nullptr;

// ---- State ----
enum LightMode { REGULAR, DISCO, TRAFFIC };
LightMode mode = REGULAR;
bool lightOn = false;
bool lastRawButton = HIGH;
bool stableButton = HIGH;
bool sequenceStartedOn = false;
byte clickCount = 0;
unsigned long lastDebounceTime = 0;
unsigned long firstClickTime = 0;
unsigned long lastClickTime = 0;
unsigned long modeStartTime = 0;
unsigned long lastAnimationTime = 0;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long CLICK_GAP_MS = 450;
const unsigned long TRIPLE_CLICK_MS = 900;

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(MAX_BRIGHTNESS);
  strip.clear();
  strip.show();                  // start off

  // Set up the I2S audio output once
  audioOut = new AudioOutputI2S();
  audioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioOut->SetGain(0.5);        // 0.0 - 1.0 volume. Raise/lower to taste.

  Serial.println("Ready. Press to toggle; triple press while on to change modes.");
}

void loop() {
  unsigned long now = millis();
  bool rawButton = digitalRead(BUTTON_PIN);
  if (rawButton != lastRawButton) {
    lastDebounceTime = now;
  }
  if (now - lastDebounceTime >= DEBOUNCE_MS && rawButton != stableButton) {
    stableButton = rawButton;
    if (stableButton == LOW) handlePress(now);
  }
  lastRawButton = rawButton;

  if (clickWav && clickWav->isRunning() && !clickWav->loop()) clickWav->stop();
  if (clickWav && !clickWav->isRunning()) stopClick();
  updateLights(now);
}

void handlePress(unsigned long now) {
  if (clickCount == 0 || now - lastClickTime > CLICK_GAP_MS ||
      now - firstClickTime > TRIPLE_CLICK_MS) {
    clickCount = 0;
    firstClickTime = now;
    sequenceStartedOn = lightOn;
  }
  lastClickTime = now;
  clickCount++;

  if (clickCount == 3 && sequenceStartedOn) {
    mode = static_cast<LightMode>((mode + 1) % 3);
    lightOn = true;
    modeStartTime = now;
    lastAnimationTime = now - 100;
    Serial.print("Mode: ");
    Serial.println(mode == REGULAR ? "regular" : mode == DISCO ? "disco" : "traffic");
    clickCount = 0;
  } else {
    lightOn = !lightOn;
    Serial.println(lightOn ? "ON" : "OFF");
    if (lightOn) {
      modeStartTime = now;
      lastAnimationTime = now - 100;
    } else {
      strip.clear();
      strip.show();             // off immediately, without a fade
    }
  }
  playClick();
}

// ---- Play the WAV clip without blocking button presses ----
void playClick() {
  stopClick();
  clickFile = new AudioFileSourcePROGMEM(click_wav, click_wav_len);
  clickWav = new AudioGeneratorWAV();
  clickWav->begin(clickFile, audioOut);
}

void stopClick() {
  if (clickWav) {
    if (clickWav->isRunning()) clickWav->stop();
    delete clickWav;
    clickWav = nullptr;
  }
  delete clickFile;
  clickFile = nullptr;
}

void updateLights(unsigned long now) {
  if (!lightOn) return;

  if (mode == REGULAR) {
    if (now - lastAnimationTime < 4 || now - modeStartTime > 1020) return;
    lastAnimationTime = now;
    setAllGreen(min(255UL, (now - modeStartTime) / 4));
  } else if (mode == DISCO) {
    if (now - lastAnimationTime < 100) return;
    lastAnimationTime = now;
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(random(65536))));
    }
    strip.show();
  } else {
    if (now - lastAnimationTime < 50) return;
    lastAnimationTime = now;
    unsigned long phase = (now - modeStartTime) % 24000;
    uint32_t color = phase < 10000 ? strip.Color(255, 0, 0) :
                     phase < 14000 ? strip.Color(255, 64, 0) :
                                    strip.Color(0, 255, 0);
    strip.fill(color);
    strip.show();
  }
}

void setAllGreen(int level) {
  int g = (level * MAX_BRIGHTNESS) / 255;
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, g, 0));
  }
  strip.show();
}
