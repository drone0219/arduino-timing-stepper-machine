# arduino-stepper-timing
Smooth sequence control logic for stepper motors 

## Wiring 
![alt text](wiring.png)

## Overview

This is an Arduino-based precision parts feeder/positioning system designed for light-duty sequential manufacturing operations. The system uses a 300 LPI linear optical encoder for high-precision quadrature feedback (4X resolution) combined with a NEMA23 stepper motor controlled by an L298N H-Bridge driver.

## Program Logic

### Architecture
The program implements a robust control architecture using two key design patterns:

- **Finite State Machine (FSM)**: Manages the overall system states and transitions
- **Function Block Diagram (FBD)**: Handles input debouncing and edge detection for reliable signal processing

### State Machine
The system operates in two primary states:

#### 1. **IDLE State** (`idle`)
- **Entry Actions**:
  - Disables motor drivers (sets ENA and ENB to 0)
  - Stops all motor movement
  - LED turns ON to indicate ready status
- **During State**:
  - Waits for remote button press
  - Part is held in position (mechanically or by detent torque)
- **Exit Condition**:
  - Remote button trigger detected → transitions to MOVING STAGE

#### 2. **MOVING STAGE State** (`movingStage`)
- **Entry Actions**:
  - Resets encoder count to 0
  - Enables motor drivers (PWM 200/255 ≈ 78% power)
  - Displays current stage information via serial
  - LED turns OFF to indicate motion
- **During State**:
  - Continuously steps motor (forward or reverse based on target)
  - Monitors encoder feedback in real-time
  - Compares actual position vs. target with ±2 count tolerance
- **Exit Conditions**:
  - **Target reached** → advances to next stage, transitions to IDLE
  - **Start edge switch triggered** → resets to Stage 0, transitions to IDLE
  - **End edge switch triggered** → jumps to final stage, transitions to IDLE

### Signal Processing (FBD Functions)
All inputs use sophisticated debouncing and edge detection:

1. **TON (Timer On Delay)**: 100ms debounce filter for all inputs
   - Prevents false triggers from switch bounce
   - Ensures stable signal detection
   
2. **Rtrg (Rising Edge Trigger)**: Detects positive edge transitions
   - Button press generates single pulse
   - Prevents continuous triggering while held

This combination ensures each button press or switch closure is recognized exactly once, regardless of contact quality or duration.

### Stage Controller
The `StepController` class manages the sequential workflow:
- Tracks current stage number (0-5)
- Maintains start and stop encoder counts for each stage
- Determines direction (forward/reverse) based on target
- Provides methods to navigate stages:
  - `nextStage()`: Advances to next stage (wraps to 0 after stage 5)
  - `gotoStart()`: Emergency reset to beginning
  - `gotoEnd()`: Jump to final return stage

### Encoder Feedback Loop
- **Quadrature Decoding**: 300 LPI encoder with 4X resolution = 1200 counts/inch
- **Position Control**: Closed-loop system compares encoder counts to target
- **Tolerance Window**: ±2 count acceptance band for arrival detection
- **Interrupt-Driven**: Hardware interrupts ensure no encoder pulses are missed

## Features

### Core Features
- **6-Stage Sequential Operation**: Configurable multi-stop positioning system
- **Closed-Loop Control**: Encoder feedback ensures accurate positioning regardless of load or slippage
- **Bidirectional Motion**: Automatically determines optimal direction for each stage
- **Position Tolerance**: ±2 encoder count acceptance window for reliable stopping

### Safety Features
- **Edge Limit Switches**: Dual micro-switches prevent mechanical overtravel
  - Start edge: Resets system to beginning
  - End edge: Triggers final return sequence
- **Signal Debouncing**: 100ms software debounce on all inputs eliminates noise
- **Edge Detection**: Single-shot triggering prevents unintended repeat commands
- **Motor Disable**: Motors power down in IDLE state to reduce heat and power consumption

### User Interface
- **Remote LED Indicator**:
  - ON = System IDLE, part secured, ready for work
  - OFF = System in motion, stand clear
- **Remote Momentary Button**: Single-button operation advances through stages
- **Serial Monitoring**: Real-time status display at 9600 baud
  - Current stage number
  - Start and stop counts
  - Direction of travel
  - Position arrival notifications

### Configurability
- **Easy Stage Addition**: Add/remove stages by modifying `stages[]` array and `STAGECNT`
- **Adjustable Parameters**:
  - Motor speed (currently 60 RPM)
  - Motor power level (ENA/ENB PWM values)
  - Debounce timing (100ms default)
  - Position tolerance (±2 counts)
