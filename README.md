# Nixie Tube Clock


<p align="left">
  <img src="https://github.com/user-attachments/assets/3f882592-0aea-41ba-a384-71dc7522f895" width="48%" height="48%" alt="Left Image">
  <img src="https://github.com/user-attachments/assets/479a62dd-9854-480c-90de-3277f28ad016" width="48%" height="48%" alt="Right Image">
</p> 

<p align="left">
  <img src="https://github.com/user-attachments/assets/11b8388b-a713-47b5-8416-5cd5ddfa48f3" width="48%" height="48%" alt="Left Image">
  <img src="https://github.com/user-attachments/assets/1d5745ea-2e5c-4783-b51c-6b7e24844d45" width="48%" height="48%" alt="Right Image">
</p> 


# About
This is a build log on how I built a nixie tube clock from old surplus IN-14 and IN-3 nixie tubes, a microcontroller, and real time clock. 

Features of my clock include:
- Real-time clock with battery backup
- Daylight Saving Time (DST) adjustment
- IR remote control for clock functions and settings
- Quick toggling between time and date display
- RGB animations with adjustable brightness
- Nixie tube cathode poisoning prevention routines for long tube life

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

<img width="691" height="893" alt="Screenshot 2025-08-21 115333" src="https://github.com/user-attachments/assets/f4b5ef06-91e0-453c-8979-8863cb35133b" />

### Shift Register Basics
A shift register is a series of D flip-flops where the output of one flip-flop is connected to the input of another. The **Serial** pin takes in multiple bits of data and stores on the shift registers in blue. For example, if you want to store the number 1001 1011 as is on the register, each clock cycle will take a bit in through **Serial**. So after one cycle, SR1 will have bit 1, when the next clock cycle hits, serial will take in a 0 and shift SR1's 1 to SR2, so on and so forth until all bits are loaded.

When you want to store the inputted data for use, sending a pulse to the RCLK will latch the data and store it in **Storage Register** in purple. In this case, the SR stage in blue can keep shifting data without affecting the **Storage Register** in purple.

To show the storage register data, setting OE to 0 on the tri-state buffers will output the stored data.

Combining software that inputs in data pertaining to what number needs to be shown on a specific display and handles the shifting logic makes for an abstract form for easy programming and visualizing.

<img width="711" height="834" alt="Screen" src="https://github.com/user-attachments/assets/71837c6e-e6a2-48c7-9cc3-0da95ea226d2" />

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
## Real-Time-Clock
Using a real-time-clock allows for precise time keeping even when the power is turned off. Once the current time is programmed in, I assigned the minutes and hours to variables and accounted for 12 hour roll-over and rollover to display on two tubes. 

<img width="711" height="368" alt="image" src="https://github.com/user-attachments/assets/d4d64632-414e-4d2e-ab26-782572a008fd" />

## Display Logic
Once the time is inputed into the array in the form 1023 for 10:23, it gets sent to the displayDigit fucntion where it takes 4 digits, packs them into 2 bytes, shifts them into the registers, and latches them to display on nixie tubes as shown earlier.

<img width="720" height="545" alt="image" src="https://github.com/user-attachments/assets/ffb68b7c-ad58-4178-b2a5-b9246ab344c7" />

### Accounting For Tube Longevity With Cathode Poisoning Prevention
Common in nixie tubes that are left on a certain digit for several days or weeks, unused cathode digits accumulate a thin insulating layer of compounds that make it harder for the tube to light up. 
<img width="567" height="265" alt="image" src="https://github.com/user-attachments/assets/66099522-5001-4816-b1f6-da3644284318" />

As a result, I added a slot machine type poisoning prevention where all the digits will cycle for a certain amount of time and eventually stop on the current time from right to left like a slot machine. This is done every four hours with a longer cycle done at 3AM for added tube longevity.

## IR Input
Using an IR remote and reciever, you can assign certain functions to happen when pressed by a specific button. The way this works is when a button is pressed on the IR remote, a string of binary numbers are sent out and recieved. Converting this long string to HEX values makes defining to variables very useful and abstract. 

<img width="492" height="440" alt="image" src="https://github.com/user-attachments/assets/5d60f7d5-1d35-4b6f-9379-a74b1ded68da" />

## RGB Presets
Using the PCA9685 PWM driver IC and four non-addressable RGB LEDs allowed me to program some vibrant color schemes. Using I2C and setPWM functions, you can control each channel of an RGB LED with varying duty cycle for custom colors from 0 to 4095.

To make things easier to program, I added abstraction to distinguish what PCA outputs pertained to which channel of each RGB LED. 
<img width="358" height="198" alt="image" src="https://github.com/user-attachments/assets/5dc10f1a-8d31-4230-ab02-c1e00ae4618b" />

For example, to turn all LEDs red, where the syntax of the abtraction is **rgbChannels[which LED][which channel, where R = 0, G = 1, B = 2], (when to start on PWM cycle), RGB_Brightness)**
<img width="543" height="211" alt="image" src="https://github.com/user-attachments/assets/5c0f4032-1512-4429-a3fd-9f20cb84b250" />

### RGB to PWM Conversion
For certain effects, instead of having to manually scale an RGB color code from 256 to 4096, I implemented a function to linearly map the values. 
<img width="875" height="161" alt="image" src="https://github.com/user-attachments/assets/a5cacf1f-93c6-4266-bae4-c436a1f468d9" />

### Accounting for the Logarithmic Response of the Eyes
For certain colors, they can appear washed out or incorrect. But using a gamma correction formula, RGB (0 - 256) to PWM (0 - 4096) can appear correctly on the RGB LEDs. As shown below, you can see a comparison of linear mapping vs the corrected gamma mapping for accurate colors.

<img width="557" height="188" alt="image" src="https://github.com/user-attachments/assets/0e00001f-b317-469e-a0e0-988504329f34" />

<img width="860" height="570" alt="image" src="https://github.com/user-attachments/assets/5118a0cf-d7fb-4052-b1f2-5f6997b36b87" />

## Non-Blocking Functions
To allow other clock functions and RGB presets to keep operating at the same time without an OS or threading, I used non blocking logic. 

For example, looking at the logic for the LSU preset animation, lastLSUUpdate stores the time when the LEDs were last updated, lsuDelay and lsuStep affects animation speed. now starts counting up in ms ans stores that value, then is checked with the if statement that sees if enough time has passed. Essentially the code will run for a short period of time and exit. 

In the loop, I have toggle logic that will check whether another button has been pressed. If not, then LSU() will run again and the cycle continues. Since this is all done super fast, there is no blinking that can be seen through any preset. 
<img width="683" height="294" alt="image" src="https://github.com/user-attachments/assets/88c82c33-e1a2-4f35-a1dc-cf1c008140ac" />

# PCB Construction
My goal was to have most of the components on a single board so that I didnt have to run a lot of wires. At first, I did design a high voltage nixie tube supply for use with this project, but due to efficiency issues when loaded the deisgn did not work correctly. So I just sourced a premade version for now until I can work the issues out. 

The PCB was fabricted by JLCPCB and came out very nice!
![IMG_5490](https://github.com/user-attachments/assets/20829da0-3362-4230-b63d-d4e7af7e30ae)

# Closing
Hope you learned something from this repository and may my example help with inspiration for your project!
