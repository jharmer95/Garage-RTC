/*****************************************************************************
*         File: GarageRTC.h
*  Description: Implements a real time IoT Garage Door controller.  See
*               https://github.com/jharmer95/Garage-RTC/ for details on the
*               Open GarageRTC project.
*      Authors: Daniel Zajac,  danzajac@umich.edu
*               Jackson Harmer, jharmer@umich.edu
*
*****************************************************************************/
#ifndef GARAGERTC_H_INCLUDE
#define GARAGERTC_H_INCLUDE

#if CONFIG_FREERTOS_UNICORE
#	define ARDUINO_RUNNING_CORE 0
#else
#	define ARDUINO_RUNNING_CORE 1
#endif

/*****************************************************************************
*     Include libraries and references
*
*****************************************************************************/

/*****************************************************************************
*     Library: LiquidCrystal_PCF8574.h
*      Author: Matthias Hertel  www.mathertel.de
*      Source: http://www.mathertel.de/Arduino/LiquidCrystal_PCF8574.aspx
*     Version: 1.1.0
* Description: A library for driving LiquidCrystal displays (LCD) by using the
*              I2C bus and an PCF8574 I2C adapter. This library is derived
*              from the original Arduino LiquidCrystal library and uses the
*              original Wire library for communication.
*****************************************************************************/
#include <LiquidCrystal_PCF8574.h>
/*****************************************************************************
*     Library: WiFi.h
*      Author: Hristo Gochkov <hristo@espressif.com>
*      Source: https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/src
*     Version: 1.0
* Description: With this library you can instantiate Servers, Clients and
*              send/receive UDP packets through WiFi. The shield can connect
*              either to open or encrypted networks (WEP, WPA). The IP
*              address can be assigned statically or through a DHCP. The
*              library can also manage DNS.
*****************************************************************************/
#include "WiFi.h"

/*****************************************************************************
*     Library: AsyncUDP.h
*      Author: Me-No-Dev
*      Source: https://github.com/me-no-dev/ESPAsyncUDP
*     Version: 1.0.0
* Description: Async UDP Library for ESP32
*****************************************************************************/
#include "AsyncUDP.h"

/*****************************************************************************
*     Library: esp_system.h
*      Author: Hristo Gochkov, Ivan Grokhtkov <hristo@espressif.com>
*      Source: https://github.com/espressif/arduino-esp32
*     Version: 1.0.2
* Description: ESP32 Board Support, includes FreeRTOS for ESP32. Reference
*              below.
*****************************************************************************/
#include "esp_system.h"

/*****************************************************************************
*     Library: FreeRTOS Copyright (C) 2015
*      Author: Real Time Engineers Ltd.
*      Source: http://www.FreeRTOS.org
*     Version: FreeRTOS V8.2.0
* Description: FreeRTOS provides completely free yet professionally developed,
*              robust, strictly quality controlled, supported, and cross
*              platform software that is more than just the market leader, it
*              s the industry's de facto standard.
*****************************************************************************/

/*****************************************************************************
*     Library: WIFI_AP.h
*      Author: Daniel Zajac, Jackson Harmer
*      Source: https://github.com/jharmer95/Garage-RTC/
*     Version: 1.0.0
* Description: Contains WIFI credentials
*****************************************************************************/
#include "WIFI_AP.h"

/*****************************************************************************
*     Defines to make addressing Pins easier
*****************************************************************************/
// Defines to make the code more readable
#define PIN_TEMP A7
#define PIN_CO A6
#define PIN_CO_DIG 13

#define BUTTON_ALARM 5
#define BUTTON_DOOR 4
#define BUTTON_STOP 0
#define BUTTON_LIGHT 15

#define LIMSW_UP 27
#define LIMSW_DOWN 12
#define LIMSW_OBS 14

#define RELAY_DOOR 32
#define RELAY_ALARM 33
#define RELAY_LIGHT 25
#define RELAY_AUX 26

// Flagging bits for timing or other debug
#define DEBUG_T1 23
#define DEBUG_T2 19
#define DEBUG_T3 18
#define DEBUG_T4 2

#define DP_OPEN 0
#define DP_CLOSE 1
#define DP_STOP 2
#define DP_MOVE 3

int outputPins[] = { RELAY_DOOR, RELAY_ALARM, RELAY_LIGHT, RELAY_AUX, DEBUG_T1, DEBUG_T2, DEBUG_T3, DEBUG_T4 };

/*****************************************************************************
*     Static system parameters
*
*****************************************************************************/
#define LOW_PRI_TASK_DELAY 1000 // Low priority loop time in mS
#define MED_PRI_TASK_DELAY 100  // Medium Priority Task Loop in mS
#define HIGH_PRI_TASK_DELAY 10  // High Prioirty Task Loop in mS
#define CO_WARMUP 90000         // worst case CO sensor warm-up time
#define DEBOUNCEMS 10           // debounce time in milliseconds
#define MAXSWS 7                // Max number of switches in the system
#define MAXIO 8                 // Max number of IO pins
#define BTNHOLDMIL 1000         // Hold time for the garage door button
#define HIGHTEMP 95             // High temperature alarm limit
#define LOWTEMP 50              // Low Temperature alarm limit
#define TEMPHYST 5              // Temp Hysteresis
#define HIGHCO 20               // High CO Limit
#define MEDCO 10                // High CO Limit
#define COHYST 5                // CO Hysteresis
#define SERIALSPEED 115200      // Serial Baud Rate
#define WDTIMEOUT 5000          // Watch Dog Timeout

/*****************************************************************************
*     Function prototypes
*
*****************************************************************************/
// Tasks
void TaskReadSensors(void* pvParameters);
void TaskUpdateDisplay(void* pvParameters);
void TaskPriorityMachines(void* pvParameters);
void TaskNetwork(void* pvParameters);
void TaskWatchdog(void* pvParameters);

// general
void initWatchdog();
void debounce(int pinIndex, bool bouncing[], int buttonState[], int stopTime[]);
void initNetwork(AsyncUDP& udp);

/*****************************************************************************
*     Global Variables
*
*****************************************************************************/
// custom char since '\' does not exist on the display
static byte customBackslash[8] = { 0b00000, 0b10000, 0b01000, 0b00100, 0b00010, 0b00001, 0b00000, 0b00000 };

static int g_switches[MAXSWS] = { BUTTON_ALARM, BUTTON_DOOR, BUTTON_STOP, BUTTON_LIGHT, LIMSW_UP, LIMSW_DOWN, LIMSW_OBS };

// Needs to match order above.
enum SWITCH_INDEX
{
	ALARM,
	DOOR,
	STOP,
	LIGHT,
	UP,
	DOWN,
	OBS
};

byte g_WatchDogBowl = 0;       // Watchdog Feed bowl
portMUX_TYPE g_wdMutex;        // Mux to protect the watchdog bowl
portMUX_TYPE g_sharedMemMutex; // mutex to protect the shared globals
portMUX_TYPE g_serialMutex;    // Mux to protect the serial device

byte g_buttonState[MAXSWS] = { 1, 1, 1, 1, 1, 1, 1 };
bool g_alarmState = false;
bool g_doorMovingState = false;
bool g_lightOnState = false;
bool g_Connected = false;
byte g_doorPosition = 0;

// Shared Variables
float g_temp = 23.4;
float g_co = 0.0;
byte g_webCmd = 0;
byte g_coState = 0;

// converting the g_coState to a string
const char* g_coStateStr[3] = { "LOW", "WARN", "HIGH" };

bool g_firstRun = true; // Indicating the system is just starting up
bool g_heating = true;  // Indicates the CO sensor is still heating

#endif