- **Scalable Architecture**: Stage structure supports additional parameters (speed per stage, acceleration profiles, etc.) 

## Stage Sequences

The system executes 6 predefined sequential stages with the following positions:

| Stage | Description | Start Position | End Position | Encoder Counts | Travel Distance |
|-------|-------------|----------------|--------------|----------------|-----------------|
| 0 | Initial advance | 0" | 2" | 0 → 600 | 2" forward |
| 1 | Major advance | 2" | 24" | 600 → 7200 | 22" forward |
| 2 | Continue forward | 24" | 48" | 7200 → 14400 | 24" forward |
| 3 | Continue forward | 48" | 72" | 14400 → 21600 | 24" forward |
| 4 | Final position | 72" | 94" | 21600 → 28288 | ~22" forward |
| 5 | Return home | 94" | 0" | 28288 → 0 | 94" reverse |

### Position Calculations
- **Encoder Resolution**: 300 LPI × 4 (quadrature) = 1200 counts/inch
- **GT2 Belt**: 2mm pitch × 20 teeth = 40mm (1.575") per pulley revolution
- **Motor**: 200 steps/rev (1.8° per step)

### Sequence Operation
1. System powers on in Stage 0, IDLE state
2. Operator presses remote button → moves to Stage 0 position (2")
3. LED turns ON when position reached, motor disables
4. Operator performs work, presses button when complete
5. Process repeats through stages 1-4
6. Stage 5 automatically returns carriage to home position (0")
7. Cycle can repeat indefinitely

### Customization
Stages can be easily modified in the `stages[]` array:
```cpp
static Stage stages[] = {
    {startCount, stopCount},  // Stage 0
    {startCount, stopCount},  // Stage 1
    // Add more stages as needed
};
```
Update `STAGECNT` to match the number of stages defined.


## Hardware Components

### Electronics
- **Arduino Uno**: Main controller (ATmega328P)
- **Stepper Motor**: NEMA23, 425oz-in (3Nm), 1.4A/phase (series wiring)
- **Motor Driver**: L298N Dual H-Bridge, supports up to 2A per channel
- **Optical Encoder**: HEDS-9731-252, 300 LPI dual photo-interrupter
- **Encoder Film**: 300 LPI linear scale

### User Interface
- **Push Button**: Momentary switch (normally open), remote mounted
- **Status LED**: Remote mounted indicator

### Mechanical
- **Belt**: GT2 timing belt (2mm pitch)
- **Pulley**: 20-tooth GT2 pulley (1.575" circumference)
- **Optional**: Mechanical clamp/brake for additional holding force (if motor detent insufficient)

### Pin Assignments
```
Motor Control:
  - ENA (Pin 5): Motor driver enable A (PWM)
  - ENB (Pin 6): Motor driver enable B (PWM)
  - Pins 8-11: Stepper coil control

Encoder:
  - CHNA (Pin 2): Encoder channel A (interrupt)
  - CHNB (Pin 3): Encoder channel B (interrupt)

User Interface:
  - REMOTELED (Pin A0): Status LED output
  - REMOTEBUTTON (Pin 4): Momentary switch input (pull-up)

Limit Switches:
  - STARTEDGE (Pin A1): Start position micro-switch (pull-up)
  - ENDEDGE (Pin A2): End position micro-switch (pull-up)
```

## Software Dependencies

The program uses custom libraries included in the project:
- `Encoder.h/.cpp`: Quadrature encoder decoder with interrupt support
- `FiniteStateMachine.h/.cpp`: Lightweight FSM implementation
- `FBD.h/.cpp`: Function block diagram elements (TON, Rtrg, etc.)

Standard Arduino libraries:
- `Stepper.h`: Arduino built-in stepper motor control

## Installation & Usage

### Upload Instructions
1. Open `arduino-stepper-timing.ino` in Arduino IDE
2. Select board: **Arduino Uno**
3. Select appropriate COM port
4. Upload sketch to Arduino

### Operation
1. Power on system - LED should turn ON (IDLE state)
2. Verify serial monitor at 9600 baud for status messages
3. Press remote button to initiate first movement
4. System moves to first stage position, LED turns ON
5. Perform work at each stage
6. Press button to advance to next stage
7. Final stage returns to home position automatically

### Troubleshooting
- **System won't move**: Check motor power connections and ENA/ENB signals
- **Inaccurate positioning**: Verify encoder wiring and film alignment
- **Button not responding**: Check serial monitor for trigger messages, verify 100ms hold time
- **Motor stuttering**: Check power supply capacity (motor draws up to 2.8A)

