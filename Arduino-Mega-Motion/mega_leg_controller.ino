#include <SoftwareSerial.h>     // For Bluetooth communication
#include <Servo.h>              // Servo library for motor control

SoftwareSerial Bluetooth(12, 9); // Arduino RX 12 and TX 9 -> HC-06 Bluetooth TX RX

// ================= CONSTANTS AND CONFIGURATION =================

// Movement parameters
const int SERVO_DELAY = 5;      // Base delay for servo movements
const int STEP_SIZE = 30;       // Default step size in degrees
const int LIFT_HEIGHT = 15;     // Height to lift legs during walking
const int MIN_VOLTAGE = 580;    // Battery low voltage threshold (10-bit ADC)

// Servo limits for safety
const int SERVO_MIN = 30;       // Minimum servo angle
const int SERVO_MAX = 150;      // Maximum servo angle

// Pins
const int LED_PIN = 17;
const int BATTERY_PIN = A1;

// ================= SERVO STRUCTURE =================

struct Leg {
  Servo coxa;     // Motor 1 - Hip rotation (left/right)
  Servo femur;    // Motor 2 - Thigh (up/down)
  Servo tibia;    // Motor 3 - Shin (up/down)
  
  // Home positions
  int home_coxa;
  int home_femur;
  int home_tibia;
  
  // Current positions (for smooth movement)
  int current_coxa;
  int current_femur;
  int current_tibia;
};

// Initialize leg array
Leg legs[6];

// ================= GLOBAL VARIABLES =================

int received_command = 0;       // Bluetooth command
int movement_mode = 0;          // Current movement mode
int movement_delay = 5;         // Dynamic movement delay
bool tripod_group_A = true;     // Tripod gait phase tracking
unsigned long last_movement = 0; // Timing for smooth movement

// Movement state variables for smooth gait
int gait_phase = 0;             // 0-7 phases in walking cycle
bool is_moving = false;         // Movement state flag

// ================= SETUP FUNCTION =================

void setup() {
  Serial.begin(9600);
  Bluetooth.begin(9600);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize leg structures with corrected home positions
  initializeLegs();
  
  // Attach servos to pins
  attachServos();
  
  // Set all servos to home position
  setHomePosition();
  
  Serial.println("Hexapod Robot Initialized");
  Serial.println("Corrected version with proper gait and servo control");
}

// ================= MAIN LOOP =================

void loop() {
  // Check battery voltage
  checkBattery();
  
  // Process Bluetooth commands
  if (Bluetooth.available() > 0) {
    processBluetoothCommand();
  }
  
  // Execute movement based on mode
  executeMovement();
  
  // Small delay for system stability
  delay(movement_delay);
}

// ================= INITIALIZATION FUNCTIONS =================

void initializeLegs() {
  // Corrected home positions with proper symmetry
  // Right side legs (1, 3, 5) - indices 0, 2, 4
  legs[0] = {Servo(), Servo(), Servo(), 90, 90, 90, 90, 90, 90};  // Leg 1
  legs[2] = {Servo(), Servo(), Servo(), 90, 90, 90, 90, 90, 90};  // Leg 3
  legs[4] = {Servo(), Servo(), Servo(), 90, 90, 90, 90, 90, 90};  // Leg 5
  
  // Left side legs (2, 4, 6) - indices 1, 3, 5
  legs[1] = {Servo(), Servo(), Servo(), 90, 90, 90, 90, 90, 90};  // Leg 2
  legs[3] = {Servo(), Servo(), Servo(), 90, 90, 90, 90, 90, 90};  // Leg 4
  legs[5] = {Servo(), Servo(), Servo(), 90, 90, 90, 90, 90, 90};  // Leg 6
}

void attachServos() {
  // Leg 1 (index 0) - Right Front
  legs[0].coxa.attach(35);
  legs[0].femur.attach(37);
  legs[0].tibia.attach(39);
  
  // Leg 2 (index 1) - Left Front  
  legs[1].coxa.attach(29);
  legs[1].femur.attach(31);
  legs[1].tibia.attach(33);
  
  // Leg 3 (index 2) - Right Middle
  legs[2].coxa.attach(34);
  legs[2].femur.attach(36);
  legs[2].tibia.attach(38);
  
  // Leg 4 (index 3) - Left Middle
  legs[3].coxa.attach(26);
  legs[3].femur.attach(24);
  legs[3].tibia.attach(22);
  
  // Leg 5 (index 4) - Right Rear
  legs[4].coxa.attach(32);
  legs[4].femur.attach(30);
  legs[4].tibia.attach(28);
  
  // Leg 6 (index 5) - Left Rear
  legs[5].coxa.attach(27);
  legs[5].femur.attach(25);
  legs[5].tibia.attach(23);
}

// ================= SERVO CONTROL FUNCTIONS =================

void moveServoSmooth(Servo &servo, int &current_pos, int target_pos, int speed_delay) {
  // Constrain target position to safe limits
  target_pos = constrain(target_pos, SERVO_MIN, SERVO_MAX);
  
  // Move gradually to target position
  if (current_pos < target_pos) {
    current_pos++;
    servo.write(current_pos);
  } else if (current_pos > target_pos) {
    current_pos--;
    servo.write(current_pos);
  }
  
  if (speed_delay > 0) {
    delay(speed_delay);
  }
}

