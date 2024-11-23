#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Button functionality
#define BUTTON_DEFROST_MEAT 0
#define BUTTON_DEFROST_VEG 1
#define BUTTON_GENERAL_HEAT 2
#define BUTTON_DOOR_SENSOR 3
#define BUTTON_STOP 4
#define LED_PIN 10

// Configuration
const int BUTTON_PINS[] = {9, 8, 7, 6, 5}; // Buttons for modes and actions
const int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
const TickType_t DEBOUNCE_DELAY = pdMS_TO_TICKS(50); // Debounce time in FreeRTOS ticks

// State variables
int buttonStates[NUM_BUTTONS] = {HIGH};
int lastButtonStates[NUM_BUTTONS] = {HIGH};
TickType_t lastDebounceTimes[NUM_BUTTONS] = {0};

// Shared variables
volatile bool doorClosed = true;  // Door sensor state
volatile bool stopRequested = false; // Stop signal
volatile bool operationInProgress = false; // Tracks if an operation is running

// Semaphores
SemaphoreHandle_t xOperationSemaphore;

// Task handles
TaskHandle_t xButtonTask, xPreProgrammedTask;

void setup() {
  Serial.begin(9600);

  // Initialize button pins
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  pinMode(LED_PIN, OUTPUT);

  // Initialize semaphore
  xOperationSemaphore = xSemaphoreCreateBinary();

  // Create FreeRTOS tasks
  xTaskCreate(ButtonListenerTask, "ButtonListener", 256, NULL, 1, &xButtonTask);
  xTaskCreate(PreProgrammedTask, "PreProgrammed", 256, NULL, 2, &xPreProgrammedTask);

  vTaskStartScheduler();
}

void loop() {
  // Empty loop; FreeRTOS handles everything
}

