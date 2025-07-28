# Heart Rate Monitor with LiquidCrystal Display

### Description
This project is an Arduino-based heart rate monitor that uses the [**SparkFun Pulse Oximeter and Heart Rate Monitor**](https://learn.sparkfun.com/tutorials/sparkfun-pulse-oximeter-and-heart-rate-monitor-hookup-guide), which integrates the **MAX30101** and the **MAX32664**. It continuously displays real-time heart rate and signal confidence data on a **16x2 LiquidCrystal Display (LCD)**. The system also automatically manages faulty data and connection stability through filtering and re-initialization logic.

---

### Quick Start
1. Connect all components as described in the wiring guide.
2. Open the Arduino IDE and install the SparkFun Pulse Oximeter and Heart Rate Monitor library via Library Manager.
3. Open the provided file `HeartRateSensor.ino` and upload it to your board.
4. Open the Serial Monitor at `115200` baud to view debug output.
5. Observe your real-time heart rate displayed on the LCD

---

### Usage instructions:

![Annotated SparkFun Pulse Oximeter and Heart Rate Monitor](https://github.com/mabembedded/heart-rate-monitor/blob/main/SparkFun_Pulse_Oximeter_and_Heart_Rate_Monitor_Annotated.png "SparkFun Pulse Oximeter and Heart Rate Monitor Annotated")

For most optimal usage, *gently* place your finger over the sensor (as indicated by the diagram). Don't apply too much pressure. Remain still and **ensure good contact**. Wait for a few seconds, for the signal to be properly validated, and soon enough, you should see your heart rate and the confidence the sensor has displayed on the LCD.

The monitor uses an intelligent buffer and watchdog logic to discard unreliable readings, detect disconnection of the finger, and automatically reset itself if consistent bad data is detected. For users and developers who would like to see a live debugging log, **open** the serial monitor and set the baud rate to **115200 baud**.

---

### Features
- **Automatic Sensor Recovery**
   - **Frame-Based Restart**: Monitors the last 15 HR, CFD, and O2 readings, and if there are 15 consecutive invalid frames, the program restarts to reinitialize the sensor
   - **Time-Based Restart**: If 30 seconds pass, and there are only invalid readings, the program will again trigger a restart
- **Smart Data Handling**
   - **Data Freeze Protection**: If sensor data is lost or malformed (such as returning all zeroes), last valid readings are displayed to LCD UI
   - An asterisk (*) is displayed on the LCD if there is active data freezing to let users know if there is a data freeze
   - On the developer's serial monitor, real-time readings are shown, so, the developer can see exactly when a data freeze takes place, and observe what conditions allow it to take place
- **User-Friendly Interface**
   - **LiquidCrystal Display**: Clear and readable output for heart rate, confidence in that heart rate, and status, using a 16x2 LCD screen
   - **Serial Monitor Feedback**: Output for developers and debugging purposes, includes heart rate, confidence in heart rate, oxygen levels, custom sensor status messages, custom messages for different situations, error/restarting messages, and more
   - **Custom Heart Icon**: Visually appealing heart icon shown on LCD to indicate live readings
- **Resilient Startup and Initialization**
   - Displays clear LCD messages for each stage of setup: configuration, success, or sensor failure
   - Will put the program into an infinite loop that does nothing should a sensor fail to initialize, useful for debugging and safety
- **Developer Experience** 
   - **Readable and Well-Commented Code**: Functions and variables are clearly and accurately named, with in-line documentation to aid replication and modification
   - **Modular Structure**: Code is well organized into initialization, core/supporting logic, and helper sections for easier maintenance and scaling
   - **Modifiable Thresholds**: All critical timing variables (grace/timeout periods) are clearly declared as constants for easy tuning

---

### List of Hardware Components
| Component Name | Function | Additional Notes |
| --- | --- | --- |
| Arduino Uno R3| Microcontroller | - |
| [SparkFun Pulse Oximeter<br> and Heart Rate Monitor](https://www.sparkfun.com/sparkfun-pulse-oximeter-and-heart-rate-sensor-max30101-max32664-qwiic.html) | Collects display data | Connect to 3.3 V
| 1602 LiquidCrystal<br> Display | Main user interface | The one we used had no I2C, hence, many pin headers
|10KΩ Potentiometer | Adjusting contrast on LCD | One end connects to 5V, middle pin connects to LCD,<br>other end connects to GND, order does not matter
| 220Ω Resistor | Resistor for backlight in LCD | - |
| Male to Male Jumper<br> Wires | Connecting components | - |
Male to Female Jumper Wires | Connecting components | - |
Breadboard | Connecting components | Any size, we used a breadboard mini |

---

### Wiring guide:
#### SparkFun Pulse Oximeter and Heart Rate Monitor to Arduino:
| Pin name | Destination | Additional Notes |
| --- | --- | --- |
| MFIO | Digital pin 6 | - |
| RST | Digital pin 7 | - |
| SDA | Analog pin 4 | - |
| SCL | Analog pin 5 | - |
| 3V3 | 3.3 V | Do **NOT** connect to 5V |
| GND | Ground | - |


#### LCD to Arduino:
| Pin name | Destination | Additional Notes |
| --- | --- | --- |
| VSS | Ground | - |
| VDD | 5 V | - |
| VO | 10K Potentiometer | Connected to middle pin<br>Other pins to 5V and GND<br> (any order) |
| RS | Digital pin 2 | - |
| RW | Ground | - |
| E | Digital pin 4 | - |
| D0 - D3 | Unused | Leave these unconnected |
| D4 | Digital pin 8 | - |
| D5 | Digital pin 10 | - |
| D6 | Digital pin 11 | - |
| D7 | Digital pin 13 | - |
| A | 5 V | Through 220Ω resistor |
| K | Ground | - |

---

### Code Overview

#### Core Variables:
|Data Type| Variable Name | Function | Additional Notes |
| --- | --- | --- | --- |
| `bioData` | `body` | From HR Monitor, returns heart rate, oxygen, confidence, and more | Is a struct, and also essential in the buffer |
| `struct` | `MiniBio` | Simplified struct version of bioData to prevent memory overflow | We use no instances of MiniBio, but it plays an essential role in the buffer |
| `MiniBio[]` | `buffer` | Array of previous 15 frames, if all are invalid, then reset | Array of the MiniBio struct |
| `const int` | `bufferSize` | Maximum allowed invalid readings before buffer-induced restart | Users can change this value to better suit their preferences |
| `const int` | `maxInvalidTime` | Maximum amount of time (ms) before program deems time-based restart | Default is `30000` (30 seconds) |
| `bool` | `disconnected` | Master flag for connection loss/data freeze | Triggers display freeze and restart logic |
| `const int` | `MIN_HR` | *Minimum* acceptable heart rate value | Filters extreme values |
| `const int` | `MAX_HR` | *Maximum* acceptable heart rate value | Filters extreme values |
| `const int` | `MIN_CFD` | Minimun acceptable confidence value | Default is `1` |
| `const int` | `MIN_O2` | Minimum acceptable oxygen value | Helps filter sensor glitches and dictates proper course of action |
| `int` | `lastHeartRate` | Keeps track of the last valid HR | This value displays on the LCD, not the live value |
| `int` | `lastConfidence` | Keeps track of the last valid CFD | This value displays on the LCD, not the live value |
| `int` | `lastOxygen` | Keeps track of the last valid O2 | This value is **not displayed** on the LCD |
| `int` | `lastStatus` | Keeps track of the last valid status | This value gets translated by  `getStatusMessage()`, and is then displayed on the LCD |
| `const int` | `freezeTimeout` | Max time between display updates before freeze logic triggers | Defaul: `3000` ms (3 seconds) |
| `const unsigned long` | `gracePeriod` | Initial wait time (ms) before logic kicks in | Default `5000` ms (5 seconds) |
| `const int` | `refreshRate` | The main "clock" of the program | Altering this will change the rate at which the entire program functions, all time-based actions revolve around this variable|


#### Core-logic methods:
- `loop()`: Continuously reads sensor data, prints to serial monitor and LCD, as well as run main logic methods below
- `determineDisconnected()`: Checks for invalid sensor outputs (all-zero readings) and prevents updating the
  display if the sensor is disconnected
- `validateSensorState()`: Calls both `timeCheck()` and `bufferCheck()`. Checks if reinitialization of the sensor by restarting the program is necessary using the following two methods:
   - `timeCheck()`: Determines if it has been 30 seconds of bad readings, and frozen data values are invalid, and if both of these conditions are met, this method restarts the program
   - `bufferCheck()`: Determines if 15 consecutive readings are invalid, and if and only if this is true, then the program restarts due to the buffer check
- `lcdPrinting()`: Outputs readings (may be impacted by data freeze) to LCD UI. Also displays brief messages about status, reinitialization, and data freezes  
- `serialPrinting()`: Outputs current readings and detailed information about status to the Serial Monitor for debugging  

#### Supporting Logic/Utility Methods
- `fingerOnSensor()`: Determines if the sensor detects a finger on it
- `isValidFrame()`: Determines if a frame is "valid," meaning all nonzero data or zero data if a finger is not sensed
- `isValidFrame(MiniBio frame)` (overload): Determines if a passed frame is valid using a counter that keeps "score" of all "points" needed to be valid, and if the passed frame gets at least 2/3 possible points, it is deemed to be a valid frame
- `restartSystem()`: Takes care of restarting the program and provides reasons printed to both the LCD and the serial monitor
- `resetBuffer()`: Refills the buffer with dummy values, called when the buffer has a valid value to essentially clear it
- `bufferLogging()`: Keeps a live log of what values are being entered into the buffer, useful for debugging
- `bufferDubugDump()`: Right before a buffer-induced restart, this method prints all the elements within the buffer to the serial monitor, useful in debugging to see if a restart was necessary or not
- `padInt()`: Takes in an int, returns a formatted string, ex. input of an integer `5` returns the string `"  5"` (note the spaces), used to help format LCD printing
- `getStatusMessage()`: Uses status codes given by sensor and translates them into brief messages to be displayed on the LCD

#### Initialization Methods
- `setup()`: Runs all initialization methods and prepares program for usage
- `initializeLCD()`: Initialized the LiquidCrystal Display, or puts the program in a safe-state if the initialization fails 
- `initializeSensor()`: Initializes the sensor, also puts the program in a safe-state if the initialization fails
- `initializeBuffer()`: Fills the buffer with non-zero values, so, the buffer-based reinitialization doesn't immediately activate
- `finalizeInitialization()`: Displays `"Configured!"` text and plays loading animation (from `loadingHearts()`) to indicate that the program is ready for use
- `printInstructions()`: Calls the next three helper methods to display proper instructions to the LCD
   - `instructionScreen()`: Handles displaying instructional text, including managing delays/clearing the screen and preparing for the next instructional screen
   - `loadingHearts()`: Prints a row of custom heart characters with two spaces in between each heart. A 16 character row should have 6 hearts displayed by default
   - `finalHeartScreen()`: Similar to loadingHearts(), just prints two rows of hearts being printed simultaneously

---
### Credits
- Built using the [SparkFun MAX30101/MAX32664 Pulse Oximeter](https://www.sparkfun.com/sparkfun-pulse-oximeter-and-heart-rate-sensor-max30101-max32664-qwiic.html)

---
