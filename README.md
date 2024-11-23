Description of plan of Task needed and related for the project:
Tasks and Parameters
1.	Button Listener Task
o	Purpose: Reads button inputs, implements debouncing, and signals operations.
o	Priority: Low (1).
o	Key Actions: Starts operations via semaphore, stops operations, toggles door state.
2.	PreProgrammed Task
o	Purpose: Manages microwave operations (start, stop, pause/resume).
o	Priority: Medium (2).
o	Trigger: Waits for semaphore from Button Listener Task.
o	Key Actions: Handles logic for selected operation, ensures safe pause/resume or stop.
3.	PerformOperation Function
o	Purpose: Executes timed microwave operations.
o	Frequency: Checks elapsed time every 100ms.
o	Key Actions: Tracks elapsed time, handles door state interruptions, logs progress.
________________________________________
Parameters
•	Shared Variables: doorClosed, stopRequested, operationInProgress.
•	Semaphore: xOperationSemaphore for task synchronization.
•	Task Priorities:
o	Button Listener Task: Low.
o	PreProgrammed Task: Medium.
________________________________________
Summary: Button Listener starts operations, PreProgrammed manages operations with safe handling of STOP/door actions, and PerformOperation handles timing and interruptions. All tasks synchronize with semaphores and shared states.
