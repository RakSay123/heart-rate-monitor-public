/* IMPORTANT INFORMATION!
  === Heart Rate Monitor & Oximeter System ===
  Developed for: Real-time heart rate (HR) and confidence (CFD) monitoring
  Hardware: SparkFun Pulse Oximeter and Heart Rate Monitor (MAX30101 with MAX32664 processor)
  Interface: I²C (SDA, SCL), plus MFIO and RESET lines

  === SYSTEM BEHAVIOR ===
  This program continuously polls sensor data, validates signal quality, handles sensor disconnection, and manages restarts via:
    - Time-based invalid data thresholds
    - Buffer-based detection of prolonged invalid frames
    - Display-freeze-aware logic to avoid false restarts

  === DISPLAY BEHAVIOR ===
  - Shows heart rate, confidence, and customizable status messages
  - Displays the last valid reading during brief signal drops
  - After sustained signal loss or invalid data, resets the system to reinitialize sensor via buffer-induced or timeout-induced

  === IMPORTANT USAGE NOTES ===
  - Maintain **light and consistent pressure** on the sensor. Too much force can compress capillaries, causing false zero or erratic values.
  - Keep the finger still during measurement. Motion artifacts reduce confidence and may trigger restarts.
  - Values shown on the LCD are last-known *valid* data for stability and UI clarity.
  - Raw live feed data is displayed on the serial monitor.
  - The buffer-induced restart is stricter than the timeout-induced restart, so, don't be alarmed if you notice the timeout-based restart isn't called as much

  === TROUBLESHOOTING ===
  - If sensor values freeze or read zeros:
    - Ensure finger placement is correct
    - Confirm stable power and clean connections
    - Check if `body.status < 2` - indicates finger not detected
  - If automatic restart occurs:
    - Timeout: No valid data within the set threshold (default 30 seconds)
    - Buffer: Set amount of consecutive invalid frames detected (default 15 invalid frames)

  === CUSTOMIZATION TIPS ===
  - `refreshRate` controls loop speed, change for faster/slower polling
  - `bufferSize`, `validFrameThreshold`, and `maxInvalidTime` can be tuned for sensitivity
  - Display logic separates raw sensor input from what is shown to the user

  === LAST UPDATED ===
  May 2025: Significantly upgraded from original version with buffer tracking, robust restart logic, freeze detection, and LCD data handling.
*/

// Imports
#include <SparkFun_Bio_Sensor_Hub_Library.h>
#include <Wire.h>
#include <LiquidCrystal.h>

// No other Address options
#define DEF_ADDR 0x55

// Reset pin, MFIO pin
const int resPin = 7; // Sensor Reset pin
const int mfioPin = 6; // Sensor MFIO pin

// LCD Pins and Initialization information
const int RS = 2;
const int E = 4;
const int D4 = 8;
const int D5 = 10;
const int D6 = 11;
const int D7 = 13;

const int lcdCols = 16;
const int lcdRows = 2;

// Component Initialization
// Takes address, reset pin, and MFIO pin.
SparkFun_Bio_Sensor_Hub bioHub(resPin, mfioPin); 
LiquidCrystal lcd(RS, E, D4, D5, D6, D7);
bioData body;
/* bioData body Definition: 
  This is a struct unique to the SparkFun Pulse Oximeter and Heart Rate Monitor.
  bioData gives us the heart rate, confidence in heart rate, oxygen level, and
  status of the sensed object, like if it is a finger or not. I will abbreviate
  heart rate, confidence in heart rate, and oxygen levels as HR, CFD, and O2 
  respectively
*/

// Constants for adjustable thresholds
const int MIN_HR = 30;
const int MAX_HR = 220;
const int MIN_CFD = 1;
const int MIN_O2 = 1;

// Frame buffer for recent bioData 
const int bufferSize = 20; // Quantity of max invalid frames before restart

struct MiniBio {
  uint16_t heartRate;
  uint8_t confidence;
  uint16_t oxygen;
  uint8_t status;
};

MiniBio buffer[bufferSize]; // Array of max continuous faulty data
int bufferIndex = 0; // Used to constantly update list of frames in buffer
int validFrameCount = 0;
const int validFrameThreshold = 3; // Number of valid frames before we trust buffer logic

// Buffer state flags
bool bufferInitialized = false; // Boolean flag lets us know if the buffer is initialized, and to prevent false restarts
bool bufferPrimed = false; // Boolean flag that lets us know if the buffer gets filled at least once to prevent premature restarts