// Button listener task
void ButtonListenerTask(void *pvParameters) {
  (void)pvParameters;

  while (1) {
    TickType_t currentTime = xTaskGetTickCount();

    for (int i = 0; i < NUM_BUTTONS; i++) {
      int reading = digitalRead(BUTTON_PINS[i]);

      // Debounce logic
      if (reading != lastButtonStates[i]) {
        lastDebounceTimes[i] = currentTime;
        lastButtonStates[i] = reading;
      }

      if ((currentTime - lastDebounceTimes[i]) > DEBOUNCE_DELAY) {
        if (reading != buttonStates[i]) {
          buttonStates[i] = reading;

          if (buttonStates[i] == LOW) { // Button pressed
            if (operationInProgress) {
              // Allow only STOP and DOOR SENSOR during operations
              if (i == BUTTON_STOP) {
                stopRequested = true;
                Serial.println("Stop requested");
              } else if (i == BUTTON_DOOR_SENSOR) {
                doorClosed = !doorClosed; // Toggle door state
                Serial.print("Door ");
                Serial.println(doorClosed ? "closed" : "opened");

                if (doorClosed) {
                  if (!operationInProgress) {
                    digitalWrite(LED_PIN, LOW); // Turn off LED if door closes and no operation
                  }
                } else {
                  digitalWrite(LED_PIN, HIGH); // Turn on LED when door opens
                }
              }
            } else {
              // Normal button handling when no operation is in progress
              switch (i) {
                case BUTTON_DEFROST_MEAT:
                  Serial.println("Button: Defrost Meat selected");
                  operationInProgress = true; // Mark operation as active
                  stopRequested = false; // Reset the stop flag
                  xSemaphoreGive(xOperationSemaphore); // Notify task
                  break;
                case BUTTON_DEFROST_VEG:
                  Serial.println("Button: Defrost Vegetables selected");
                  operationInProgress = true; // Mark operation as active
                  stopRequested = false; // Reset the stop flag
                  xSemaphoreGive(xOperationSemaphore); // Notify task
                  break;
                case BUTTON_GENERAL_HEAT:
                  Serial.println("Button: General Heating selected");
                  operationInProgress = true; // Mark operation as active
                  stopRequested = false; // Reset the stop flag
                  xSemaphoreGive(xOperationSemaphore); // Notify task
                  break;
                case BUTTON_DOOR_SENSOR:
                  doorClosed = !doorClosed; // Toggle door state
                  Serial.print("Door ");
                  Serial.println(doorClosed ? "closed" : "opened");
                  if (doorClosed) {
                    if (!operationInProgress) {
                      digitalWrite(LED_PIN, LOW); // Turn off LED if door closes and no operation
                    }
                  } else {
                    digitalWrite(LED_PIN, HIGH); // Turn on LED when door opens
                  }
                  break;
                case BUTTON_STOP:
                  stopRequested = true; // Stop all operations
                  Serial.println("Stop requested");
                  break;
              }
            }
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Poll buttons every 10ms
  }
}

// Pre-programmed task for handling modes
void PreProgrammedTask(void *pvParameters) {
  while (1) {
    // Wait for a signal from the button task
    if (xSemaphoreTake(xOperationSemaphore, portMAX_DELAY)) {
      if (stopRequested) {
        Serial.println("Operation stopped");
        stopRequested = false; // Reset stop flag
        operationInProgress = false; // Reset operation state
        digitalWrite(LED_PIN, LOW); // Turn off LED when stopped
        continue;
      }

      if (!doorClosed) {
        Serial.println("Cannot start: Door is open.");
        operationInProgress = false; // Reset operation state
        continue;
      }

      // Execute selected mode based on button press
      if (buttonStates[BUTTON_DEFROST_MEAT] == LOW) {
        Serial.println("Starting: Defrost Meat (5 minutes, 800W)");
        PerformOperation(800, pdMS_TO_TICKS(300000)); // 5 minutes
      } else if (buttonStates[BUTTON_DEFROST_VEG] == LOW) {
        Serial.println("Starting: Defrost Vegetables (1 minute, 400W)");
        PerformOperation(400, pdMS_TO_TICKS(60000)); // 1 minute
      } else if (buttonStates[BUTTON_GENERAL_HEAT] == LOW) {
        Serial.println("Starting: General Heating (30 seconds, 800W)");
        PerformOperation(800, pdMS_TO_TICKS(30000)); // 30 seconds
      }

      operationInProgress = false; // Mark operation as complete
      digitalWrite(LED_PIN, LOW); // Turn off LED after operation
    }
  }
}

// Perform a microwave operation
void PerformOperation(int power, TickType_t duration) {
  Serial.print("Heater ON: ");
  Serial.print(power);
  Serial.println("W");

  digitalWrite(LED_PIN, HIGH); // Turn on LED during operation

  TickType_t startTime = xTaskGetTickCount();
  TickType_t elapsedTime = 0;
  TickType_t lastReportedTime = 0; // Tracks the last reported second

  while (elapsedTime < duration) {
    if (stopRequested) {
      Serial.println("Operation stopped.");
      break;
    }

    if (!doorClosed) {
      Serial.println("Door opened: Pausing operation.");
      digitalWrite(LED_PIN, HIGH); // Keep LED ON while paused
      while (!doorClosed) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait until the door is closed
      }
      Serial.println("Door closed: Resuming operation.");
      // Update startTime to account for the pause
      startTime = xTaskGetTickCount() - elapsedTime;
    }

    // Update elapsed time
    elapsedTime = xTaskGetTickCount() - startTime;

    // Report elapsed time every second
    if ((elapsedTime / 1000) != lastReportedTime) {
      lastReportedTime = elapsedTime / 1000;
      Serial.print("Elapsed time: ");
      Serial.print(lastReportedTime);
      Serial.println(" seconds");
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
  }

  Serial.println("Heater OFF");
  digitalWrite(LED_PIN, LOW); // Turn off LED after operation
}
