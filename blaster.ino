#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>

struct color {
  int r;
  int g;
  int b;

  color(int _r, int _g, int _b) {
    r = _r;
    g = _g;
    b = _b;
  }

  void subtract(int amount) {
    r = min(0, r - amount);
    g = min(0, g - amount);
    b = min(0, b - amount);
  }
};

// Use ACTIVATED = LOW for pullup, ACTIVATED = HIGH for pulldown
#define ACTIVATED LOW

// Set which pins these devices are on
#define DFPLAYER_RX 10
#define DFPLAYER_TX 11
#define BUTTON_PIN 3
#define LED_R A4
#define LED_G A3
#define LED_B A5

/* SETUP SOUNDS
    * state how many sounds are in each profile
        * first sound in each profile will be played when switching to that profile; this could be someone saying the profile name 'DC-17' or just the blast sound that profile makes, either works
        * all other sounds should be blast sounds for that profile, these will be rotated between so the gun doesn't sound exactly the same every single shot
    * state how many profiles there are
    * each profile must have the same number of sounds (eg if you only have 1 sound for a profile, duplicate it 5 times to match the other profiles)
*/
#define SOUNDS_PER_PROFILE 5
#define NUM_SOUND_PROFILES 4

#define LONG_PRESS_TIME 1500

// Set volume (0-30)
#define VOLUME 20



SoftwareSerial softSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;

// When the button was pressed (-1 if button is up)
long buttonActiveTime = -1;
// A bool triggered when a press goes from long to short
bool inLongPress = false;

// Which sound profile we are on (Can change between these to make gun sound/look different)
int currSoundProfile = 0;
// Which sound in that profile we are on (rotate through sounds in each profile so we aren't just playing the same sound over and over again)
int currSoundPos = 1;

color colors[4] = {
  color(0, 0, 255),     // DC-17
  color(255, 0, 0),     // E-5
  color(255, 0, 0),     // DL-44
  color(55, 55, 255)  // Stun
};

int blastTimes[4] = {
  100,  // DC-17
  100,  // E-5
  100,  // DL-44
  250   // Stun
};

int delayTimes[4] = {
  400,  // DC-17
  400,  // E-5
  400,  // DL-44
  750   // Stun
};


void setup() {
  Serial.begin(115200);
  setupDFPlayer();
  setupIO();
}

void setupDFPlayer() {
  softSerial.begin(9600);

  Serial.println();
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!dfPlayer.begin(softSerial, /*isACK = */ true, /*doReset = */ false)) {
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true) {
      delay(0);  // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));

  dfPlayer.volume(VOLUME);
  dfPlayer.play(1);
}

void setupIO() {
  pinMode(BUTTON_PIN, OUTPUT);
  digitalWrite(BUTTON_PIN, HIGH);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
}


void loop() {
  // Get the current time (how long the program has been running)
  long currentTime = millis();

  // User has held down button longer than max time
  if (buttonActiveTime != -1 && currentTime - buttonActiveTime >= LONG_PRESS_TIME && !inLongPress) {
    changeProfile();
    inLongPress = true;
    // Button stays pressed until user lets go (so they don't accidentally switch through multiple profiles)
    return;
  }

  // When user releases button (button isn't down and was down last frame)
  if (digitalRead(BUTTON_PIN) != ACTIVATED && buttonActiveTime != -1) {
    // Button is now unpressed
    buttonActiveTime = -1;

    // If a long press was already registered, remove it
    if (inLongPress) {
      inLongPress = false;
    }
    // Otherwise register a short press (aka fire the blaster)
    else {
      // Previous check ensures that if we are in this case, button has been held for short time
      fire();
    }
  }
  // When user presses button (button is down and wasn't down last frame)
  else if (digitalRead(BUTTON_PIN) == ACTIVATED && buttonActiveTime == -1) {
    // Store the time button went down
    buttonActiveTime = currentTime;
  }
}

// Switch between sound profiles
void changeProfile() {
  // If on the last sound profile, reset to first sound profile
  if (currSoundProfile == NUM_SOUND_PROFILES - 1) {
    currSoundProfile = 0;
  }
  // Otherwise increase the sound profile by 1
  else {
    currSoundProfile++;
  }
  // Set the currSoundPos to 1 (to reset to first blast sound, the first sound is a sound that says what the profile is)
  currSoundPos = 1;

  Serial.print("Changed to ");
  Serial.println(currSoundProfile);

  // Play first sound from profile (a sound that says what profile it is)
  dfPlayer.play(currSoundProfile * SOUNDS_PER_PROFILE);
}

// Play fire sound and light effects
void fire() {
  fireSound();
  fireLight();
}

void fireSound() {
  Serial.print("Playing ");
  Serial.println(currSoundProfile * SOUNDS_PER_PROFILE + currSoundPos);

  // Calculate which sound to play
  // EXAMPLE: if there are 5 sounds per profile and we are on profile 2, we have to skip 10 sounds (5 for profile 0 AND 5 for profile 1). that's currSoundProfile * SOUNDS_PER_PROFILE
  // THEN we also have to skip however many sounds based on our sound pos (so we rotate through our 5 sounds)
  // AND we add 1 because the first of the 5 sounds is actually the profile name sound (eg 'DC-17') so we don't want to play that when we're trying to blast
  dfPlayer.play(currSoundProfile * SOUNDS_PER_PROFILE + currSoundPos);

  // If on the last sound in the profile, reset to first blast sound in the profile (first blast sound = second sound because first sound is profile name)
  if (currSoundPos == SOUNDS_PER_PROFILE - 1) {
    currSoundPos = 1;
  }
  // Otherwise increase the sound pos by 1
  else {
    currSoundPos++;
  }
}

void fireLight() {
  color currentColor = colors[currSoundProfile];

  Serial.print("LED set to color(");
  Serial.print(currentColor.r);
  Serial.print(", ");
  Serial.print(currentColor.g);
  Serial.print(", ");
  Serial.print(currentColor.b);
  Serial.println(")");

  // Flash LED
  setLED(currentColor);
  delay(blastTimes[currSoundProfile]);
  setLED(color(0, 0, 0));
  // Delay before they can fire again
  delay(delayTimes[currSoundProfile]);
}

void setLED(color newColor) {
  analogWrite(LED_R, newColor.r);
  analogWrite(LED_G, newColor.g);
  analogWrite(LED_B, newColor.b);
}