// Time tracking for invalid frame timeouts
unsigned long lastProperReading = 0; // Time variable used to check max time between invalid frames and restart
const int maxInvalidTime = 30000; // Milliseconds, max time of invalid readings before sensor restarts

// Live connection flags
bool o2Valid = true;
bool isFingerDetected = false; // Detected finger on sensor
bool isNoFinger = true;    // Lets us know if there is a finger or not (MAY BE REPLACED WITH METHOD DEFINITIONS)
bool validFrame = false;  // Confidence check added (MAY BE REPLACED WITH METHOD DEFINITIONS)
bool allowUpdate = false;  // Variable that gives permission to update values
bool disconnected = false; // Ultimate variable to determine if sensor disconnects


// Last known valid data values for UI display
int lastHeartRate = 0;
int lastConfidence = 0;
int lastOxygen = 0;
int lastStatus = 0;

// Data/display freeze tracking
unsigned long lastDisplayUpdate = 0;
const int freezeTimeout = 3000; // 3 seconds
bool displayingFrozenData;

// Initial grace period logic
unsigned long lastConnectedMillis = 0;  // Used for preventing false data freezes at the beginning of the program
const unsigned long gracePeriod = 5000; // milliseconds, used for initial values so data doesn't freeze initially
bool hasBeenConnected = false;          // Also used to prevent data freezes at the first initialization

// Display refresh and values 
const int refreshRate = 1000; // milliseconds
byte heartChar[8] = {0b00000,  0b01010,  0b11111,  0b11111, 0b11111,  0b01110,  0b00100,  0b00000}; // Custom heart character

// Status message strings, used by getStatusMessages(), which is called in serialPrinting()
const char* statusMessages[4] = {"No Object Detected", "Object Detected", "Object Other Than Finger Detected", "Finger Detected"};


// ============================================================================================
// ====================================== Initialization ======================================
// ============================================================================================

void initializeLCD() {
  lcd.begin(lcdCols, lcdRows);
  lcd.createChar(3, heartChar);  // Save it in slot 3

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Configuring ");
  for (int i = 0; i < 3; i++) {
    delay((unsigned long)(refreshRate * 0.2));
    lcd.print(".");
  }
  delay((unsigned long)(refreshRate * 0.5));
}

// Configures the sensor, or puts the program into a safe state should the configuration fail
void initializeSensor() {
  int result = bioHub.begin();
  if (result == 0) {
    Serial.println("Sensor started!");
  }
  else {
    Serial.println("Could not communicate with the sensor!!!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    while (true);  // halt if no communication
  }

  Serial.println("Configuring Sensor...");
  int error = bioHub.configBpm(MODE_ONE);
  if (error == 0) {
    Serial.println("Sensor configured.");
  }
  else {
    Serial.println("Error configuring sensor.");
    Serial.print("Error: ");
    Serial.println(error);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.print(error);

    if (error == 255) {
      Serial.println("Faulty setup. Program will not do anything.");
      lcd.setCursor(0, 1);
      lcd.print("Faulty setup");
      while (true);  // freeze program
    }
  }

  Serial.println();
}

// Fills all values in the buffer with the dummy value "-1" so there isn't initially a loop of endless restarts
void initializeBuffer(int dummyVal) {
  for (int i = 0; i < bufferSize; i++) {
    buffer[i] = {dummyVal, dummyVal, dummyVal, dummyVal};
  }
  bufferInitialized = true;
}

// Displays "Configured!" text as well as playing small loading animation 
void finalizeInitialization() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Configured!"); // Will not reach this line if there is an error in the configuration
  
  loadingHearts(0, 1, 6, 0.3, 1.2); 
  lcd.clear();
}

// Prints instructions to the LCD and serial monitor 
void printInstructions() {
  // Screen 0:
  delay((unsigned long)(refreshRate * 2));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("How to use:");
  loadingHearts(0, 1, 6, 0.3, 1.2);
  lcd.clear();

  // Screens
  instructionScreen("1. Place finger", "on the sensor", 4);
  instructionScreen("2. View readings", "on LCD & serial", 4);
  instructionScreen("Have fun & enjoy", "- Rakhshan Sayed", 4);  
  instructionScreen("Have fun & enjoy", "- Rakhshan Sayed", 4);
  finalHeartScreen(6, 0.5, 2);

}

