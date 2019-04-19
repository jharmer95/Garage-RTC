/*****************************************************************************
*         File: GarageRTC.ino
*  Description: Implements a real time IoT Garage Door controller.  See 
*               https://github.com/jharmer95/Garage-RTC/ for details on the 
*               Open GarageRTC project.
*      Authors: Daniel Zajac,  danzajac@umich.edu
*               Jackson Harmer, harmer@umich.edu
*
*****************************************************************************/
#include "GarageRTC.h"

// configure global for the LCD port
LiquidCrystal_PCF8574 lcd(0x27);

/*****************************************************************************
*     Function: void setup( void )
*  Description: This function is run once at startup after reset.  It sets up
*               hardware and initalizes the system. 
*
*****************************************************************************/
void setup()
{
	// initialize serial communication
	Serial.begin(SERIALSPEED);
	
  // Setup the serial mutex
	g_serialMutex = portMUX_INITIALIZER_UNLOCKED;

	// initalize digital output pins
	for (int i = 0; i < MAXIO; ++i)
	{
		digitalWrite(outputPins[i], HIGH);  // set the pins for inital pullup
		pinMode(outputPins[i], OUTPUT);     // Assign them the output state
	}

	// initalize digital input pins
	for (int i = 0; i < MAXSWS; ++i)
	{
		pinMode(g_switches[i], INPUT_PULLUP);  // Setup as input with pull up
	}

    // setup the display
	//TODO: DEADCODE Wire.begin();
	//TODO: DEADCODE Wire.beginTransmission(0x27); // configure the SPI

	lcd.begin(20, 4);       // setup the LCD dims
	lcd.setBacklight(128);  // set the backlighting
	lcd.home();             // send the cursor home
	lcd.clear();            // clear the display

	// create a new character, display missing backslash
	lcd.createChar(1, customBackslash);

	// set the static display elements
	lcd.setCursor(0, 0);
	lcd.print("T:         NET:     ");
	lcd.setCursor(0, 1);
	lcd.print("C:      SYSTEM:    ");
	lcd.setCursor(0, 2);
	lcd.print("         LIGHT:     ");
	lcd.setCursor(0, 3);
	lcd.print("          DOOR:     ");
 
  // Share Memory mutex
  g_sharedMemMutex = portMUX_INITIALIZER_UNLOCKED;
  
  // g_WatchDogBowl mutex
  g_wdMutex = portMUX_INITIALIZER_UNLOCKED;
  
	xTaskCreatePinnedToCore(TaskReadSensors, "TaskReadSensors", 
		1024, 
		NULL, 3, // Priority 3 is the highest
		NULL, ARDUINO_RUNNING_CORE);

	xTaskCreatePinnedToCore(TaskUpdateDisplay, "TaskUpdateDisplay", 2048, // Stack size
		NULL, 1, // Priority
		NULL, ARDUINO_RUNNING_CORE);

	xTaskCreatePinnedToCore(TaskPriorityMachines, "TaskPriorityMachines", 1024, // Stack size
		NULL, 2, // Priority
		NULL, ARDUINO_RUNNING_CORE);

	xTaskCreatePinnedToCore(TaskNetwork, "TaskNetwork", 4096, // Stack size
		NULL, 1, // Priority
		NULL, ARDUINO_RUNNING_CORE);

	xTaskCreatePinnedToCore(TaskWatchdog, "TaskWatchdog", 1024, // Stack size
		NULL, 1, // Priority
		NULL, ARDUINO_RUNNING_CORE);

}


/*****************************************************************************
*     Function: void loop( void )
*  Description: This function is run during idle cycles but does no useful
*               work since everything is done in tasks.
*****************************************************************************/
void loop()
{
	// Empty. Things are done in Tasks.
}

/*****************************************************************************
*     Function: void initNetwork( AsyncUDP &udp )
*               AsyncUDP &udp - handle to the UDP connection
*  Description: This function is run once at startup after reset.  It sets up
*               wifi network.
* 
*         TODO: fix this so it will automatically reconnect
*
*   Referecnes: Modifed from example code provided with the WiFi Library. 
*               See GarageRTC.h for the complete reference. 
*****************************************************************************/
void initNetwork(AsyncUDP &udp)
{
	WiFi.mode(WIFI_STA);         // Setup the wifi Mode
	WiFi.begin(g_ssid, g_password);  // Setup the SSID and Password
  bool connected = false;      // local for connected

	// Wait for aconnection
	if (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		// if we do not get a connection, let the serial know
		taskENTER_CRITICAL(&g_serialMutex);
			Serial.println("WiFi Did not connect");
		taskEXIT_CRITICAL(&g_serialMutex);		
		connected = false;
	}
	else
	{
		// otherwise we connected
		connected = true;
		
		// setup the port to listen to
		if (udp.listen(1234))
		{
			// print out the IP we obtained. 
			taskENTER_CRITICAL(&g_serialMutex);
				Serial.print("UDP Listening on IP: ");
				Serial.println(WiFi.localIP());
			taskEXIT_CRITICAL(&g_serialMutex);	
		}
	}

  // write the connection status to the shared variable
  taskENTER_CRITICAL(&g_sharedMemMutex);
    g_Connected = connected; 
  taskEXIT_CRITICAL(&g_sharedMemMutex);  
}

