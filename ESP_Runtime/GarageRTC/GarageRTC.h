#if CONFIG_FREERTOS_UNICORE
#	define ARDUINO_RUNNING_CORE 0
#else
#	define ARDUINO_RUNNING_CORE 1
#endif

#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include "WiFi.h"
#include "AsyncUDP.h"
#include "WIFI_AP.h"
#include "esp_system.h"

// WatchDog config
byte WatchDogBowl = 0;
portMUX_TYPE wdMutex;

// Defines to make the code more readable
// TODO: move these into a pin array for scanning input/output with enums
#define PIN_TEMP A7
#define PIN_CO A6
#define PIN_CO_DIG 13

#define BUTTON_ALARM 5
#define BUTTON_DOOR 4
#define BUTTON_STOP 0
#define BUTTON_LIGHT 15

#define LIMSW_UP 27
#define LIMSW_DOWN 14
#define LIMSW_OBS 12

#define RELAY_DOOR 32
#define RELAY_ALARM 33
#define RELAY_LIGHT 25
#define RELAY_AUX 26

// Flagging bits for timing or other debug
#define DEBUG_T1 23
#define DEBUG_T2 19
#define DEBUG_T3 18
#define DEBUG_T4 2

#define TOP 0
#define BOTTOM 1
#define MIDDLE 2

int outputPins[] = { RELAY_DOOR, RELAY_ALARM, RELAY_LIGHT, RELAY_AUX, DEBUG_T1, DEBUG_T2, DEBUG_T3, DEBUG_T4 };

// System parameters
#define DELAY_PERIOD 1000
#define MED_PRI_TASK_DELAY 100
#define CO_WARMUP 90000 // worst case CO sensor warm-up time
#define DEBOUNCEMS 10   // debounce time in milliseconds
#define MAXSWS 7        // Max number of switches in the system
#define BTNHOLDMIL 1000 // Hold time for the garage door button
#define HIGHTEMP 95     // High temperature alarm limit
#define LOWTEMP 50      // Low Temperature alarm limit
#define TEMPHYST 5      // Temp Hysteresis
#define HIGHCO 20       // High CO Limit
#define MEDCO 10        // High CO Limit
#define COHYST 5        // CO Hysteresis

// function Prototypes ///////////////////////////////////////
// Tasks
void TaskDoorOperation(void* pvParameters);
void TaskReadSensors(void* pvParameters);
void TaskUpdateDisplay(void* pvParameters);
void TaskPriorityMachines(void* pvParameters);
void TaskNetwork(void* pvParameters);

// general
void initDisplay();
void initWatchdog();
void debounce();
void initNetwork();
//////////////////////////////////////////////////////////////

// custom char since '\' does not exist on the display
byte customBackslash[8] = { 0b00000, 0b10000, 0b01000, 0b00100, 0b00010, 0b00001, 0b00000, 0b00000 };

// TODO: move these out of globals
int switches[MAXSWS] = { BUTTON_ALARM, BUTTON_DOOR, BUTTON_STOP, BUTTON_LIGHT, LIMSW_UP, LIMSW_DOWN, LIMSW_OBS };

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

int stopTime[MAXSWS] = { 0, 0, 0, 0, 0, 0, 0 };
bool bouncing[MAXSWS] = { false, false, false, false, false, false, false };
int buttonState[MAXSWS] = { 1, 1, 1, 1, 1, 1, 1 };
bool alarmState = false;
bool doorMovingState = false;
bool lightOnState = false;
bool changeLightState = false; // Mailbox to change the light state
bool Connected = false;

// Shared Variables
float temp = 23.4;
float co = 0.0;
bool lim_up = false;
bool lim_dn = false;
bool lim_ob = false;
AsyncUDP udp;

// system state variables
// todo: pull this into a malloc memory segment to manage
bool firstRun = true;
bool heating = true;

char buff[100];
