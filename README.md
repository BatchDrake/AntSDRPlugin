# AntSDRPlugin
_**WARNING:** This is a WIP and also a hack. I will be using this to debug my poor man interferometer for the [forthcoming Perseids meteor shower](https://en.wikipedia.org/wiki/Perseids)._

This is ANTSDR 2RX hack plugin for SigDigger, for phase detection. It implements:

1. A new signal source named "Pluto/ANTSDR 2RX combined source"
2. A tool widget named "Phase comparison tool"

## Requirements
* AntSDR / Pluto with dual RX and 2r2t enabled.
* Latest SigDigger headers from `develop`. These are installed in your system when you build SigDigger from the command line and run `sudo make install`.
* [libiio](https://github.com/analogdevicesinc/libiio) and associated development files
* [libad9361](https://github.com/analogdevicesinc/libad9361-iio) and associated development files

## Usage
1. Select the "Pluto/ANTSDR 2RX combined source as input source", and specify the IP at which it can be found (by default, 192.168.1.10 if you are using the Ethernet interface, 192.168.2.1 if you are using the USB interface)
  ![](doc/step1.png)

2. Start the capture. You will see the spectrum doubled around the center frequency. The one in the left comes from RX1. The one in the right, from RX2. Select either one of the doubled channels. This is the channel whose phase we are going to measure.
   ![](doc/step3.png)
   
3. In the side panel, scroll down to the Phase Comparison tool, and click **Open**. A new tab containing a colored plot will show up.
   ![](doc/step2.png)
   
4. In the phase comparison plot, the amplitude is proportional to the power of the signal in both channels and the color represents the phase difference between both channels, according to the [YIQ color wheel](doc/yiq.png).
   ![](doc/step4.png)
   
## TODO
* The color interface sucks. Add some kind of legend and/or a tool tip text to display the exact phase.
* Add a vector representation