// Helper method that helps handling and displaying an instruction screen
void instructionScreen(const char* str1, const char* str2, float refreshRateMultiplier) {
  lcd.setCursor(0, 0);
  lcd.print(str1);
  lcd.setCursor(0, 1);
  lcd.print(str2);
  delay((unsigned long)(refreshRate * refreshRateMultiplier));
  lcd.clear();
}

// Helper method that plays custom heart loading animation
void loadingHearts(int row, int col, int numHearts, float refreshRateMultiplier, float finalRefreshRateMultiplier) {
  lcd.setCursor(row, col);
  for (int i = 0; i < numHearts; i++) {
    delay((unsigned long)(refreshRate * refreshRateMultiplier));
    lcd.write(3);
    lcd.print("  ");
  }
  delay((unsigned long)(refreshRate * finalRefreshRateMultiplier));
}

// Screen that displays 12 hearts on the screen with 2 rows loading at the same time
void finalHeartScreen(int numHearts, float refreshRateMultiplier, float finalRefreshRateMultiplier) {
  lcd.setCursor(0, 0);
    for (int i = 0; i < numHearts; i++) {
      delay((unsigned long)(refreshRate * refreshRateMultiplier));
      lcd.write(3);
      lcd.print("  ");
      lcd.setCursor(3 * i, 1);
      lcd.write(3);
      lcd.print("  ");
      lcd.setCursor(3 * i, 0);
    }
  lcd.setCursor(15, 0);
  lcd.write(3);
  delay((unsigned long)(refreshRate * finalRefreshRateMultiplier));
  lcd.clear();
}

// ============================================================================================
// =================================== Core Monitoring Logic ==================================
// ============================================================================================

// Determines if the sensor has been disconnected, communicates with other methods to ensure last valid readings stay printed
void determineDisconnected() {
  isFingerDetected = (body.status == 3) || (body.status == 2); // Detected finger on sensor
  isNoFinger = !isFingerDetected; // Determines the sensors guaranteed perception if a finger is on it or not (could mean anything else too)
  validFrame = isValidFrame();
  o2Valid = (body.oxygen > 0);  // Determines if the oxygen is valid (which sometimes is not), and if it is, and HR and CFD plummet to zero, but oxygen remains consistent, than skip updating
  allowUpdate = false; // Permission boolean for updating values

  // Decide whether to allow update
  if (validFrame || isNoFinger) {
    allowUpdate = true;
  }
  else if (isFingerDetected && o2Valid) {
    // Known glitch pattern - ignore frame
    Serial.println("Glitched frame ignored: zero HR/CFD while finger + valid O2.");
    allowUpdate = false;
  }
  else if (isFingerDetected && (!o2Valid && (body.heartRate != 0 && body.confidence != 0))) {
    allowUpdate = true; // Handles the edge-case that if oxygen is zero, and heart rate and confidence are nonzero, update values anyways
  }

  // Perform update if valid
  if (allowUpdate) {
    disconnected = false;
    hasBeenConnected = true;
    lastConnectedMillis = millis();

    lastHeartRate = body.heartRate;
    lastConfidence = body.confidence;
    lastOxygen = body.oxygen;
    lastStatus = body.status;

    lastDisplayUpdate = millis();
  }
  else {
    // If grace period exceeded, mark as disconnected
    if (!hasBeenConnected || millis() - lastConnectedMillis > gracePeriod) {
      disconnected = true;
      Serial.println("Sensor likely disconnected (all zero data).");
    }
  }
}

// Checks if conditions for reinitialization, either too much time with consecutive faulty readings, or too many consecutive faulty readings, and restarts program if necessary
void validateSensorState(int maxInvalidTime) {
  isFingerDetected = fingerOnSensor(); // meaning is body.status == 2 or body.status == 3
  validFrame = isValidFrame(); // finger is on sensor (status > 1) and HR, CFD, and O2 are all nonzero OR a finger is not on the sensor and everything else is zero
  bool displayingProperFrozenData = (disconnected) && (millis() - lastDisplayUpdate > freezeTimeout) && (lastHeartRate != 0 || lastConfidence != 0);

  timeCheck(maxInvalidTime, displayingProperFrozenData);
  bufferCheck(displayingProperFrozenData);
}

