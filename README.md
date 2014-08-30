PC04 Reader / Punch control
==========

This project has been extended to be able to both control the reader and punch of the DEC PC04. The plan is to manufacture a DEC dual card that inserts into the PC04 and provide a serial interface, both 20 mA current loop and RS-232 levels to both be able to interface with the standard DL11 and modernd DLV11-J.

The intreface is one rx signal for the punch section running at 500 bps and a tx signal which is running at 3000 bps. The DL11 has to be modified using a different crystal to support this. There is also a reader run signal that starts the reader. Reader run is a pulse that signals that one character is to be read.

The intention is to use a AtMega1284p chip.

                           +---\/---+
                (D 0) PB0 1|        |40 PA0 (AI 0 / D24)
                (D 1) PB1 2|        |39 PA1 (AI 1 / D25)
           INT2 (D 2) PB2 3|        |38 PA2 (AI 2 / D26)
            PWM (D 3) PB3 4|        |37 PA3 (AI 3 / D27)
         PWM/SS (D 4) PB4 5|        |36 PA4 (AI 4 / D28)
           MOSI (D 5) PB5 6|        |35 PA5 (AI 5 / D29)
       PWM/MISO (D 6) PB6 7|        |34 PA6 (AI 6 / D30)
        PWM/SCK (D 7) PB7 8|        |33 PA7 (AI 7 / D31)
                      RST 9|        |32 AREF
                     VCC 10|        |31 GND
                     GND 11|        |30 AVCC
                   XTAL2 12|        |29 PC7 (D 23)
                   XTAL1 13|        |28 PC6 (D 22)
           RX0 (D 8) PD0 14|        |27 PC5 (D 21) TDI
           TX0 (D 9) PD1 15|        |26 PC4 (D 20) TDO
     RX1/INT0 (D 10) PD2 16|        |25 PC3 (D 19) TMS
     TX1/INT1 (D 11) PD3 17|        |24 PC2 (D 18) TCK
          PWM (D 12) PD4 18|        |23 PC1 (D 17) SDA
          PWM (D 13) PD5 19|        |22 PC0 (D 16) SCL
          PWM (D 14) PD6 20|        |21 PD7 (D 15) PWM
                           +--------+


The reader cable is connected as follows

  
     1284p pin         Use                      Direction     PC04 Reader Connector
     ------------------------------------------------------------------------------
     1                 Stepper Motor Coil A(0)     Out               P
     2                 Stepper Motor Coil A(1)     Out               R
     4                 Stepper Motor Coil B(0)     Out               S
     5                 Stepper Motor Coil B(1)     Out               T
     3                 Feed hole detector          In                N

     6                 Stepper power enable        Out               U
     7                 Feed switch                 In                V
     
    40                 Hole 1 detector             In                D
    39                 Hole 2 detector             In                E
    38                 Hole 3 detector             In                F 
    37                 Hole 4 detector             In                H
    36                 Hole 5 detector             In                J
    35                 Hole 6 detector             In                K
    34                 Hole 7 detector             In                L
    33                 Hole 8 detector             In                M
 
                       Ground                      GND               C


Punch cable

    1284p pin         Use                      Direction      PC04 Punch Connector
    ------------------------------------------------------------------------------
     8                Punch Feed Switch            In                A
                      GND                                            B
    16                Punch Sync                   In                C  - need leveling circuit see below
                      GND                                            D
    18                Punch Done                  Out                E
    29                Hole 8                      Out                H
    28                Hole 7                      Out                J
    27                Hole 6                      Out                K
    26                Hole 5                      Out                L
                                                                     E  - bias to punch sync in reader.
    25                Hole 4                      Out                N
    24                Hole 3                      Out                P
    23                Hole 2                      Out                R
    22                Hole 1                      Out                S
    19                Out of tape                 In                 T
    

The original PC8E uses a M840 board to let the computer control the punch. This uses this circuit to bias the punch sync coil in the punch.