void setLegPosition(int leg_index, int coxa_pos, int femur_pos, int tibia_pos) {
  // Apply directional compensation for left side legs
  if (leg_index % 2 == 1) { // Left side legs (2, 4, 6)
    coxa_pos = 180 - coxa_pos; // Mirror the coxa movement
  }
  
  // Move servos to target positions
  moveServoSmooth(legs[leg_index].coxa, legs[leg_index].current_coxa, coxa_pos, 1);
  moveServoSmooth(legs[leg_index].femur, legs[leg_index].current_femur, femur_pos, 1);
  moveServoSmooth(legs[leg_index].tibia, legs[leg_index].current_tibia, tibia_pos, 1);
}

void setHomePosition() {
  Serial.println("Setting home position...");
  
  for (int i = 0; i < 6; i++) {
    setLegPosition(i, legs[i].home_coxa, legs[i].home_femur, legs[i].home_tibia);
  }
  
  // Allow time for all servos to reach position
  delay(1000);
  
  Serial.println("Home position set");
}

// ================= GAIT FUNCTIONS =================

void liftLeg(int leg_index) {
  int lift_femur = legs[leg_index].home_femur - LIFT_HEIGHT;
  int lift_tibia = legs[leg_index].home_tibia - LIFT_HEIGHT;
  
  setLegPosition(leg_index, legs[leg_index].current_coxa, lift_femur, lift_tibia);
}

void lowerLeg(int leg_index) {
  setLegPosition(leg_index, legs[leg_index].current_coxa, 
                 legs[leg_index].home_femur, legs[leg_index].home_tibia);
}

void moveLegForward(int leg_index, int step_size) {
  int new_coxa = legs[leg_index].home_coxa + step_size;
  setLegPosition(leg_index, new_coxa, legs[leg_index].current_femur, legs[leg_index].current_tibia);
}

void moveLegBackward(int leg_index, int step_size) {
  int new_coxa = legs[leg_index].home_coxa - step_size;
  setLegPosition(leg_index, new_coxa, legs[leg_index].current_femur, legs[leg_index].current_tibia);
}

// ================= MOVEMENT FUNCTIONS =================

void moveForward() {
  switch (gait_phase) {
    case 0: // Lift Group A legs (1, 3, 5)
      liftLeg(0); liftLeg(2); liftLeg(4);
      gait_phase++;
      break;
      
    case 1: // Move Group A forward
      moveLegForward(0, STEP_SIZE);
      moveLegForward(2, STEP_SIZE);
      moveLegForward(4, STEP_SIZE);
      gait_phase++;
      break;
      
    case 2: // Lower Group A
      lowerLeg(0); lowerLeg(2); lowerLeg(4);
      gait_phase++;
      break;
      
    case 3: // Move Group B backward (push)
      moveLegBackward(1, STEP_SIZE);
      moveLegBackward(3, STEP_SIZE);
      moveLegBackward(5, STEP_SIZE);
      gait_phase++;
      break;
      
    case 4: // Lift Group B legs (2, 4, 6)
      liftLeg(1); liftLeg(3); liftLeg(5);
      gait_phase++;
      break;
      
    case 5: // Move Group B forward
      moveLegForward(1, STEP_SIZE);
      moveLegForward(3, STEP_SIZE);
      moveLegForward(5, STEP_SIZE);
      gait_phase++;
      break;
      
    case 6: // Lower Group B
      lowerLeg(1); lowerLeg(3); lowerLeg(5);
      gait_phase++;
      break;
      
    case 7: // Move Group A backward (push)
      moveLegBackward(0, STEP_SIZE);
      moveLegBackward(2, STEP_SIZE);
      moveLegBackward(4, STEP_SIZE);
      gait_phase = 0; // Reset cycle
      break;
  }
}

void moveBackward() {
  switch (gait_phase) {
    case 0: // Lift Group A legs (1, 3, 5)
      liftLeg(0); liftLeg(2); liftLeg(4);
      gait_phase++;
      break;
      
    case 1: // Move Group A backward
      moveLegBackward(0, STEP_SIZE);
      moveLegBackward(2, STEP_SIZE);
      moveLegBackward(4, STEP_SIZE);
      gait_phase++;
      break;
      
    case 2: // Lower Group A
      lowerLeg(0); lowerLeg(2); lowerLeg(4);
      gait_phase++;
      break;
      
    case 3: // Move Group B forward (push)
      moveLegForward(1, STEP_SIZE);
      moveLegForward(3, STEP_SIZE);
      moveLegForward(5, STEP_SIZE);
      gait_phase++;
      break;
      
    case 4: // Lift Group B legs (2, 4, 6)
      liftLeg(1); liftLeg(3); liftLeg(5);
      gait_phase++;
      break;
      
    case 5: // Move Group B backward
      moveLegBackward(1, STEP_SIZE);
      moveLegBackward(3, STEP_SIZE);
      moveLegBackward(5, STEP_SIZE);
      gait_phase++;
      break;
      
    case 6: // Lower Group B
      lowerLeg(1); lowerLeg(3); lowerLeg(5);
      gait_phase++;
      break;
      
    case 7: // Move Group A forward (push)
      moveLegForward(0, STEP_SIZE);
      moveLegForward(2, STEP_SIZE);
      moveLegForward(4, STEP_SIZE);
      gait_phase = 0; // Reset cycle
      break;
  }
}

