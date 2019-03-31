#include "GarageRTC.h"

LiquidCrystal_PCF8574 lcd(0x27); 


// the setup function runs once when you press reset or power the board
void setup() {
  
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  // initalize digital output pins
  for(int i = 0; i < 8; i++)
  {
      digitalWrite(outputPins[i], HIGH);
      pinMode(outputPins[i], OUTPUT);
  }

  // initalize digital input pins
  for(int i = 0; i < MAXSWS; i++)
  {
      pinMode(switches[i], INPUT_PULLUP);
  }

  //todo add serial semaphore

//  if ( xLightChangeSemaphore == NULL ) 
//  {
//    xLightChangeSemaphore = xSemaphoreCreateMutex();  
//    if ( ( xLightChangeSemaphore ) != NULL )
//      xSemaphoreGive( ( xSerialSemaphore ) ); 
//  }
//  
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

  xTaskCreatePinnedToCore(
    TaskProcessWeb
    ,  "TaskProcessWeb"
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

void loop()
{
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
void TaskDoorOperation(void *pvParameters) 
{
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
void TaskReadSensors(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  
  for (;;)
  {
    // read Analogs:
    int sensorValueT = analogRead(PIN_TEMP);
    int sensorValueCO = analogRead(PIN_CO);
    
    // read digitals: 
    for(int i = 0; i < MAXSWS; i++)
    {
      debounce(i);
    }
   
    // print out the value you read:
    //Serial.println(buttonState[LIGHT]);
    float temp_buf = 0.0637*sensorValueT - 40.116;
    float co_buf  = 0.0527*sensorValueCO - 72.728;

    // Update shared variables in critical region 
    //taskENTER_CRITICAL();

    temp = temp_buf;
    co = co_buf;

    if (!changeLightState && !buttonState[LIGHT])
    {
       changeLightState = true;
    }
    
    //taskEXIT_CRITICAL();

    vTaskDelay(60);  // one tick delay (15ms) in between reads for stability
  }
}

/*
 * 
 * 
 */
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
  TickType_t xLastWakeTime;
  int mode = 0;

  xLastWakeTime = xTaskGetTickCount();
    
  for (;;)
  {
      vTaskDelayUntil(&xLastWakeTime, MED_PRI_TASK_DELAY);

//    if(changeLightState)
//    {
//      if ( xSemaphoreTake( xLightSemaphore, ( TickType_t ) 5 ) == pdTRUE )
//      {
//        //set the relay state
//
//        // clear the light state flag
//        changeLightState = false;
//        xSemaphoreGive( xSerialSemaphore ); // Now free or "Give" the Serial Port for others.
//      }
//    }


    Serial.println(buttonState[LIGHT]);
    Serial.print(mode);
    
    // light State machine (on, off) 
    switch( mode )
    {
     case 0://------------------------ I'm off and in restmode
       if ( buttonState[LIGHT] == LOW )
       { // switch relay ON
         // switch LED ON
         digitalWrite(RELAY_LIGHT, LOW);
         mode = 1;
       }
       break;
     case 1://------------------------ I'm in ON mode, w8 4 keyrelease
       if ( buttonState[LIGHT] == HIGH )
         mode = 2;
       break;
     case 2://------------------------ I'm ON and in restmode
       if (buttonState[LIGHT] == LOW )
       { // switch relay OFF
         // switch LED OFF
         digitalWrite(RELAY_LIGHT, HIGH);
         mode = 3;
       }
       break;
     case 3://------------------------ I'm in OFF mode, w8 4 keyrelease
       if ( buttonState[LIGHT] == HIGH )
         mode = 0;
       break;
    }//switch
    
   vTaskDelay(60);  // one tick delay (15ms) in between reads for stability
 
/*
    if(changeLightState)
    {
      //set the releay
      lightOnState = !lightOnState; 
      digitalWrite(RELAY_LIGHT, lightOnState ? LOW : HIGH);
      Serial.println( buttonState[LIGHT]);
      //clear the flag
      changeLightState = false;
    }*/

  }
}
