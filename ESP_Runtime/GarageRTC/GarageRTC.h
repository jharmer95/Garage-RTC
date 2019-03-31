#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>


// Defines to make the code more readable
// todo: move these into a pin array for scanning input/output with enums
#define PIN_TEMP     A7
#define PIN_CO       A6
#define PIN_CO_DIG   13

#define BUTTON_ALARM  5
#define BUTTON_DOOR   4
#define BUTTON_STOP   0
#define BUTTON_LIGHT 15

#define LIMSW_UP     27
#define LIMSW_DOWN   14
#define LIMSW_OBS    12

#define RELAY_DOOR   32
#define RELAY_ALARM  33
#define RELAY_LIGHT  25
#define RELAY_AUX    26

// Flagging bits for timing or other debug
#define DEBUG_T1     23
#define DEBUG_T2     19
#define DEBUG_T3     18
#define DEBUG_T4      2

int outputPins[] = 
{
    RELAY_DOOR,
    RELAY_ALARM,
    RELAY_LIGHT,
    RELAY_AUX,
    DEBUG_T1,   
    DEBUG_T2,  
    DEBUG_T3,   
    DEBUG_T4  
};


// System parameters
#define DELAY_PERIOD 1000
#define MED_PRI_TASK_DELAY  100
#define CO_WARMUP    90000 // worst case CO sensor warm-up time
#define DEBOUNCEMS   10    // debounce time in milliseconds
#define MAXSWS        7    // Max number of switches in the system



/* struct that contains data for IOT outbound UDP
{
  "status": {
    "id": 4,
    "timestamp": "2019-03-31 11:49:14",
    "temp": 66.94709171154325,
    "alarm": "False",
    "light": "True",
    "door": "UP",
    "up_lim": "False",
    "down_lim": "False",
    "co": 113.93413418694331
  }
}
*/
/*
struct udp_send_Xxxx 
{
  const char name[7] = "status";
  int id = 4;
  char timestamp[20] = "2019-03-31 11:49:14";
  float temp = 0.0;
  bool alarm = false;
  bool light = false;
  int door = 4;
  bool up_lim = false;
  bool down_lim = false;
  float co = 0.0;
};
*/


// function Prototypes ///////////////////////////////////////
// Tasks
void TaskDoorOperation(void *pvParameters);
void TaskReadSensors(void *pvParameters);
void TaskUpdateDisplay(void *pvParameters);
void TaskProcessWeb(void *pvParameters);

// general
void initDisplay();
void debounce();
//////////////////////////////////////////////////////////////

// custom char since '\' does not exist on the display
byte customBackslash[8] = {
  0b00000,
  0b10000,
  0b01000,
  0b00100,
  0b00010,
  0b00001,
  0b00000,
  0b00000
};

// todo: move these out of globals
int switches[MAXSWS] = {
  BUTTON_ALARM, 
  BUTTON_DOOR, 
  BUTTON_STOP, 
  BUTTON_LIGHT, 
  LIMSW_UP, 
  LIMSW_DOWN, 
  LIMSW_OBS
};

// Needs to match order above.
enum SWITCH_INDEX {
    ALARM, 
    DOOR, 
    STOP, 
    LIGHT, 
    UP, 
    DOWN, 
    OBS
};

int stopTime[MAXSWS] =    { 0, 0, 0, 0, 0, 0, 0};
bool bouncing[MAXSWS] =   { false, false, false, false, false, false, false} ;
int buttonState[MAXSWS] = { 1, 1, 1, 1, 1, 1, 1};
bool alarmState = false;
bool doorMovingState = false; 
bool lightOnState = false;
bool changeLightState = false; // Mailbox to change the light state

// Shared Variables
float temp = 23.4;
float co = 0.0;
bool lim_up = false;
bool lim_dn = false;
bool lim_ob = false;

// system state variables
// todo: pull this into a malloc memory segment to manage
bool firstRun = true;
bool heating = true;