void rotateLeft() {
  switch (gait_phase) {
    case 0: // Lift Group A
      liftLeg(0); liftLeg(2); liftLeg(4);
      gait_phase++;
      break;
      
    case 1: // Rotate Group A for left turn
      moveLegBackward(0, STEP_SIZE);  // Right front back
      moveLegBackward(2, STEP_SIZE);  // Right middle back
      moveLegBackward(4, STEP_SIZE);  // Right rear back
      gait_phase++;
      break;
      
    case 2: // Lower Group A
      lowerLeg(0); lowerLeg(2); lowerLeg(4);
      gait_phase++;
      break;
      
    case 3: // Lift Group B
      liftLeg(1); liftLeg(3); liftLeg(5);
      gait_phase++;
      break;
      
    case 4: // Rotate Group B for left turn
      moveLegForward(1, STEP_SIZE);   // Left front forward
      moveLegForward(3, STEP_SIZE);   // Left middle forward
      moveLegForward(5, STEP_SIZE);   // Left rear forward
      gait_phase++;
      break;
      
    case 5: // Lower Group B
      lowerLeg(1); lowerLeg(3); lowerLeg(5);
      gait_phase = 0; // Reset cycle
      break;
  }
}

void rotateRight() {
  switch (gait_phase) {
    case 0: // Lift Group A
      liftLeg(0); liftLeg(2); liftLeg(4);
      gait_phase++;
      break;
      
    case 1: // Rotate Group A for right turn
      moveLegForward(0, STEP_SIZE);   // Right front forward
      moveLegForward(2, STEP_SIZE);   // Right middle forward
      moveLegForward(4, STEP_SIZE);   // Right rear forward
      gait_phase++;
      break;
      
    case 2: // Lower Group A
      lowerLeg(0); lowerLeg(2); lowerLeg(4);
      gait_phase++;
      break;
      
    case 3: // Lift Group B
      liftLeg(1); liftLeg(3); liftLeg(5);
      gait_phase++;
      break;
      
    case 4: // Rotate Group B for right turn
      moveLegBackward(1, STEP_SIZE);  // Left front back
      moveLegBackward(3, STEP_SIZE);  // Left middle back
      moveLegBackward(5, STEP_SIZE);  // Left rear back
      gait_phase++;
      break;
      
    case 5: // Lower Group B
      lowerLeg(1); lowerLeg(3); lowerLeg(5);
      gait_phase = 0; // Reset cycle
      break;
  }
}

// ================= UTILITY FUNCTIONS =================

void checkBattery() {
  if (analogRead(BATTERY_PIN) < MIN_VOLTAGE) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

void processBluetoothCommand() {
  received_command = Bluetooth.read();
  
  switch (received_command) {
    case 0: // Stop and go to home position
      movement_mode = 0;
      gait_phase = 0;
      is_moving = false;
      setHomePosition();
      break;
      
    case 1: // Move forward
      movement_mode = 1;
      is_moving = true;
      break;
      
    case 2: // Move backward
      movement_mode = 2;
      is_moving = true;
      break;
      
    case 3: // Rotate right
      movement_mode = 3;
      is_moving = true;
      break;
      
    case 4: // Rotate left
      movement_mode = 4;
      is_moving = true;
      break;
      
    case 9: // Calibration mode
      calibrationMode();
      break;
      
    default:
      if (received_command >= 12) {
        // Speed control from slider
        movement_delay = map(received_command, 15, 100, 5, 80);
      }
      break;
  }
}

void executeMovement() {
  if (!is_moving || movement_mode == 0) {
    return;
  }
  
  switch (movement_mode) {
    case 1:
      moveForward();
      break;
    case 2:
      moveBackward();
      break;
    case 3:
      rotateRight();
      break;
    case 4:
      rotateLeft();
      break;
  }
}

void calibrationMode() {
  Serial.println("Entering calibration mode...");
  
  // Test each leg individually
  for (int leg = 0; leg < 6; leg++) {
    Serial.print("Testing Leg ");
    Serial.println(leg + 1);
    
    // Test range of motion
    for (int angle = 60; angle <= 120; angle += 10) {
      setLegPosition(leg, angle, 90, 90);
      delay(500);
    }
    
    // Return to home
    setLegPosition(leg, 90, 90, 90);
    delay(1000);
  }
  
  Serial.println("Calibration complete");
}

// ================= EMERGENCY FUNCTIONS =================

void emergencyStop() {
  movement_mode = 0;
  is_moving = false;
  gait_phase = 0;
  
  // Gradually return to home position
  setHomePosition();
  
  Serial.println("EMERGENCY STOP ACTIVATED");
}