![Punch sync bias circuit](https://dl.dropboxusercontent.com/u/96935524/Datormusuem/Sk%C3%A4rmavbild%202014-07-18%20kl.%2007.55.17.png "Title") 



PC04 Reader is a utility program to read paper tapes on a DEC PC04 Paper tape reader.

The hardware consists of an Arduino Uno card, but I guess that any Arduino board would work fine.

There is a also software for the host to read in the information read by the arduino board.

### PC04 


![PC04 schematic](http://i.imgur.com/7pdr3M1.png "PC04 schematic")

M044 solenoid driver

![M044 soleniod driver](http://i.imgur.com/RlRk2i8.png "M044 soleniod driver")

The solenoid driver drive the punch solenoids when the inputs are high.  Thus to make sure taht no punch are active we need to pull down these inputs since the AtMega chip input floats to a high impedance state when configured as inputs which is the default at startup. The M044 driver card uses SN7401 chip which need at most 1.6 mA out of the chip to detect a low level. High level on the other hand is detected if 40 uA is feed into the chip input. To make sure low is detected at startup of the AtMega chip a rework is required to add pull downs on all outputs. 560 ohm is selcted a pull down. A punch solenoid is activated for 10 ms so the average powerconsumption should be quite low anyhow.

[Webpage](http://www.datormuseum.se/reading-paper-tapes)

### DL11

The [DL11](https://dl.dropboxusercontent.com/u/96935524/Datormusuem/DL11%20Asynchronous%20Line%20Interface%20Engineering%20Drawings.pdf) Asynchronous Line interface is implemented by DEC on a M7800 circuit board. It is using a standar UART circuit and a number of circuits nu adapt to the unibus and to both EIA RS-232 / CCITT V.28 levels and 20 mA current loop.

The DL11 has a baud rate generator that can generate independet clock signals for the Rx and Tx section of the UART chip. 8 different buad rates are selectable using two rotary switches. The crystal to teh baud rate generator can be replaced. 

![Baud rates](http://i.imgur.com/zpG0JzT.png "Baud rates")

The problem is that the speed of the punch is dependent on line frequency and the ratio between the pulleys. If the speed is lower than the rate of the incoming serial line then we eventually would have a buffer overflow. If it is to fast we will have situation where the punch skips to punch for a revolution of the punch pulley. If we assume that the punch speed is exactly 50 cps then a 500 bps baud rate. For the reader part it is just necessary that baud rate is above the 300 cps of the reader, thus more than 3000 bps.




| Divisor | 844.8 kHz      | 1032.96 kHz     | 1.152MHz | 4.608MHz | 11.52MHz | 7.68MHz     | 3.84MHz     | 1.92MHz     | 8MHz     | 3.93216MHz     | 2MHz     |
|---------|-------------|-------------|---------|---------|----------|-------------|-------------|-------------|-------------|-------------|-------------|
| 23040   | 36,66666667 | 44,83333333 | 50      | 200     | 500      | 333,3333333 | 166,6666667 | 83,33333333 | 347,2222222 | 170,6666667 | 86,80555556 |
| 15360   | 55          | 67,25       | 75      | 300     | 750      | 500         | 250         | 125         | 520,8333333 | 256         | 130,2083333 |
| 7680    | 110         | 134,5       | 150     | 600     | 1500     | 1000        | 500         | 250         | 1041,666667 | 512         | 260,4166667 |
| 3840    | 220         | 269         | 300     | 1200    | 3000     | 2000        | 1000        | 500         | 2083,333333 | 1024        | 520,8333333 |
| 1920    | 440         | 538         | 600     | 2400    | 6000     | 4000        | 2000        | 1000        | 4166,666667 | 2048        | 1041,666667 |
| 960     | 880         | 1076        | 1200    | 4800    | 12000    | 8000        | 4000        | 2000        | 8333,333333 | 4096        | 2083,333333 |
| 640     | 1320        | 1614        | 1800    | 7200    | 18000    | 12000       | 6000        | 3000        | 12500       | 6144        | 3125        |
| 480     | 1760        | 2152        | 2400    | 9600    | 24000    | 16000       | 8000        | 4000        | 16666,66667 | 8192        | 4166,666667 |

The four first columns are the original crystals as per the DL11 engineering drawings. Then there are a number of columns for recommended crystals that would give a 500 cps rate. The problem, though is that 1.92MHz and 3.84MHz doesn't seem to be standard crystals. 7.68MHz and 11.52 Mhz jsut might be to high for the DL11 baud rate geberator circuit. This need to be tested. Then there are three columns with crystals that generate cps rates that are slightly higher than 50 cps. 512 bps givs a 2.4% higher rate. If the buffer is 8192 bytes it will take 241333 bytes until there will be a buffer overflow in the punch software.

The Reader Run signal on the DL11 is used with 110 baud teletypes to engage the reader relay and feed it one step forward. The Teletype is connected over current loop which means that the Reader Run signal is only available on 20 mA current loop interface on the DL11.

This is the current loop Rx circuit from the DEC DL11 Async card. The design is aimed to interface towards this ciruit.
Since this Rx circuit is aimed for 110 bps mechanincal relay switching it has to be modified when running at 3000 bps. All the capacitors has to be removed so that the signals is not low pass filtered to much.

![Current Loop Rx Circuit](http://i.imgur.com/KvVhtoU.png "Current Loop Rx Circuit")

And this is the Tx circuit of the DL11 card.

![Current Loop Tx Circuit](http://i.imgur.com/tKE7BaE.png "Current Loop Tx Circuit")

### Gerber files

![PCB layout](http://i.imgur.com/TbeRUjJ.png "PCB layout")

This is the layout I did fo this project. I added Gerber files to the repository.

Circuit boards populated:

![PCB layout](http://i.imgur.com/X3ZvkEL.png "Finished PCB Top")
![PCB layout](http://i.imgur.com/Wc0AXZP.png "Finished PCB Bottom")

### Rework required

Since it is reuired that the punch baud rate corresponds closely with the actual punch speed, it has to be around 500 bps correpsonding to a punch rate of 50 cps. If faster buffers will overflow. On the other hand it is necessary that the transmit speed is able to handle more than the read speed, 300 cps, thus more than 3000 bps. Maybe 4500 bps is a good choice. Thus this means that we need to handle split speed which is not supported by the Atmega1284p chip USART. But the Atmega 1284p has two USARTS built in. We can thus use one USART for tx and another USART for Rx. The USARTs are operating using different baud rates. Since pin 14 and 15 is used for USART0 and pin 16 and 17 for USART1 we need to do some rewiring. We keep pin 14 as the Rx signal for the punch and use pin 17 as the Tx signal for the reader. However pin 17 is used for the Reader Run signal from the interface. Thus this signal has to be input on pin 15. This signal is PCINT25

### Reader software

The initialization code sets up the USART1 using serial1.begin() but then disbales the receiver so that can be used for a pin change interrupt. Configure the PCINT25 to create change interrupt.

The Reader Run signal is handled by a pin change interrupt handler that detects that this pin has changed. Then it sets the reader_run variable to 2.

Reader 300 cps. I.e 300 steps per second. Timer driven, one interrupt each 1.667 milisecond
Use timer 1 to control the stepper motor. 
 
There need to be a slow turn on / turn off logic as well. The M940 module start
at a 5 ms clock time and then ramps down to a 1.67 ms clock time. We will do 
similar when starting and stopping. If reader_run or FEEDSWITCH signal the rampup variable is decremented (higher speed)
If neither reader_run nor FEEDSWITCH is pressed the rampup variable is incremented until it reaches the MAX_RAMP value.

As long as the rampup value is less than MAX_RAMP we will generated stepper motor pulses else we will stop

The Power line is just to decrease the power to the motor when it is in a stopped
state. Thus to let the motor become completely lose we need to switch all stepper
signals to off. 

Feed hole input generate an edge triggered interrupt. The ISR will the initiate a timer to 
expire within 200 microseconds.
 
The timer 2 200 microseconds timeout ISR will sample the eight holes data and put them into
a buffer and signal a semafor to the mainloop that data is available. If reader_run is greater than 0 it will be decremented. Thus if there are no reader_run pulses we will decrease the speed of the reader until we stop.

Mainloop waits for the data semaphore to be active and then send the byte received over USART1.

### Punch software


The mainloop receives a character on USART0 and puts it into a 8 k buffer. If buffer_full is set the received character is ignored. Incremenets the bufferIn index. If the bufferIn is greater than or equal to 8192 we wrap around to 0. If the bufferIn index is equal to bufferOut we have an overflow condition and then we set the flag buffer_full. 

The punchInt interrupt routine reads one character from the buffer and increments the buffer_out index. If buffer_full is set it will be cleared. The byte is sent to the punch solenoids and the punch done signal is activated. TIMER3 is initailized to do a 10 ms timeout.

The TIMER3 10 ms timeout will clear all signals to the punch soleniods.
