#include "GarageRTC.h"

LiquidCrystal_PCF8574 lcd(0x27);



////////////////////////////////////////////////////////////////////////

const byte interruptPin = 12;
volatile int interruptCounter = 0;
int numberOfInterrupts = 0;
 
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ISR example
void IRAM_ATTR handleInterrupt() {
  portENTER_CRITICAL_ISR(&mux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&mux);
}

////////////////////////////////////////////////////////////////////////

// the setup function runs once when you press reset or power the board
void setup() {

  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  // initalize digital output pins
  for (int i = 0; i < 8; i++) {
    digitalWrite(outputPins[i], HIGH);
    pinMode(outputPins[i], OUTPUT);
  }

  // initalize digital input pins
  for (int i = 0; i < MAXSWS; i++) {
    pinMode(switches[i], INPUT_PULLUP);
  }

  initDisplay();
  initNetwork();

  //todo add serial semaphore

  //  if ( xLightChangeSemaphore == NULL ) 
  //  {
  //    xLightChangeSemaphore = xSemaphoreCreateMutex();  
  //    if ( ( xLightChangeSemaphore ) != NULL )
  //      xSemaphoreGive( ( xSerialSemaphore ) ); 
  //  }
  //  
  // Now set up two tasks to run independently.

  // WatchDogBowl mutex
  wdMutex = portMUX_INITIALIZER_UNLOCKED;
  //initWatchdog();
  
  xTaskCreatePinnedToCore(
    TaskReadSensors, "TaskReadSensors" // A name just for humans
    , 1024 // This stack size can be checked & adjusted by reading the Stack Highwater
    , NULL, 2 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    , NULL, ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskUpdateDisplay, "TaskUpdateDisplay", 1024 // Stack size
    , NULL, 1 // Priority
    , NULL, ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskPriorityMachines, "TaskPriorityMachines", 1024 // Stack size
    , NULL, 1 // Priority
    , NULL, ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskNetwork, "TaskNetwork", 1024 // Stack size
    , NULL, 1 // Priority
    , NULL, ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    IdleTask, "IdleTask", 1024 // Stack size
    , NULL, 1 // Priority
    , NULL, ARDUINO_RUNNING_CORE);
  
}






void IRAM_ATTR resetModule() {
  esp_restart();
}

void initWatchdog() {
  
}

// todo fix this so it will automatically reconnect
void initNetwork() {
        
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      
      if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed");
        Connected = false;
      } else {
        Connected = true;
        if (udp.listen(1234)) {
          Serial.print("UDP Listening on IP: ");
          Serial.println(WiFi.localIP());
        }
     }
}

void initDisplay() {
  Wire.begin();
  Wire.beginTransmission(0x27);

  lcd.begin(20, 4);
  lcd.setBacklight(128);
  lcd.home();
  lcd.clear();

  // create a new character
  lcd.createChar(1, customBackslash);

  // set the static display elements
  lcd.setCursor(0, 0);
  lcd.print("T:XXX.X    NET:NONE");
  lcd.setCursor(0, 1);
  lcd.print("C:XXX%  SYSTEM:ALARM");
  lcd.setCursor(0, 2);
  lcd.print("         LIGHT:OFF");
  lcd.setCursor(0, 3);
  lcd.print("          DOOR:OPEN");
}