/*****************************************************************************
*     Function: void  TaskReadSensors( void* pvParameters )
*	            void* pvParameters - Paramaters passed in as part of the 
*	                                 scheduler configuration for this task.
*
*  Description: Sensor Processing Tasks - This task is responsible for fetching, 
*               converting values from the sensors and making the results 
*               available other tasks.  In general, this task will perform the 
*               following functions:
*               • Fetch values from sensors
*               • Scale/Convert values
*               • Read/debounce switches
*               • Store values to memory
*               • Update shared variables
*
*****************************************************************************/
void TaskReadSensors(void* pvParameters) 
{
	(void)pvParameters;
	TickType_t xLastWakeTime;

  bool bouncing[MAXSWS];
  int  buttonState[MAXSWS];
  int  stopTime[MAXSWS];
  
  for(int i = 0; i < MAXSWS; i++)
      bouncing[i] = false;

	while (true)
	{
		// Macro that expands to schedule the next wake-up time and then
		// sleep the task until the next schedule wake-up.
		vTaskDelayUntil(&xLastWakeTime, 10);

		// read Analogs - this is the only task that reads so no need to 
		// provide exclusive access.
		int sensorValueT = analogRead(PIN_TEMP);
		int sensorValueCO = analogRead(PIN_CO);

		// read digitals:
		for (int i = 0; i < MAXSWS; ++i)
		{
			debounce(i, bouncing, buttonState, stopTime);
		}

		// print out the value you read:
		float temp_buf = 0.0637 * sensorValueT - 40.116;
		float co_buf = 0.0527 * sensorValueCO - 72.728;
		
		// write back variables to shared globals. Protect with
		// mutex to avoid race condition with another thread
		taskENTER_CRITICAL(&g_sharedMemMutex);
			g_temp = temp_buf;
			g_co = co_buf;
      for(int i = 0; i < MAXSWS; i++)
        g_buttonState[i] = buttonState[i];
		taskEXIT_CRITICAL(&g_sharedMemMutex);
		
		/* think this is dead code
		if (!changeLightState && !buttonState[LIGHT])
		{
			changeLightState = true;
		}
        */
		
		// set my bit in the watchdog
		taskENTER_CRITICAL(&g_wdMutex);
			g_WatchDogBowl = g_WatchDogBowl | 0x1;
		taskEXIT_CRITICAL(&g_wdMutex);
	}
}


/*****************************************************************************
*     Function: void debounce( int pinIndex )
*	            int pinIndex - The index of the pin to debounce. 
*             bool bouncing[] - Array of bools indicating if that switch is 
*                                currently debouncing. 
*             int buttonState[] - Array of current button states.
*             int stopTime[] - array of times to stop debouncing. 
*
*  Description: Debounces the pin for DEBOUNCEMS milliseconds
*
*****************************************************************************/
void debounce(int pinIndex, bool bouncing[], int buttonState[], int stopTime[]) 
{
	// only read if we are in a debounce
	if (!bouncing[pinIndex])
	{
		int current = digitalRead(g_switches[pinIndex]);

		// have we have seen an edge?
		if (buttonState[pinIndex] != current)
		{
			//startTimer and signal debouncing
			stopTime[pinIndex] = esp_log_timestamp() + DEBOUNCEMS;
			bouncing[pinIndex] = true;
		}
	}
	if (bouncing && (esp_log_timestamp() > stopTime[pinIndex])) // see if we are past the bounce window
	{
		//save the value
		buttonState[pinIndex] = digitalRead(g_switches[pinIndex]);

		//reset the bouncing
		bouncing[pinIndex] = false;
	}
}

