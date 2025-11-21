// Automated Sentry Turret System Code
// --- DESCRIPTION ---
// This sketch controls an automated turret with a radar scanner (Ultrasonic Sensor 
// mounted on a Servo) and a separate gun system (Yaw Servo and Fire Servo).
// NEW REQUIREMENTS: 
// 1. Ultrasonic scan and fire (with buzzer tones).
// 2. IR detection triggers a separate buzzer alarm without stopping the servos.
// FIX: The IR alarm now runs concurrently with the scanning motors.

// Includes the Servo library
#include <Servo.h> 

// --- PIN DEFINITIONS ---
// Ultrasonic Sensor (Radar) Pins
const int trigPin = 11;
const int echoPin = 12;

// Proximity/Safety Sensor Pin
const int irSensorPin = 4; // IR Proximity Sensor for critical close-range detection

// Buzzer (Auditory Feedback) Pin
const int buzzerPin = 7; 

// Servo Control Pins
const int radarServoPin = 9;  // Radar Scanner Servo Pin
const int gunServoPin = 5;    // Gun Yaw/Horizontal Aim Servo Pin
const int fireServoPin = 6;   // Fire/Trigger Servo Pin

// --- STATE VARIABLES ---
long duration;
int distance;       // Distance in cm
bool hasShot = false; // Flag to prevent rapid, continuous firing.
bool irAlarmActive = false; // NEW: Flag to track if IR alarm is currently overriding sound

// Servo Objects
Servo radarServo;   
Servo gunServo;     
Servo fireServo;    

// --- FUNCTION PROTOTYPES ---
void handleIRAlarm();

// --- SETUP ---
void setup() {
  // Initialize communication and sensors
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT); // Initialize buzzer pin
  pinMode(irSensorPin, INPUT); // Initialize IR sensor pin
  
  Serial.begin(9600);
  Serial.println("--- Turret System Initialized ---");

  // Attach servos to pins
  radarServo.attach(radarServoPin);
  gunServo.attach(gunServoPin);
  fireServo.attach(fireServoPin);

  // Initial positions
  radarServo.write(20);   // Start scan left
  gunServo.write(90);     // Aim center (90 degrees)
  fireServo.write(100);   // Load/Safe position (assuming 100 is safe/pulled back)

  // --- BUZZER TEST SEQUENCE ---
  tone(buzzerPin, 1000, 100); 
  delay(150); 
  tone(buzzerPin, 500, 100);  
  delay(150);
  noTone(buzzerPin);
  // -----------------------------

  delay(1000); // Wait for servos to settle
}

// --- MAIN LOOP ---
void loop() {
  
  // 1. Handle IR Alarm Concurrently
  // This function is now blocking: if IR detects an object, it loops here 
  // until the object is gone, preventing any other code from running.
  handleIRAlarm(); 

  // 2. Scan from left (20) to right (160)
  for (int i = 20; i <= 160; i += 5) { 
    // Check IR again in case it was triggered during a scan step
    if (digitalRead(irSensorPin) == LOW) break; 
    
    radarServo.write(i);
    delay(40); // UPDATED: Radar Speed increased to 40ms delay per step.

    distance = calculateDistance(); 

    if (!hasShot && distance > 5 && distance < 30) {
      shoot(i);
    }

    // Output for radar plotting (e.g., Processing sketch)
    Serial.print(i);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(".");
  }

  // 3. Scan back from right (160) to left (20)
  for (int i = 160; i >= 20; i -= 5) {
    // Check IR again in case it was triggered during a scan step
    if (digitalRead(irSensorPin) == LOW) break; 

    radarServo.write(i);
    delay(40); // UPDATED: Radar Speed increased to 40ms delay per step.

    distance = calculateDistance(); 

    if (!hasShot && distance > 5 && distance < 30) {
      shoot(i);
    }

    // Output for radar plotting
    Serial.print(i);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(".");
  }
}

// FIX: This function now blocks the main loop, making the IR response immediate.
void handleIRAlarm() {
  // IR sensors typically read LOW when an object is detected.
  while (digitalRead(irSensorPin) == LOW) {
    // This loop executes repeatedly as long as the IR sensor is blocked.
    // It stops all other activity, including the servo movements.
    
    if (!irAlarmActive) {
      // Start the alarm tone only once
      noTone(buzzerPin); // Stop any previous tone (like a potential ultrasonic beep)
      tone(buzzerPin, 4000); 
      Serial.println("!!! IR ALARM ACTIVE: Obstacle Too Close !!!");
      irAlarmActive = true;
    }
    
    // Play the alarm for a very short duration and check the sensor again instantly
    // The lack of delay here means the loop checks the sensor constantly, 
    // giving a truly immediate, sustained sound and halting movement.
    
    // Optional: Add a short, non-blocking delay if the CPU usage is too high.
    // delay(5); 
  }
  
  // If we exit the while loop (object is gone):
  if (irAlarmActive) {
    noTone(buzzerPin);
    Serial.println("IR ALARM CLEAR.");
    irAlarmActive = false;
  }
}


// FIX: Ensures the ultrasonic reading is accurate by temporarily detaching 
// ONLY the radar servo from its timer control during the pulseIn() operation.
int calculateDistance() {
  
  // 1. DETACH the radar servo before reading
  radarServo.detach();
  
  // 2. Perform the reading (standard ultrasonic sequence)
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echoPin with a timeout
  duration = pulseIn(echoPin, HIGH, 30000); 
  
  // 3. ATTACH the radar servo back immediately
  radarServo.attach(radarServoPin);

  // 4. Calculate and return distance
  int calculatedDistance = duration * 0.034 / 2;
  
  if (calculatedDistance == 0 || duration == 0) {
    return 200; 
  }
  if (calculatedDistance < 5) {
    return 5;
  }

  return calculatedDistance;
}

// Aims the gun and fires, then reloads
void shoot(int radarAngleDeg) {
  Serial.println("\n--- TARGET LOCK ---");
  
  hasShot = true;
  
  // Stop any currently running IR alarm
  noTone(buzzerPin); 

  // Buzzer: Target Lock Tone (1500 Hz for 250ms)
  tone(buzzerPin, 1500, 250); 

  int launcherAngleDeg = radarAngleDeg;

  // Smoothly aim the gun to the target angle
  smoothAimTo(launcherAngleDeg);

  delay(200);

  // Fire!
  Serial.println("--- FIRING ---");
  noTone(buzzerPin); 
  // Buzzer: Firing Tone (300 Hz for 100ms)
  tone(buzzerPin, 300, 100); 
  
  fireServo.write(70);     // Fire/release position
  delay(500);              

  // Reload/Reset
  Serial.println("--- RELOADING ---");
  noTone(buzzerPin); 
  
  fireServo.write(100);   // Reload/Safe position
  
  delay(2000); // 2 seconds cool-down
  hasShot = false; 
  Serial.println("--- READY ---");
}

// Rotates the gun (Yaw) to the target angle smoothly
void smoothAimTo(int targetAngle) {
  int currentAngle = gunServo.read();

  targetAngle = constrain(targetAngle, 20, 160); 

  if (currentAngle < targetAngle) {
    for (int i = currentAngle; i <= targetAngle; i++) {
      gunServo.write(i);
      delay(5); 
    }
  } else {
    for (int i = currentAngle; i >= targetAngle; i--) {
      gunServo.write(i);
      delay(5); 
    }
  }
}