// Helper method called in validateSensorState() that checks if it's been maxInvalidTime / 1000 seconds since a proper reading was given
void timeCheck(int maxInvalidTime, bool displayingProperFrozenData) {

  if (!isFingerDetected || validFrame || displayingProperFrozenData) {
    lastProperReading = millis();
  }

  if ((millis() - lastProperReading) > maxInvalidTime) {
    if (!displayingProperFrozenData) {
      String serialReason = "Time based restart: " + String(maxInvalidTime/1000) + " seconds of invalid readings while finger detected.";
      String lcdReason = "No good data " + String(maxInvalidTime/1000) + "s";
      restartSystem(serialReason, lcdReason);
    }
    else { // This code segment runs if there is properly frozen data displayed for more than maxInvalidTime / 1000 seconds, then resets timer again
      Serial.println("Time-induced restart skipped: Valid frozen data displayed.");
      lastProperReading = millis();
    }
  }
}

// Helper method called in validateSensorState() that determines if we get 15 consecutive invalid readings, and if so, we restart the program
void bufferCheck(bool displayingProperFrozenData) {
  if (!isFingerDetected) {
    return;
  }

  buffer[bufferIndex] = {body.heartRate, body.confidence, body.oxygen, body.status};
  int currentIndex = bufferIndex;
  bufferIndex = (bufferIndex + 1) % bufferSize;

  bufferLogging(currentIndex);

  if (!bufferPrimed && bufferIndex == 0) {
    bufferPrimed = true;
  }

  // Count valid frames (for recovery logic)
  if (isValidFrame(buffer[currentIndex])) {
    validFrameCount++;
    if (validFrameCount >= validFrameThreshold) {
      Serial.println("Valid data returned - canceling buffer-based restart.");
      resetBuffer('V'); // V = recovered by valid frame
    }
  }

  // If buffer is fully primed, check for restart condition
  if (bufferPrimed) {
    bool allInvalid = true;
    for (int i = 0; i < bufferSize; i++) {
      if (isValidFrame(buffer[i])) {
        allInvalid = false;
        break;
      }
    }

    if (allInvalid && !displayingProperFrozenData) { // Second bool says to Skip restart if data is frozen but still shows valid last-known readings
      String serialReason = "Buffer handling deems restart: " + String(bufferSize) + " consecutive invalid frames.";
      String lcdReason = "x" + String(bufferSize) + " bad frames";
      bufferDebugDump();
      restartSystem(serialReason, lcdReason);
    }
  }

}

// Prints data to the LCD, which users will see and interact with
void lcdPrinting() {
  // The two ternary operators are to ensure if the finger is taken off, HR and CFD are set to zero without messing anything else up
  int hrToDisplay = (body.status < 2) ? 0 : lastHeartRate;
  int cfdToDisplay = (body.status < 2) ? 0 : lastConfidence;
  
  lcd.setCursor(0, 0);
  
  lcd.print("HR:");
  lcd.print(padInt(hrToDisplay, 3)); // 2-3 digit number
  lcd.print("  ");
  
  lcd.print("CFD:");
  lcd.print(padInt(cfdToDisplay, 3)); // 2-3 digit number
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print(getStatusMessage(lastStatus));
  lcd.print("  ");

  lcd.setCursor(14, 1);
  if (lastStatus == 3) {
    lcd.write(3);
  }
  else {
    lcd.print(" "); // Clears the heart emoji to prevent repeats
  }

  lcd.setCursor(15, 1);
  if (disconnected) {
    lcd.print("*"); // Indicates stale data due to sensor disconnect
  }
  else {
    lcd.print(" "); // Clears the asterisk to prevent reprints
  }
}

// Prints more information to the serial monitor, useful for debugging
void serialPrinting() {
  Serial.print("Heart rate: ");
  Serial.print(body.heartRate);
  Serial.println(" beats per minute");

  Serial.print("Confidence: ");
  Serial.print(body.confidence);
  Serial.println("%");

  Serial.print("Oxygen Levels: ");
  Serial.print(body.oxygen);
  Serial.println("");

  Serial.print("Status: ");
  Serial.print(statusMessages[body.status]);
  Serial.print(" [");
  Serial.print(body.status);
  Serial.println("]");

  Serial.println();
}


// ============================================================================================
// ===================================== Supporting Logic =====================================
// ============================================================================================

// Helper method that quickly determines if we have a figner on the sensor
bool fingerOnSensor() {
  return (body.status > 1);
}