/*****************************************************************************
*     Function: void  TaskUpdateDisplay( void* pvParameters )
*	            void* pvParameters - Paramaters passed in as part of the 
*	                                 scheduler configuration for this task.
*
*  Description: LCD Display Task - This task is responsible for updating the 
*               local display.  In general, this task will perform the 
*               following functions:
*               • Fetch values from memory
*               • Post readings or status to the display
*               • Update the displays including:
*                   o Temperature
*                   o CO Level
*                   o Connection Status
*                   o System state
*
*         Note: The LCD selected is very slow to update so we do not
*               redraw the entire display each cycle, only updating the chars
*               that have changed.
*
*****************************************************************************/
void TaskUpdateDisplay(void* pvParameters)
{
	(void)pvParameters;
	TickType_t xLastWakeTime;

	char TEMPmsg[6];
	char DOORmsg[5];
  int twiddle = 0;  // the running indicator on the display

  // local buffers for shared memory
  float temp;
  float co;
  bool doorMovingState;
  bool lightOnState;
  int upButtonState;
  int downButtonState;
  bool alarmState;
  bool connected;


	while (true)
	{
		// Macro that expands to schedule the next wake-up time and then
		// sleep the task until the next schedule wake-up.
		vTaskDelayUntil(&xLastWakeTime, 500);

    // buffer in the shared memory
    taskENTER_CRITICAL(&g_sharedMemMutex);
      temp = g_temp;
      doorMovingState = g_doorMovingState;
      lightOnState = g_lightOnState;
      upButtonState = g_buttonState[UP];
      downButtonState = g_buttonState[DOWN];
      alarmState = g_alarmState;
      connected = g_Connected; 
    taskEXIT_CRITICAL(&g_sharedMemMutex);


		lcd.setCursor(0, 3);
		switch(twiddle)
		{
			case 0:
				lcd.write('|');
				break;
			case 1:
				lcd.write('/');
				break;
			case 2:
				lcd.write('-');
				break;
			case 3:
				lcd.write(byte(1));
				break;
		}
		twiddle = (twiddle + 1) % 4;

		// Create temprate string from float
		dtostrf(temp, 5, 1, TEMPmsg);
		lcd.setCursor(2, 0);
		lcd.print(TEMPmsg);

		// update the CO content 
		if (co > HIGHCO)
		{
			lcd.setCursor(2, 1);
			lcd.print("HIGH");
		}
		else if (co > MEDCO)
		{
			lcd.setCursor(2, 1);
			lcd.print("WARN");
		}
		else
		{
			lcd.setCursor(2, 1);
			lcd.print("LOW ");
		}

		// Update the door Position
		if (doorMovingState)
		{
			lcd.setCursor(15, 3);
			lcd.print("MOVE ");
		}
		else if (upButtonState == LOW)
		{
			lcd.setCursor(15, 3);
			lcd.print("OPEN ");
		}
		else if (downButtonState == LOW)
		{
			lcd.setCursor(15, 3);
			lcd.print("CLOSE");
		}
		else
		{
			lcd.setCursor(15, 3);
			lcd.print("STOP ");
		}

		// Update the light state
		if (lightOnState)
		{
			lcd.setCursor(15, 2);
			lcd.print("ON ");
		}
		else
		{
			lcd.setCursor(15, 2);
			lcd.print("OFF");
		}

		// update the alarm state
		if (alarmState)
		{
			lcd.setCursor(15, 1);
			lcd.print("ALARM");
		}
		else
		{
			lcd.setCursor(15, 1);
			lcd.print("OK   ");
		}

		// update the network state
		if (connected)
		{
			lcd.setCursor(15, 0);
			lcd.print("OK  ");
		}
		else
		{
			lcd.setCursor(15, 1);
			lcd.print("NONE");
		}

		// set my bit in the watchdog
		taskENTER_CRITICAL(&g_wdMutex);
			g_WatchDogBowl = g_WatchDogBowl | 0x2;
		taskEXIT_CRITICAL(&g_wdMutex);
	}
}

