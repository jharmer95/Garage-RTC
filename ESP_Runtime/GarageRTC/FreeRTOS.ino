#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
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

// System parameters
#define CO_WARMUP    90000 // worst case CO sensor warm-up time
#define DEBOUNCEMS   10    // debounce time in milliseconds

LiquidCrystal_PCF8574 lcd(0x27); 

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

// ENUMS
enum door_state_t
{
  UP,
  DOWN,
  MOVING,
  START
};


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



// the setup function runs once when you press reset or power the board
void setup() {
  
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(
    TaskReadSensors
    ,  "TaskReadSensors"   // A name just for humans
    ,  1024  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskUpdateDisplay
    ,  "TaskUpdateDisplay"
    ,  1024  // Stack size
    ,  NULL
    ,  1  // Priority
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  initDisplay();
  
}

void initDisplay()
{
  Wire.begin();
  Wire.beginTransmission(0x27);

  lcd.begin(20, 4); 
  lcd.setBacklight(128);
  lcd.home(); lcd.clear();

  // create a new character
  lcd.createChar(1, customBackslash);

  
  lcd.setCursor(0, 0);
  lcd.print("T:XXX.X    NET:NONE");
  lcd.setCursor(0, 1);
  lcd.print("C:XXX%  SYSTEM:ALARM");
  lcd.setCursor(0, 2);
  lcd.print("         LIGHT:OFF");
  lcd.setCursor(0, 3);
  lcd.print("          DOOR:OPEN");  
}

void loop()
{
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskDoorOperation(void *pvParameters) 
{
  (void) pvParameters;

  for (;;) // A Task shall never return or exit.
  {

  }
}


// todo: move these out of globals
int switches[4];
int stopTime[4];
bool bouncing[4];
int buttonState[4];

void TaskReadSensors(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

  
  for (;;)
  {
    // read Analogs:
    int sensorValueT = analogRead(PIN_TEMP);
    int sensorValueCO = analogRead(PIN_CO);
    
    // read digitals: 
    int alarm = digitalRead(BUTTON_ALARM);
    int door = digitalRead(BUTTON_DOOR);
    int stop_button = digitalRead(BUTTON_STOP);
    int light = digitalRead(BUTTON_LIGHT);
    
    
    // print out the value you read:
    // Serial.println(sensorValueCO);
    temp = 0.0637*sensorValueT - 40.116;
    co = 0.0527*sensorValueCO - 72.728;

    // Update shared variables in critical region 
    taskENTER_CRITICAL( );
    
    
    taskEXIT_CRITICAL( );

    vTaskDelay(60);  // one tick delay (15ms) in between reads for stability
  }
}

// saw a rising edge
// ignore everything for BOUNCEMS
// check to see if it is the same
void debounce(int pinIndex) // bool bouncing, int last, int pin,)
{
    // only read if we are in a debounce 
    if(!bouncing[pinIndex])
    {
      int current = digitalRead(switches[pinIndex]);

      // have we have seen an edge?
      if (buttonState[pinIndex] != current)
      {
          //startTimer and signal debouncing
          stopTime[pinIndex] = esp_log_timestamp()+ DEBOUNCEMS;
          bouncing[pinIndex] = true;
      }
     }
     if (bouncing && (esp_log_timestamp() > stopTime[pinIndex]))  // see if we are past the bounce window
      {
          //save the value
          buttonState[pinIndex] = digitalRead(switches[pinIndex]);
          
          //reset the bouncing
          bouncing[pinIndex] = false;
      }
       
 
  
}


#define DELAY_PERIOD 1000

void TaskUpdateDisplay(void *pvParameters) 
{
  (void) pvParameters;
  TickType_t xLastWakeTime;

  char TEMPmsg[6];
  char COmsg[4];

    xLastWakeTime = xTaskGetTickCount();
    int var = 0;

    for( ;; )
    {
        // Wait for the next cycle.
        vTaskDelayUntil( &xLastWakeTime, DELAY_PERIOD );
      lcd.setCursor(0, 3);
      if(var == 0)
      {
        lcd.write('|'); 
      }
      else if(var == 1)
      {
        lcd.write('/');    
      }
      else if(var == 2)
      {
        lcd.write('-'); 
      }
      else if(var == 3)
      {
        lcd.write(byte(1));
      }
      var = (var+1) % 4;
      
      dtostrf(temp, 5, 1, TEMPmsg);
      lcd.setCursor(2, 0);
      lcd.print(TEMPmsg);

      dtostrf(co, 3, 0, COmsg);
      lcd.setCursor(2, 1);
      lcd.print(COmsg);

      
    }

}

void TaskProcessWeb(void *pvParameters) 
{
  (void) pvParameters;
  
  for (;;)
  {
    
    //vTaskDelayUntil( &xLastWakeTime, DELAY_PERIOD );

  }
}
