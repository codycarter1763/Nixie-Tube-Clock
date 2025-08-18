# Nixie Tube Clock

# About
This is a build log on how I built a nixie tube clock from old surplus IN-14 and IN-3 nixie tubes, a microcontroller, and real time clock. 

## What Are Nixie Tubes?
Nixie tubes are a type of electronic device used for numerical displaying of letters and numbers. They were a precursor to LED displays first introduced as a useable product in the 1950's by David Hagelbarger. Nixie tubes are made up of a wire mesh anode connected to a high voltage DC source, a glass tube filled with neon and argon, and metal cathodes connected to metal pieces in the shape of a character. When the anode is powered and a cathode is grounded, a warm glow is emmited around the character. 

Nixie tubes were really popular because of their aesthetic, high contrast for good readibility, and really long lifespan. Even though LED displays outshine nixie tubes with lower power consumption, less complex driver circuts, and less operating and manufacturing costs, their steampunk look and history behind them add a vintage charm and one of a kind look to any project. 

<p align="left">
  <img src="https://github.com/user-attachments/assets/78a55e7a-1c02-4301-827f-7538575d066c" width="48%" height="48%" alt="Left Image">
  <img src="https://github.com/user-attachments/assets/135f465f-212f-4471-b1f1-bcb95fd68a47" width="48%" height="48%" alt="Right Image">
</p> 

# Design and Construction
## Design Considerations
### Power Supply
For the nixie tubes to illuminate, you need to supply the anode pin with a high DC voltage. For example, an IN-14 nixie tube requires 170 - 200 volts to light up. To accomplish this, you can use a DC-DC boost converter circuit that will efficiently step up a voltage to the required value. Once the voltage is stepped up, you need to limit the amount of current supplied the tubes to 2.5mA to prevent damage. So, in series with the anode, a resistor value can be calculated using this ohms law formula.

For this version of my nixie tube clock, I attempted to design a DC-DC converter for use with this build. Though, due to inefficiencies in the PCB version, the efficiency of the converter dips when loaded by tubes, creating a lot of heat. As a result, I sourced a pre-made version until I can work through the issues with my current design. 

### Driving The Nixie Tube
To light up the Nixie tube, you ground a pin pertaining to a certain character that will be displayed.

![image](https://github.com/user-attachments/assets/b0a6968c-ba08-451f-885d-e048f5deeb32)

Now that the tube lights up, to best way to control which pin is grounded is through the K155ID1 nixie tube driver IC which takes a binary coded decimal number and assigns it to the ten character pins of the nixie tube. Since there will be four IN-14 tubes for my clock, that would mean I would need sixteen I/O pins avaliable on my microcontroller. 

Since I will have more features and parts to control on the clock, I decided to use daisy-chained 74HC595 shift registers per two nixie tubes to change the amount of pins needed to control the tubes fron sixteen to just three.

#### Controlling IN-14 Tube
Assuming 180 volt supply, 145 volt IN-14 nixie tube sustain voltage, 2.5mA current.
Resistor IN-14 = (VSupply - VSustain) / 2.5mA = (180 - 145) / 2.5mA = 14kOhm

#### Controlling IN-3 Tube
Since the IN-3 tube will be used as a colon for the clock, I designed a simple BJT and MOSFET switch to be able to control this tube with the Arduino.

![image](https://github.com/user-attachments/assets/4a6406b8-1934-4892-a057-7e4af488c48a)

Assuming 180 volt supply, 75 volt IN-3 nixie tube sustain voltage, 0.8mA current.
Resistor IN-14 = (VSupply - VSustain) / 2.5mA = (180 - 75) / 0.8mA = 130kOhm

### RGB LEDs and IR Remote
For the build, I decided to add four RGB LEDs to the center of the nixie tubes to light them up different colors. I used the PCA9685 PWM IC as an RGB led driver to independently control the RGB LED colors. The IC is controlled via I2C with just two pins SDA and SCL.

To set a different design presets, I used a IR remote and reciever to accomplish this goal.

I wired the output pins to a header to be able to physically wire the LEDs though a hole under the nixie tubes. Since there was not a way to safely use surface mount LEDs and wire traces without increasing PCB layers and cost, I decided to go this route.

![image](https://github.com/user-attachments/assets/a9467ba5-9445-40fd-92eb-68f7c9fadb1c)

## Microcontroller and Real Time Clock (RTC)
I used an Arduino Pro Micro clone for the form factor and 5V logic. To keep the current time even when the clock is turned off, I decided to go with a RTC board that will connect via I2C to the arduino and tell what numbers to send out to the nixie tubes.

![image](https://github.com/user-attachments/assets/9d3566cc-8304-45ca-b49d-dbe9ee0c8051)

# Programming 
# PCB Construction
The PCB was fabricated with JLCPCB.
# Enclosure
# Closing
