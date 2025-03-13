Research notes on sleeping the Arduino and Raspi: 

The RasPi does not have a timed sleep mode, but the Arduino does. Currently exploring: controlling PI wake/sleep mode via the Arduino, which is set to turn on every X minutes. 

On the Pi, it isn’t possible to run anything in its low power mode, since it is basically equivalent to be completely off.

**The following is relevant for more advanced Arduino models, not the ones we are using.**

To manage power consumption in the Arduino, the main/most effective method is putting the Arduino in sleep mode. In sleep mode, no code can be run. Sleep can be interrupted, however, by the following methods:

- **Deep Sleep**: the Arduino can be set to wake up after a set time. This is most power efficient, since it is put in Deep Sleep mode, which uses the least possible amount of power. In this mode, there are very limited possibilities of running peripherals. “This will stop every clock sources of the microcontroller and set the voltage regulators to be in low power state. Oscillators can be in 3 different states where it stops or run, and run on behalf of peripheral request. The device will be then in deep sleep while WFI (Wait For Interrupt) is active.”
- (Non-Deep) Sleep: (Idle Mode): Continue running peripherals. In this mode, it’s possible to wake up based on set external events, including by a change in voltage from a connected device.
- Unless it is necessary to react to external changes independent from time, it seems that Deep Sleep/waking based on times is appropriate here.

**For our Arduino Uno R3s:**

- Can only sleep in 8s intervals! See below
- https://github.com/rocketscream/Low-Power/issues/98
- The official ArduinoLowPower library does not work, instead, https://github.com/rocketscream/Low-Power seems to work well. Find it in the Arduino IDE as Low-Power by Rocket Scream Electronics
- How efficient this is (sleeping in 8s intervals) is unknown—todo test power draw.

Advanced Linux modifications (from Nick): 

- Software won’t start running again, and will be at standby.
- Might have to reboot software tasks after the RasPi switches back on. Bash script to start processes?
- Kill processes in Linux to shut down specific processes.

https://littlebirdelectronics.com.au/blogs/news/how-can-i-sleep-a-raspberry-pi-and-wake-it-again-with-an-interrupt?srsltid=AfmBOop3bE5hew3rB6RGzLl3DPPBmlCIu-EkME9XCSxDgSa9bP72l3IG

https://docs.arduino.cc/learn/electronics/low-power/