void loop() {
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

/* Door Operation Control Task - Dan
    This task is responsible monitoring door position, obstacle detection, and starting or stopping movement.  In general, this task will perform the following functions:
    • Maintain Door state
    • Maintain direction state
    • Start/Stop door movement
    • Monitor obstacle detection
 */
void TaskDoorOperation(void * pvParameters) {
  (void) pvParameters;

  for (;;) // A Task shall never return or exit.
  {

  }
}

/* Sensor Processing Tasks - Dan
    This task is responsible for fetching, converting values from 
    the sensors and making the results available other tasks.  In 
    general, this task will perform the following functions:
    • Fetch values from sensors
    • Scale/Convert values
    • Read/debounce switches
    • Store values to memory
    • Update shared variables
 */
void TaskReadSensors(void * pvParameters) // This is a task.
{
  (void) pvParameters;
  TickType_t xLastWakeTime;

  for (;;) {
    vTaskDelayUntil( & xLastWakeTime, 10);
 
    digitalWrite(DEBUG_T2, HIGH);
    
  
    // read Analogs:
    int sensorValueT = analogRead(PIN_TEMP);
    int sensorValueCO = analogRead(PIN_CO);

    // read digitals: 
    for (int i = 0; i < MAXSWS; i++) {
      debounce(i);
    }

    // print out the value you read:
    float temp_buf = 0.0637 * sensorValueT - 40.116;
    float co_buf = 0.0527 * sensorValueCO - 72.728;

    // Update shared variables in critical region 
    //taskENTER_CRITICAL();

    temp = temp_buf;
    co = co_buf;

    if (!changeLightState && !buttonState[LIGHT]) {
      changeLightState = true;
    }

    //taskEXIT_CRITICAL();

    //vTaskDelay(60); // one tick delay (15ms) in between reads for stability
    
    digitalWrite(DEBUG_T2, LOW);
    
    // set my bit in the watchdog
    taskENTER_CRITICAL(&wdMutex);
    WatchDogBowl = WatchDogBowl|0x1 ;
    taskEXIT_CRITICAL(&wdMutex);
  }
}

/* debounce(int pinIndex)
 *  Debounces the pin for DEBOUNCEMS milliseconds
 * 
 */
void debounce(int pinIndex) // bool bouncing, int last, int pin,)
{
  // only read if we are in a debounce 
  if (!bouncing[pinIndex]) {
    int current = digitalRead(switches[pinIndex]);

    // have we have seen an edge?
    if (buttonState[pinIndex] != current) {
      //startTimer and signal debouncing
      stopTime[pinIndex] = esp_log_timestamp() + DEBOUNCEMS;
      bouncing[pinIndex] = true;
    }
  }
  if (bouncing && (esp_log_timestamp() > stopTime[pinIndex])) // see if we are past the bounce window
  {
    //save the value
    buttonState[pinIndex] = digitalRead(switches[pinIndex]);

    //reset the bouncing
    bouncing[pinIndex] = false;
  }
}

/* LCD Display Task - Jackson
    This task is responsible for updating the local display.  In 
    general, this task will perform the following functions:
    • Fetch values from memory
    • Post readings or status to the display
    • Rotate through the different displays including, but not limited to:
      o Temperature
      o CO Level
      o Connection Status
      o System state
 */
void TaskUpdateDisplay(void * pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime;

  char TEMPmsg[6];
  //char COmsg[4];
  char DOORmsg[5];

  xLastWakeTime = xTaskGetTickCount();
  int
  var = 0;

  for (;;) {
    // Wait for the next cycle.
    vTaskDelayUntil( & xLastWakeTime, 500);
    digitalWrite(DEBUG_T3, HIGH);
    
    lcd.setCursor(0, 3);
    if (var == 0) {
      lcd.write('|');
    } else if (var == 1) {
      lcd.write('/');
    } else if (var == 2) {
      lcd.write('-');
    } else if (var == 3) {
      lcd.write(byte(1));
    }
    var = (var +1) % 4;

    dtostrf(temp, 5, 1, TEMPmsg);
    lcd.setCursor(2, 0);
    lcd.print(TEMPmsg);

    /*dtostrf(co, 3, 0, COmsg);
    lcd.setCursor(2, 1);
    lcd.print(COmsg);
    */
    if (co > HIGHCO)
    {
      lcd.setCursor(2, 1);
      lcd.print("HIGH");
    } 
    else if (co > MEDCO) {
      lcd.setCursor(2, 1);
      lcd.print("WARN");
    }  
    else {
      lcd.setCursor(2, 1);
      lcd.print("LOW ");
    } 



    if (doorMovingState) {
      lcd.setCursor(15, 3);
      lcd.print("MOVE ");
    } 
    else if (buttonState[UP] == LOW)
    {
      lcd.setCursor(15, 3);
      lcd.print("OPEN ");
    } 
    else if (buttonState[DOWN] == LOW) {
      lcd.setCursor(15, 3);
      lcd.print("CLOSE");
    }  
    else {
      lcd.setCursor(15, 3);
      lcd.print("STOP ");
    }

    if (lightOnState) {
      lcd.setCursor(15, 2);
      lcd.print("ON ");
    } else {
      lcd.setCursor(15, 2);
      lcd.print("OFF");
    }

    if (alarmState) {
      lcd.setCursor(15, 1);
      lcd.print("ALARM");
    } else {
      lcd.setCursor(15, 1);
      lcd.print("OK   ");
    }
    
    if (Connected) {
      lcd.setCursor(15, 0);
      lcd.print("OK  ");
    } else {
      lcd.setCursor(15, 1);
      lcd.print("NONE");
    }
    
    digitalWrite(DEBUG_T3, LOW);

    // set my bit in the watchdog
    taskENTER_CRITICAL(&wdMutex);
    WatchDogBowl = WatchDogBowl|0x2 ;
    taskEXIT_CRITICAL(&wdMutex);
  }
}

void TaskNetwork(void * pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime;
  char wifiBuff[50] = "abcd";
  char tempBuff[6];
  char coBuff[6];
  int rollCounter = 0;

  for (;;) {
    vTaskDelayUntil( & xLastWakeTime, DELAY_PERIOD);

    if (!Connected) {
    } else {
      
      // have to load via dtosttrf due to sprintf bug
      dtostrf(temp, 5,1, tempBuff);
      dtostrf(co, 5,1, coBuff);
      sprintf(wifiBuff, "{\"status\": {\"id\": %d,\"timestamp\": \"2019-03-31 11:49:14\",\"temp\": %s,\"alarm\": \"%s\",\"light\": \"%s\",\"door\": \"%s\",\"up_lim\": \"%s\",\"down_lim\": \"%s\",\"co\": %s}}",
                        rollCounter, tempBuff, alarmState ? "True" : "False" , lightOnState ? "True" : "False", "TODO", buttonState[UP] ? "True" : "False", buttonState[DOWN] ? "True" : "False", coBuff);
         
   
      udp.broadcast(wifiBuff);
      rollCounter++;
    }

    // set my bit in the watchdog
    taskENTER_CRITICAL(&wdMutex);
    WatchDogBowl = WatchDogBowl|0x4 ;
    taskEXIT_CRITICAL(&wdMutex);
  }

}

void TaskPriorityMachines(void * pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime;
  int mode = 0;

  int state = 0;
  int buttonHoldTime;

  alarmState = 0;
  
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil( & xLastWakeTime, MED_PRI_TASK_DELAY);

    // light State machine (on, off) 
    switch (mode) {
    case 0: //------------------------ I'm off and in restmode
      if (buttonState[LIGHT] == LOW) { // switch relay ON
        // switch LED ON
        digitalWrite(RELAY_LIGHT, LOW);
        lightOnState = true;
        mode = 1;
      }
      break;
    case 1: //------------------------ I'm in ON mode, w8 4 keyrelease
      if (buttonState[LIGHT] == HIGH)
        mode = 2;
      break;
    case 2: //------------------------ I'm ON and in restmode
      if (buttonState[LIGHT] == LOW) { // switch relay OFF
        // switch LED OFF
        digitalWrite(RELAY_LIGHT, HIGH);
        lightOnState = false;
        mode = 3;
      }
      break;
    case 3: //------------------------ I'm in OFF mode, w8 4 keyrelease
      if (buttonState[LIGHT] == HIGH)
        mode = 0;
      break;
    } //switch


    // Door Control State Machine
    
    switch (state) {
    case 0:
      if (buttonState[DOOR] == LOW) // someone pressed the button
      {
        buttonHoldTime = esp_log_timestamp() + BTNHOLDMIL;
        digitalWrite(RELAY_DOOR, LOW);
        doorMovingState = true;
        state = 1;
      }
      break;
    case 1: // hold the button for some time to start moving
      if (esp_log_timestamp() > buttonHoldTime) {
        digitalWrite(RELAY_DOOR, HIGH);
        state = 2;
      }
      break;
    case 2: // clear the limit switch
      if (((buttonState[UP] == HIGH) && (buttonState[DOWN] == HIGH)) || (buttonState[STOP] == LOW) || (buttonState[OBS] == LOW))
      {
         state = 3; 
      }
      break;
    case 3: // moving
      if ((buttonState[UP] == LOW) || (buttonState[DOWN] == LOW) || (buttonState[STOP] == LOW) || (buttonState[OBS] == LOW)) // Limit switch Hit
      {
        buttonHoldTime = esp_log_timestamp() + BTNHOLDMIL;
        digitalWrite(RELAY_DOOR, LOW);
        state = 4;
      }
      break;
    case 4: // stop moving
      if (esp_log_timestamp() > buttonHoldTime) {
        digitalWrite(RELAY_DOOR, HIGH);
        doorMovingState = false;
        state = 0;
      }
      break;
    default:
      // todo: some error checking
      break;
    }

    // alarm state machine
    switch (alarmState) {
      case 0: // idle
        if (( temp > HIGHTEMP ) || ( temp < LOWTEMP ) || ( co > HIGHCO ) )
        { // ALARM condition
          digitalWrite(RELAY_ALARM, LOW);
          alarmState = 1;
        }
        break;
       case 1: // ALARM
        if (( temp < HIGHTEMP - TEMPHYST ) && ( temp > LOWTEMP + TEMPHYST ) && ( co < HIGHCO - COHYST ) )
        { // ALARM condition clearing
          digitalWrite(RELAY_ALARM, HIGH);
          alarmState = 0;
        }     
        else if (buttonState[ALARM] == LOW)
        { // Someone manually cleared the alarm
          digitalWrite(RELAY_ALARM, HIGH);
        }  
        break;          
    }

    // set my bit in the watchdog
    taskENTER_CRITICAL(&wdMutex);
    WatchDogBowl = WatchDogBowl|0x8;
    taskEXIT_CRITICAL(&wdMutex);
  }
}



void IdleTask(void * pvParameters) {
  (void) pvParameters;
  TickType_t xLastWakeTime;
  byte watchDogBowl_buff = 0;
  
  const int wdtTimeout = 3000;  //time in ms to trigger the watchdog
  hw_timer_t *timer = NULL;
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  timerAlarmEnable(timer);                          //enable interrupt

  xLastWakeTime = xTaskGetTickCount();

  // Check to see that the watchdog is being fed by all tasks
  // no task is slower than the idle task
  for (;;) {
    vTaskDelayUntil( & xLastWakeTime, 1500);
    
    taskENTER_CRITICAL(&wdMutex);
    Serial.println(WatchDogBowl, HEX);
    if ( WatchDogBowl == 0xF)
    {
        timerWrite(timer, 0); //reset timer (feed watchdog)        
    }
    WatchDogBowl = 0;
    taskEXIT_CRITICAL(&wdMutex);
  }
}
