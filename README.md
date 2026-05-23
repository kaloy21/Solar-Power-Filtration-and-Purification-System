# Solar-Power-Filtration-and-Purification-System

The Solar-Powered Water Filtration and Purification System is an automated, multi-stage water treatment solution that uses real-time sensor data to monitor, filter, and purify water. Controlled by an ESP32 microcontroller, the system autonomously manages water levels across multiple tanks and steps through rigorous quality-control phases—including mechanical filtration, pH balance checks, and ultraviolet (UV) sterilization—to ensure that the output water is completely safe for consumption.  

System Architecture & Components

The system coordinates water movement through an assembly of electronic sensors, switches, and relays:

1. Water Quality
  - SensorsAnalog pH Sensor: Continuously monitors the acidity or alkalinity of the water, ensuring it stays within a safe, neutral range ($6.5 \text{ to } 8.5$).
  - Total Dissolved Solids (TDS) Sensor: Measures the concentration of dissolved substances in parts per million (PPM) to track baseline purity.
  - Turbidity Sensor: Evaluates water clarity by analyzing light scattering. It classifies water dynamically into CLEAR, CLOUDY, or DIRTY statuses.

2. Physical Automation & Containment
   - Tank 1 (Raw/Input Tank): Equipped with high/low float switches to manage initial filling.
   - Tank 2 (Post-Filter Tank): Receives water after it passes filtration tests.
   - Clean Water Reservoir: Holds the final product, guarded by a dedicated float switch to prevent overflow.
   - Relay Module Matrix: Dynamically switches power to four separate fluid pumps, automated flow valves, and an industrial UV sterilization lamp.
  
Step-by-Step Operational Workflow
The system operates as a finite state machine, executing a strict sequence of safety protocols:

[FILL TANK 1] ➔ [STABILIZE SENSORS] ➔ [MONITOR QUALITY] ➔ (If Unsafe) ➔ [REFILTER]
                                             │
                                         (If Safe)
                                             ▼
[STANDBY]     ◄─── [RESERVOIR FILL] ◄─── [UV STERILIZE] ◄─── [TRANSFER TANK 2]

Stage 1: Intake & Filling (FILL_TANK1)
The system opens an electronic inlet valve and activates Pump 1 to draw raw water into Tank 1. It fills until the upper float switch detects that the tank is full, shutting off the intake to prevent overflow.

Stage 2: Calibration & Settling (STABILIZE)
To prevent false sensor readings caused by sloshing or air bubbles, the system pauses for 15 seconds (STABLE_TIME). This allows the water to settle across the pH, TDS, and turbidity probes.

Stage 3: Real-Time Quality Check (MONITOR_QUALITY)
The micro-controller pulls data from the sensors using custom smoothing algorithms (such as a 30-sample median filter for TDS to strip away signal noise). The water is flagged as UNSAFE if:
 - The TDS exceeds 600 PPM.
 - The Turbidity percentage reaches or exceeds 50%.
 - The pH drops below 6.5 or rises above 8.5.

Stage 4: Closed-Loop Recirculation (REFILTER)
If the water fails the quality test, the system diverts it into a filtration loop. Pump 2 and Valve 2 run continuously, cycling the water through physical filters until the sensors report that the parameters have dropped back into safe thresholds.

Stage 5: Transfer (TRANSFER_TANK2)
Once verified safe, Pump 3 activates to shift the treated water from Tank 1 into Tank 2.  

Stage 6: Ultraviolet Sterilization (UV_STERILIZE)
To eliminate biological pathogens, viruses, and bacteria, an ultraviolet (UV) lamp is activated inside Tank 2. The system institutes a brief delay to let the lamp reach full power, then bathes the water in UV light for a strict 20-minute cycle (UV_TIME).

Stage 7: Clean Reservoir Distribution (RESERVOIR_FILL)
Pump 4 engages to pull sterile water from Tank 2 into the final Clean Water Reservoir for end-user distribution.

Stage 8: Intelligent Standby (STANDBY)
With all tanks serviced, the system shuts down all pumps to conserve solar battery power. It constantly monitors the float switches ; if the final reservoir runs low, it automatically restarts the transfer and purification cycle.  

User Interface & Debugging Features
- Integrated 20x4 I2C LCD: Provides physical onsite updates, tracking current pH levels, precise TDS PPM, Turbidity percentages (NTU equivalents), and the active system state.
- Telemetry Serial Stream: Streams continuous operational logs over serial communication (115200 baud) for remote diagnostics and debugging..
- In-System Calibration: Features an interrupt sequence allowing engineers to calibrate the pH probe in real time using the Serial Monitor without needing to rewrite the base firmware.


