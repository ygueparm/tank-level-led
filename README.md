🔧 How It Works


<img src="kicad3D.png" width="400" alt="3D de l'afficheur led">

<img src="nano_mini.png" width="400" alt="arduino nano mini">

This system monitors and visualizes the fill level of an underground fuel/oil tank using:

    A 0–1 bar pressure sensor (measuring static head pressure),
    An Arduino Nano for signal processing and control,
    A 10-LED bar for intuitive visual feedback,
    A solid-state relay (SSR) to automate a pump based on level thresholds.

📊 Calibration & Real-World Parameters

All calculations are based on empirical measurements (not just theoretical formulas), making the system robust to sensor drift or installation-specific conditions:
Parameter
	
Value
Empty tank
	
0.00 bar (4.28 mA)
Full tank
	
0.21 bar (7.46 mA)
Pump shut-off (dynamic)
	
0.43 bar (prevents overfill during pump operation)
Pump start threshold
	
≤15% level
Display update interval
	
5 seconds (via millis() — non-blocking)
Smoothing
	
10-sample moving average on analog readings
🌟 Key Features

    ✅ Startup LED test — visual confirmation of all LEDs and level simulation  
    📈 Progressive LED bar — linear mapping of pressure → fill level (0 to 10 LEDs = 0% to 100%)  
    ⚠️ Critical-level alert — first LED blinks when level ≤10% (low fuel warning)  
    🔄 Hysteresis-based pump control — avoids rapid ON/OFF cycling:  
        Pump starts when level ≤15%  
        Pump stops when dynamic pressure ≥ 0.43 bar (measured during pumping)
    📋 Live serial dashboard — ASCII bar graph + status in real time:


Pression | % Remplissage | Barre LEDs | Pompe
---------|---------------|------------|------
0.092 bar | 43.8% | ████░░░░░░ | Niveau 4 | OFF
0.018 bar | 8.6%  | █░░░░░░░░░ | VIDE    | ON   ← pump just started!
>>> POMPE DÉMARRÉE (niveau bas) <<<

📐 Wiring Hint

    Sensor → analogPin (via shunt resistor for 4–20 mA loop)  
    LEDs → digital pins (simple HIGH/LOW, no PWM needed)  
    SSR control → pinSSR → triggers external pump contactor

    🔧 Note: The linear conversion convertirPressionReelle() uses real current-to-pressure calibration points — easily adaptable to other sensors.