/*****************************************************************************
*     Function: void  TaskNetwork( void* pvParameters )
*	            void* pvParameters - Paramaters passed in as part of the 
*	                                 scheduler configuration for this task.
*
*  Description:  
*
*
*****************************************************************************/
void TaskNetwork(void* pvParameters)
{
	(void)pvParameters;
	TickType_t xLastWakeTime;
	char wifiBuff[255] = "";
	char tempBuff[6];
	char coBuff[6];
  AsyncUDP udp;

  // local buffers for shared memory
  float temp;
  float co;
  bool lightOnState;
  bool connected;
  
	while (true)
	{
		// Macro that expands to schedule the next wake-up time and then
		// sleep the task until the next schedule wake-up.
		vTaskDelayUntil(&xLastWakeTime, LOW_PRI_TASK_DELAY);


    // buffer in the shared memory
    taskENTER_CRITICAL(&g_sharedMemMutex);
      temp = g_temp;
      co = g_co;
      lightOnState = g_lightOnState;
      connected = g_Connected;
    taskEXIT_CRITICAL(&g_sharedMemMutex);

    if (!connected)
      initNetwork(udp);
      
		if (connected)
		{
      // have to load via dtosttrf due to sprintf bug
			dtostrf(temp, 5, 1, tempBuff);
			dtostrf(co, 5, 1, coBuff);
      sprintf(wifiBuff, "[{\"name\": \"doorStatus\", \"value\": \"%s\"}, {\"name\": \"lightStatus\", \"value\": \"%s\"}, {\"name\": \"tempStatus\", \"value\": \"%s\"}, {\"name\": \"coStatus\", \"value\": \"%s\"}]", "true", lightOnState ? "true" : "false", tempBuff, coBuff);

			udp.broadcast(wifiBuff);
		}

		// set my bit in the watchdog
		taskENTER_CRITICAL(&g_wdMutex);
		  g_WatchDogBowl = g_WatchDogBowl | 0x4;
		taskEXIT_CRITICAL(&g_wdMutex);
	}
}



/*****************************************************************************
*     Function: void TaskPriorityMachines( void* pvParameters )
*	            void* pvParameters - Paramaters passed in as part of the 
*	                                 scheduler configuration for this task.
*
*  Description: This task is responsible monitoring door position, obstacle 
*               detection, and starting or stopping movement.  In general, 
*               this task will perform the following functions:
*               • Maintain Door state
*               • Maintain direction state
*               • Start/Stop door movement
*               • Monitor obstacle detection
*
*   Referecnes: Modifed from example code provided with the WiFi Library. 
*               See GarageRTC.h for the complete reference. 
*****************************************************************************/
void TaskPriorityMachines(void* pvParameters)
{
	(void)pvParameters;
	TickType_t xLastWakeTime;

  // state machine state vars
  byte lightState = 0;
	byte doorState = 0;
  byte alarmState = 0;
  
	int buttonHoldTime;
  
  xLastWakeTime = xTaskGetTickCount();
  
  // local buffers for global variables
  float temp; 
  float co;
  bool doorMovingState;
  int lightButtonState;
  int alarmButtonState;
  int doorButtonState;
  int upButtonState;
  int downButtonState;
  int stopButtonState;
  int obsButtonState;
  bool lightOnState;


	while (true)
	{
		// Macro that expands to schedule the next wake-up time and then
		// sleep the task until the next schedule wake-up.
		vTaskDelayUntil(&xLastWakeTime, MED_PRI_TASK_DELAY);

    // buffer in the shared memory
    taskENTER_CRITICAL(&g_sharedMemMutex);
      temp = g_temp;
      co = g_co;
      lightButtonState = g_buttonState[LIGHT];
      alarmButtonState = g_buttonState[ALARM];
      doorButtonState = g_buttonState[DOOR];
      upButtonState = g_buttonState[UP];
      downButtonState = g_buttonState[DOWN];
      stopButtonState = g_buttonState[STOP];
      obsButtonState = g_buttonState[OBS];
    taskEXIT_CRITICAL(&g_sharedMemMutex);


		// light State machine (on, off)
		switch (lightState)
		{
			case 0:
				if (lightButtonState == LOW)
				{ // switch relay ON
					// switch LED ON
					digitalWrite(RELAY_LIGHT, LOW);
					lightOnState = true;
					lightState = 1;
				}
				break;
			case 1:
				if (lightButtonState == HIGH)
				{
          lightState = 2;
        }
				break;

			case 2:
				if (lightButtonState == LOW)
				{ // switch relay OFF
					// switch LED OFF
					digitalWrite(RELAY_LIGHT, HIGH);
					lightOnState = false;
					lightState = 3;
				}
				break;
			case 3: 
				if (lightButtonState == HIGH)
				{
          lightState = 0;
        }
				break;
		}

		// Door Control State Machine
		switch (doorState)
		{
			case 0:
				if (doorButtonState == LOW) // someone pressed the button
				{
					buttonHoldTime = esp_log_timestamp() + BTNHOLDMIL;
					digitalWrite(RELAY_DOOR, LOW);
					doorMovingState = true;
					doorState = 1;
				}
				break;

			case 1: // hold the button for some time to start moving
				if (esp_log_timestamp() > buttonHoldTime)
				{
					digitalWrite(RELAY_DOOR, HIGH);
					doorState = 2;
				}
				break;

			case 2: // clear the limit switch
				if (((upButtonState == HIGH) && (downButtonState == HIGH)) || (stopButtonState == LOW) || (obsButtonState == LOW))
				{
					doorState = 3;
				}
				break;

			case 3: // moving
				if ((upButtonState == LOW) || (downButtonState == LOW) || (stopButtonState == LOW)
					|| (obsButtonState == LOW)) // Limit switch Hit
				{
					buttonHoldTime = esp_log_timestamp() + BTNHOLDMIL;
					digitalWrite(RELAY_DOOR, LOW);
					doorState = 4;
				}
				break;

			case 4: // stop moving
				if (esp_log_timestamp() > buttonHoldTime)
				{
					digitalWrite(RELAY_DOOR, HIGH);
					doorMovingState = false;
					doorState = 0;
				}
				break;
		}

		// alarm state machine
		switch (alarmState)
		{
			case 0: // idle
				if ((temp > HIGHTEMP) || (temp < LOWTEMP) || (co > HIGHCO))
				{ // ALARM condition
					digitalWrite(RELAY_ALARM, LOW);
					alarmState = 1;
				}
				break;

			case 1: // ALARM
				if ((temp < HIGHTEMP - TEMPHYST) && (temp > LOWTEMP + TEMPHYST) && (co < HIGHCO - COHYST))
				{ // ALARM condition clearing
					digitalWrite(RELAY_ALARM, HIGH);
					alarmState = 0;
				}
				else if (alarmButtonState == LOW)
				{ // Someone manually cleared the alarm
					digitalWrite(RELAY_ALARM, HIGH);
				}
				break;
		}

    // write back global variables
    taskENTER_CRITICAL(&g_sharedMemMutex);
      g_doorMovingState = doorMovingState;
      g_lightOnState = lightOnState;
      g_alarmState = alarmState;
    taskEXIT_CRITICAL(&g_sharedMemMutex);
    
		// set my bit in the watchdog
		taskENTER_CRITICAL(&g_wdMutex);
			g_WatchDogBowl = g_WatchDogBowl | 0x8;
		taskEXIT_CRITICAL(&g_wdMutex);
	}
}