// Helper method that defines what a valid frame is
/* Definition of valid frame means finger is on sensor (status > 1) and HR, CFD, and O2 are all nonzero    *
 * OR a finger is not on the sensor and everything else is zero                                            */
bool isValidFrame() {
  if (body.heartRate == 65535 || body.confidence == 255) { // For the current frame
    return false;
  }
  return ((fingerOnSensor() && (body.heartRate > 0 && body.confidence > 0 && body.oxygen > 0)) || (!fingerOnSensor() && (body.heartRate == 0 && body.confidence == 0 && body.oxygen == 0)));
}
bool isValidFrame(MiniBio frame) { // For any given frame
  // Absolute invalid/corrupt values
  if (frame.heartRate == 65535 || frame.confidence == 255 || frame.oxygen == 255) {
    return false;
  }

  // Finger detected: accept partially valid data
  bool hrValid  = frame.heartRate >= MIN_HR && frame.heartRate <= MAX_HR;
  bool cfdValid = frame.confidence >= MIN_CFD;
  bool o2Valid  = frame.oxygen >= MIN_O2 && frame.oxygen <= 100;

  int validCount = 0;
  if (hrValid) { 
    validCount++;
  }

  if (cfdValid) { 
    validCount++;
  }

  if (o2Valid) {
    validCount++;
  }

  return (validCount >= 2); // Require 2 out of 3 to pass
}

// Helper method that restarts the program
void restartSystem(String serialReason, String lcdReason) {
  Serial.println(serialReason);
  Serial.println("Restarting now...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reinitializing");
  lcd.setCursor(0, 1);
  lcd.print(lcdReason);
  delay((unsigned long)(refreshRate * 4));
  asm volatile ("  jmp 0");
}

// Resets the buffer
void resetBuffer(int dummyVal) {
  bufferPrimed = false;
  bufferIndex = 0;
  for (int i = 0; i < bufferSize; i++) {
    buffer[i] = {dummyVal, dummyVal, dummyVal, dummyVal};
  }
}

// Prints out a log of buffer entries being sent
// Prints the following on one line: "Buffer entry [num]: HR=[HR], CFD=[CFD], O2=[O2], STS=[STS]"
void bufferLogging(int i) {

  Serial.print("Buffer entry ");
  Serial.print(i);

  Serial.print(": HR=");
  Serial.print(buffer[i].heartRate);

  Serial.print(", CFD=");
  Serial.print(buffer[i].confidence);

  Serial.print(", O2=");
  Serial.print(buffer[i].oxygen);

  Serial.print(", STS=");
  Serial.println(buffer[i].status);
}

// Helper method that prints out all buffer logs
void bufferDebugDump() {
  Serial.println("Dumping buffer before restart:");
  for (int j = 0; j < bufferSize; j++) {
    bufferLogging(j);
  }
}


// ============================================================================================
// ===================================== Utility Methods ======================================
// ============================================================================================

// Helper method pads integer to 3 digits with leading spaces (e.g., "75" to " 75" or "5" to "  5" (note the added spaces for formatting))
String padInt(int val, int width) {
  String s = String(val);
  while (s.length() < width) {
    s = " " + s;
  }
  return s;
}

// Helper method to quickly traanslate status codes into status messages to be printed to LCD
String getStatusMessage(int status) {
  switch (status) {
    case 0: return "No Finger";
    case 1: return "Searching";
    case 2: return "Reading...";
    case 3: return "Finger On";
    default: return "Unknown";
  }
}


// ============================================================================================
// ===================================== Arduino Methods ======================================
// ============================================================================================

// Gets everything ready to run
void setup() {
  Serial.begin(115200);
  Wire.begin();

  initializeLCD();
  initializeSensor();
  initializeBuffer('*'); // Dummy value as a char, which is still an int
  lastProperReading = millis();
  delay((unsigned long)(refreshRate * 5));
  finalizeInitialization();

  printInstructions();
}

// Runs all of our defined methods in a continuous loop
void loop() {  
  body = bioHub.readBpm();                // Information from the readBpm function will be saved to "body" variable.

  determineDisconnected();                // Determine if the sensor is disconnected
  serialPrinting();                       // Serial debugging prints:
  lcdPrinting();                          // LCD User interface data printing
  validateSensorState(maxInvalidTime);    // Checks buffer for any errors, and handles improper data timeouts by restarting the program
  delay((unsigned long)(refreshRate));    // Slow it down to ease flow of numbers
}
