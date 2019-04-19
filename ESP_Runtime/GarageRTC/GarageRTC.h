/*****************************************************************************
*         File: GarageRTC.h
*  Description: Implements a real time IoT Garage Door controller.  See 
*               https://github.com/jharmer95/Garage-RTC/ for details on the 
*               Open GarageRTC project.
*      Authors: Daniel Zajac,  danzajac@umich.edu
*               Jackson Harmer, harmer@umich.edu
*
*****************************************************************************/
#if CONFIG_FREERTOS_UNICORE
#	define ARDUINO_RUNNING_CORE 0
#else
#	define ARDUINO_RUNNING_CORE 1
#endif

/*****************************************************************************
*     Include libraries
*
*****************************************************************************/
//TODO:DEADCODE #include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include "WiFi.h"
#include "AsyncUDP.h"
#include "WIFI_AP.h"
#include "esp_system.h"

/*****************************************************************************
*     Defines to make addressing Pins easier
*****************************************************************************/
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
#define WDTIMEOUT 3000			// Watch Dog Timeout 

/*****************************************************************************
*     Function prototypes 
*
*****************************************************************************/
// Tasks
void TaskDoorOperation(void* pvParameters);
void TaskReadSensors(void* pvParameters);
void TaskUpdateDisplay(void* pvParameters);
void TaskPriorityMachines(void* pvParameters);
void TaskNetwork(void* pvParameters);

// general
void initDisplay();
void initWatchdog();
void debounce(int pinIndex, bool bouncing[], int buttonState[], int stopTime[]);
void initNetwork(AsyncUDP &udp);

/*****************************************************************************
*     Blobal Variables
*
*****************************************************************************/

// custom char since '\' does not exist on the display
static byte customBackslash[8] = { 0b00000, 0b10000, 0b01000, 0b00100, 0b00010, 0b00001, 0b00000, 0b00000 };
 
static int g_switches[MAXSWS] = { BUTTON_ALARM, 
							BUTTON_DOOR, 
							BUTTON_STOP, 
							BUTTON_LIGHT, 
							LIMSW_UP, 
							LIMSW_DOWN, 
							LIMSW_OBS };

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

byte g_WatchDogBowl = 0;        // Watchdog Feed bowl
portMUX_TYPE g_wdMutex;         // Mux to protect the watchdog bowl
portMUX_TYPE g_sharedMemMutex;  // mutex to protect the shared globals
portMUX_TYPE g_serialMutex;     // Mux to protect the serial device

// todo: convert buttons to byte
int  g_buttonState[MAXSWS] = { 1, 1, 1, 1, 1, 1, 1 };
bool g_alarmState = false;
bool g_doorMovingState = false;
bool g_lightOnState = false;
bool g_Connected = false;

// Shared Variables
float g_temp = 23.4;
float g_co = 0.0;
bool  g_lim_up = false;
bool  g_lim_dn = false;
bool  g_lim_ob = false;



bool g_firstRun = true;  // Indicating the system is just starting up
bool g_heating = true;   // Indicates the CO sensor is still heating

// todo: think this is dead code char g_buff[100];