/*****************************************************************************
*     Function: void IRAM_ATTR resetModule( void ) 
*
*  Description: Interrupt service routine for watchdog timer
*
*   Referecnes: Modifed from example code provided with the ESP32 library. 
*               See GarageRTC.h for the complete reference. 
*****************************************************************************/
void IRAM_ATTR resetModule()
{
	esp_restart();  // if called, trigger a systems reset
}

/*****************************************************************************
*     Function: void TaskWatchdog( void* pvParameters )
*	            void* pvParameters - Paramaters passed in as part of the 
*	                                 scheduler configuration for this task.
*
*  Description: This task is responsible monitoring  the watchdog bowl and 
*               ensuring all tasks are running.  If a task fails to update
*               its bit in the bowl, the timer will eventually elapse and
*               reset the device. 
*
*   Referecnes: Modifed from example code provided with the ESP32 library. 
*               See GarageRTC.h for the complete reference.
*****************************************************************************/
void TaskWatchdog(void* pvParameters)
{
	(void)pvParameters;
	TickType_t xLastWakeTime;
	byte g_WatchDogBowl_buff = 0;

	// configure the watchdog timer
	const int wdtTimeout = WDTIMEOUT;   //time in ms to trigger the watchdog
	hw_timer_t* timer = NULL;
	timer = timerBegin(0, 80, true);                  //timer 0, div 80
	timerAttachInterrupt(timer, &resetModule, true);  //attach callback
	timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
	timerAlarmEnable(timer);                          //enable interrupt

	xLastWakeTime = xTaskGetTickCount();

	// Check to see that the watchdog is being fed by all tasks
	// no task is slower than the idle task
	while (true)
	{
		// Macro that expands to schedule the next wake-up time and then
		// sleep the task until the next schedule wake-up.
		vTaskDelayUntil(&xLastWakeTime, 1500);

		taskENTER_CRITICAL(&g_wdMutex);
			if (g_WatchDogBowl == 0xF)
			{
				timerWrite(timer, 0); //reset timer (feed watchdog)
			}
			g_WatchDogBowl = 0;  // clear the bowl 
		taskEXIT_CRITICAL(&g_wdMutex);
	}
}
