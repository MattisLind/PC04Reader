PC04 Reader / Punch control
==========


This project started by using a Arduino board to control the reader part of a PC04 paper tape reader . I have made a small [webpage](http://www.datormuseum.se/reading-paper-tapes.html) on this.

Now this project has been extended to be able to both control the reader and punch of the DEC PC04. The idea behind this came from the fact that the bootstrap for the high speed and low speed paper tape is the same. The only difference is the CSR used. Thus my idea is to emulate the PC11 using a DL11 card. Another reason is that I have several PC04 but no PC05. Neither do I have any M7810 boards. 

The plan is to manufacture a DEC dual card that inserts into the PC04 and provide a serial interface, both 20 mA current loop and RS-232 levels to both be able to interface with the standard DL11 or any other DEC DL card that has the reader enable signal.

The interface is one rx signal for the punch section running at 500 bps and a tx signal which is running at 3000 bps. The DL11 has to be modified using a different crystal to support this. There is also a reader run signal that starts the reader. Reader run is a pulse that signals that one character is to be read.

AtMega1284p
-----------

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


PC05 / PC11
-----------

This is the target emulation. This is the [PC11 Engieering Drawing](http://storage.datormuseum.se/u/96935524/Datormusuem/PC11_Engineering_Darwings.pdf) and this is the [PC11 manual](http://storage.datormuseum.se/u/96935524/Datormusuem/PC11_Reader-Punch_Manual.pdf). The interface card is the M7810 board which is a very simple unibus interface. The PC05 on the other hand compared with the PC04 include all logic to control the reader and punch. The small backplane in the PC04 has been expanded in the PC05 to twelve slots and three cards is loctated here: M7050, M710 and M715. One interesting note is that the PDP-8/I and PDP-8/L (and I assume also PDP-12 and PDP-15) uses these cards as well, but located in the CPU. (Well not M7050, but M705). This means that the interface the PC05 reader a device address and three IOP signals, IOP1, IOP2 and IOP4. For the reader the IOP1 is used to test the reader flag. IOP2 is used to transfer data from the reader to host and IOP4 is to initiate reading of the next character. For the punch IOP1 is used test the punch done flag, IOP2 to clear the punch done flag and IOP4 to move a byte from host to punch.

For the reader part writing bit 0 of the Reader CSR register will trigger the IOP4 pulse of the reader in the M7050, initiating a read cycle. 



PC04 / PC8E
-----------

This section describe some fact about the [PC04 / PC8E](http://www.bitsavers.org/pdf/dec/pdp8/pdp8e/DEC-8E-HMM3A-C-D_PDP-8e_Maintenance_Manual_Volume_3_External_Bus_Options_Jul73.pdf) combination. The interface card used in the PDP-8/E computer is the M840. The M840 combines all logic found on the M705(0), M710, M715 into one board and add omnibus interfacing. Thus the PC04 is a rather dumb device that rely on the controller do everything, including generating the stepper motor pulses for the stepper. This is the [mainteance manual](https://bitsavers.org/pdf/dec/papertape/DEC-00-PC0A-D_PC04_Manual.pdf) for the PC04/PC04 which contain a lot of interesting information.

![PC04 schematic](http://i.imgur.com/7pdr3M1.png "PC04 schematic")

M044 solenoid driver

![M044 soleniod driver](http://i.imgur.com/RlRk2i8.png "M044 soleniod driver")

The solenoid driver drive the punch solenoids when the inputs are high.  Thus to make sure that no punch are active we need to pull down these inputs since the AtMega chip input floats to a high impedance state when configured as inputs which is the default at startup. The M044 driver card uses SN7401 chip which need at most 1.6 mA out of the chip to detect a low level. High level on the other hand is detected if 40 uA is feed into the chip input. To make sure low is detected at startup of the AtMega chip a rework is required to add pull downs on all outputs. 560 ohm is selcted a pull down. A punch solenoid is activated for half of the time and not all whole are punched all the time so the average powerconsumption should be quite low anyhow. 


The original PC8E uses a M840 board to let the computer control the punch. This uses this circuit to bias the punch sync coil in the punch.

![Punch sync bias circuit](http://storage.datormuseum.se/u/96935524/Datormusuem/Ska%cc%88rmavbild%202014-07-18%20kl.%2007.55.17.png) 

This ciruit is incorporated in the new design.

![Test Punch sync bias](http://i.imgur.com/4ZvZMHd.png "Title")

Reader timing

The picture below describe the reader timing in more detail inside the M840 board.

![Reader timing](http://i.imgur.com/Ik4rNR1.png)

From the description in PC8E manual for the M840 board there are a number of requirements put on the reader logic:
* When stopping it stops over a hole. I.e with FEED HOLE active.
* If there is no READER RUN before a motor next stepper pulse, a motor 40 ms stopping sequence will be initiated. A READER RUN received during this sequence will be ignored.
* The accelaration sequence starts at 5ms step pulse interval and accelarates to 1.667 ms over a 20 ms period
* The FEED HOLE signal is sampled once each step pulse.

Test of the punch sync signal conditioning circuit. The actual rate is not 50 cps but 54.6 cps!
### DL11

The [DL11](http://storage.datormuseum.se/u/96935524/Datormusuem/DL11%20Asynchronous%20Line%20Interface%20Engineering%20Drawings.pdf) Asynchronous Line interface is implemented by DEC on a M7800 circuit board. It is using a standard UART circuit and a number of circuits nu adapt to the unibus and to both EIA RS-232 / CCITT V.28 levels and 20 mA current loop. [DL11 manual](http://storage.datormuseum.se/u/96935524/Datormusuem/DEC-11-HDLAA-B-D%20DL11%20Asynchronous%20Line%20Interface%20Manual.pdf).

The DL11 has a baud rate generator that can generate independet clock signals for the Rx and Tx section of the UART chip. 8 different buad rates are selectable using two rotary switches. The crystal to the baud rate generator can be replaced. 

![Baud rates](http://i.imgur.com/zpG0JzT.png "Baud rates")

The problem is that the speed of the punch is dependent on line frequency and the ratio between the pulleys. If the speed is lower than the rate of the incoming serial line then we eventually would have a buffer overflow. If it is to fast we will have situation where the punch skips to punch for a revolution of the punch pulley. If we assume that the punch speed is exactly 50 cps then a 500 bps baud rate. For the reader part it is just necessary that baud rate is above the 300 cps of the reader, thus more than 3000 bps.




| Divisor | 844.8 kHz | 1032.96 kHz     | 1.152MHz | 4.608MHz | 4.2MHz | 11.52MHz | 7.68MHz     | 3.84MHz     | 1.92MHz     | 8MHz     | 3.93216MHz     | 2MHz     |
|---------|-------------|-------------|---------|---------|--------|----------|-------------|-------------|-------------|-------------|-------------|-------------|
| 23040   | 36,66666667 | 44,83333333 | 50      | 200     | 182,3      | 500      | 333,3333333 | 166,6666667 | 83,33333333 | 347,2222222 | 170,6666667 | 86,80555556 |
| 15360   | 55          | 67,25       | 75      | 300     | 273.4      | 750      | 500         | 250         | 125         | 520,8333333 | 256         | 130,2083333 |
| 7680    | 110         | 134,5       | 150     | 600     | 546,9      | 1500     | 1000        | 500         | 250         | 1041,666667 | 512         | 260,4166667 |
| 3840    | 220         | 269         | 300     | 1200    | 1093,8      |3000     | 2000        | 1000        | 500         | 2083,333333 | 1024        | 520,8333333 |
| 1920    | 440         | 538         | 600     | 2400    | 2187,5      |6000     | 4000        | 2000        | 1000        | 4166,666667 | 2048        | 1041,666667 |
| 960     | 880         | 1076        | 1200    | 4800    | 4375      | 12000    | 8000        | 4000        | 2000        | 8333,333333 | 4096        | 2083,333333 |
| 640     | 1320        | 1614        | 1800    | 7200    | 6562,5      |18000    | 12000       | 6000        | 3000        | 12500       | 6144        | 3125        |
| 480     | 1760        | 2152        | 2400    | 9600    | 8750      | 24000    | 16000       | 8000        | 4000        | 16666,66667 | 8192        | 4166,666667 |

The four first columns are the original crystals as per the DL11 engineering drawings. Then there are a number of columns for recommended crystals that would give a 500 cps rate. The problem, though is that 1.92MHz and 3.84MHz doesn't seem to be standard crystals. 7.68MHz and 11.52 Mhz just might be to high for the DL11 baud rate geberator circuit. This need to be tested. Then there are three columns with crystals that generate cps rates that are slightly higher than 50 cps. 512 bps givs a 2.4% higher rate. If the buffer is 8192 bytes it will take 241333 bytes until there will be a buffer overflow in the punch software. Since the actual speed of the punch is not 50 cps but 54.6 cps a 4.2 MHz crystal which is available from Mouser would be a good alternative. This will cause very little buffering in the punch part of the card.

The Reader Run signal on the DL11 is used with 110 baud teletypes to engage the reader relay and feed it one step forward. The Teletype is connected over current loop which means that the Reader Run signal is only available on 20 mA current loop interface on the DL11.

This is the current loop Rx circuit from the DEC DL11 Async card. The design is aimed to interface towards this ciruit.
Since this Rx circuit is aimed for 110 bps mechanincal relay switching it has to be modified when running at 3000 bps. All the capacitors has to be removed so that the signals is not low pass filtered to much.

![Current Loop Rx Circuit](http://i.imgur.com/KvVhtoU.png "Current Loop Rx Circuit")

And this is the Tx circuit of the DL11 card.

![Current Loop Tx Circuit](http://i.imgur.com/tKE7BaE.png "Current Loop Tx Circuit")

DLV11J
------

The DLV11J card (M8043) has foour serial ports. Using an external adapter it is possible to connect the DLV11J to current loop devices. The DLV11J has circuitry to create the reader pulse which is output on the interface as a TTL level signal.

Thus it would be possible to use the DLV11J signal dirctly by removing the resistor in the READER RUN current loop signal.

| DLV11J pin | Function | 
|------------|----------|
|     1      |  NOT USED     |
|     2      |  GND     |
|     3      |  TX     |
|     4      |  READER RUN |
|     5      |  GND     |
|     6      |  KEY     |
|     7      |  GND     |
|     8      |  RX      |
|     9      |  GND     |
|    10      |  NOT USED  | 


PC11 vs DL11
------------

![PC11 Reader status register](http://i.imgur.com/rmqksxn.png)

PC11 Reader status register

![DL11 Receive status register](http://i.imgur.com/vYxBq08.png)

DL11 Receive status register

![PC11 Punch status register](http://i.imgur.com/MWCWT3o.png)

PC11 Punch status register

![DL11 Transmit status register](http://i.imgur.com/bBcbV9f.png)

DL11 Transmit status register

The difference is as far as I can see that the PC11 can signal errors in th high bit of the status registers. This means that for example out of tape in the punch would not be detected by the software. From a programming point of view the behavior of the reader enable is identical in the PC11 and the DL11. In the PC11 a pulse is generated when writing a 1 that trigger the reading dirctly, in the DL11 a flip flop is set when writing a 1 that is cleared by the receiver busy signal, thus when something is being received.

Aside from the Error indication the hypothesis is that system software would think that a DL11 at CSR 176550 is a in fact a PC11 device!

And yes it does! Aside from the fact that it is not detecting the end of the tape other than there is no more data coming it will successfully both read and write paper tapes udner the RSX11M operating system when connected to a DLV11-F card.

![Running FLX in RSX11M](http://i.imgur.com/UWvIvaJ.png)

I suppose that the inability to cause an error condition at end of tape causes the error message from FLX.

Adding handling of paper-out
----------------------------

Possible modifications to the DLV11-F to allow for paper out error:

![DLV11-F modifictaions](http://i.imgur.com/UPWZpWg.png)

Add a input paper error signal at pin 3 of E32 instead of ground.

Punch paper-out is a switch which is closed when paper out condition occur. It is connected to alot B2, pin E and T. Pin T is connected to slot B1 while E is unconnected on my unit. B1E is wired to ground level. On the reader/punch board the T signal is connected to pin 19. An external pull-up resistor is used. The corresponding PORT pin is set to 0 and the DDR bit is used to either three state the signal or drive it low. Thus an wire-or function will result. The output is driven low when the punch is off-line. I.e. there are no pulses coming from the punch-sync signal for more than 18 ms. The combined signal at pin 19 is then the PUNCH ERROR signal which is active low. It will be active when paper is out, punch not running or punch/reader out of power.

The online / offline switch is connected to pin B of A1. It is +5V when on-line and floating when off-line. This signal is connected to pin 15 and wire-ored with an internal signal output on this pin. An external pull-down resistor is needed so that it will read low when the switch is off-line. The internal paper out signal is sent to this pin as an active low signal and wire-ored with the on-line switch signal. This is handled by setting the correspnding the PORT bit to 0 while changing the DDR bit. The resulting signal, READER ERROR is active low and active when paper is out in reader, reader off-line or power off. Pin 15 is normally used for TxD when programming the device (or debugging) but shall be possible to use as output while not connected to programmer.

Modifying DL11 and DLV11 boards
-------------------------------

I decided to go for a simple all TTL interface using some of the unused signal in the 40 pin connector of both DL11 and DLV11.

| PIN | Function |
|-----|----------|
| E   | Serial In TTL |
| R  | Serial Out TTL |
| D   | Punch Error TTL |
| L   | Reader Error TTL |
| N,CC   | External clock TTL |
| P  | Reader Run TTL |
| UU | GND |

Then there are a couple of modifictions to be done to the M7800 (DL11) and M7940 (DLV11) boards to allow external clock while simulatenously generating an internal receiver clock, being able to convey the punch error and reader error signals to the bus and to generate interrupts based on punch error and reader error.

The DL11 already support to use external clock. It is just a matter of setting the TX clock roatry switch to position 9. Then it will use the clock signal supåplied on pin CC. 

To support READER ERROR the active low signal is received by a 7404 inverting buffer which has to be added to the board and then forwarded to pin 12 of E01 bus driver.

![DL11 Reader Error](http://i.imgur.com/6nnOz00.png)

For PUNCH ERROR the active low signal is likewise buffered in a 7404 inverting buffer and the forwarded to pin 8 on E02 bus driver. Pin 8 is already in use. The signals to pins 8 and 9 has to be cut off. The DL-5 XCSR TO BUS is connected fro E11 pin 9 to E02 pin 9.

![DL11 Punch Error](http://i.imgur.com/ntVVSQh.png)

The existing RCVR INT H signal is intercepted before arriving to pin 1 of E52 and pin 2 of E58. This signal is OR:ed together in a 7432 chip that is added to the board with the READER ERROR signal output from the 7404 inverting buffer. The output of the OR gate is connected to pin 1 of E52 and pin 2 of E58.

The same procedure takes place for the PUNCH ERROR signal, intercepting XMIT INT H signal before it arrives at E57 pin 1 and E58 pin 5. This signal is connected to another 7432 OR-gate together with the PUNCH ERROR signal from the 7404 inverting buffer.

![DL11 interrupts](http://i.imgur.com/sL98qqX.png)


For the DLV11 board the input clock signal to the UART has to be cut off. The UART pin 40 is wired to 40 pin connector pin N.

![DL11 interrupts](http://i.imgur.com/KyJr2B6.png)

The READER ERROR signal at pin D is feed via a 7404 inverting buffer chip to pin 14 of E30. The current signal is cut off. The PUNCH ERROR signal at pin L is feed via a 7404 inverting buffer to pin 13 of E30. This pin is currently grounded. Cut it off.

![DL11 interrupts](http://i.imgur.com/ge2Ue6n.png)

The active high READER ERROR and PUNCH ERROR signals from the outputs of the 7404 inverting buffer is feed into one each 7432 OR-gate. The other input to this OR-gate is the signal going to E32 pin 9 and 12. This signals need to be intercepted.

![DL11 interrupts](http://i.imgur.com/umuGUcl.png)



Synthezise clock signal
-----------------------

Pin 21 is used as an output for a clock signal used by the transmitter in the host. It is synthezised as a signal with a frequency 320 times the frequency of the punch sync signal. Thereby there is no need for special crystals and there will be no buffering over-run in the punch logic. The clocg generator is made using timer 0 in the atmega chip. When the punch is off-line and no punch sync is generated the signal is fixed at 54.6 * 320 Hz. Each timeout interrupt of the timer 0 will toggle pin 21. The timeout routine will increase an internal variable. The pucnh sync interrupt will check this variable at each interrupt. If the value is higher than 320 it will lower the timeout value for timer 0 and if it is higher it will increase the value so that clock signal will be locked to the punch sync signal.



Implementation
--------------


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
    18                Punch Done                  Out                M
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
    

This is the [schematic as a PDF](http://storage.datormuseum.se/u/96935524/Datormusuem/PC04-controller.pdf) and below is a jpeg.

![Schematic](http://storage.datormuseum.se/u/96935524/Datormusuem/PC04-controller.jpg "schematic")

After scanning and updating the page I found that I missed the supply voltages and some decoupling capacitors. AA is supposed be +5V and AC is ground. There is two 100nF decoupling capacitors and one 22uF (or similar) tantalum capacitor which is not shown in the schematic.

#### Gerber files

![PCB layout](http://i.imgur.com/TbeRUjJ.png "PCB layout")

This is the layout I did fo this project. I added Gerber files to the repository.

Circuit boards populated:

![PCB layout](http://i.imgur.com/X3ZvkEL.png "Finished PCB Top")
![PCB layout](http://i.imgur.com/Wc0AXZP.png "Finished PCB Bottom")

#### Rework required

Since it is reuired that the punch baud rate corresponds closely with the actual punch speed, it has to be around 500 bps correpsonding to a punch rate of 50 cps. If faster buffers will overflow. On the other hand it is necessary that the transmit speed is able to handle more than the read speed, 300 cps, thus more than 3000 bps. Maybe 4500 bps is a good choice. Thus this means that we need to handle split speed which is not supported by the Atmega1284p chip USART. But the Atmega 1284p has two USARTS built in. We can thus use one USART for tx and another USART for Rx. The USARTs are operating using different baud rates. Since pin 14 and 15 is used for USART0 and pin 16 and 17 for USART1 we need to do some rewiring. We keep pin 14 as the Rx signal for the punch and use pin 17 as the Tx signal for the reader. However pin 17 is used for the Reader Run signal from the interface. Thus this signal has to be input on pin 20 which is unused. This signal is PCINT30

![Top side with reworks](http://i.imgur.com/nR6pEld.png "Top side with reworks")

Details shown below:
![Detail of top side](http://i.imgur.com/TnXaJmh.png "Detail of top side")

1. Do not cut wire at pin 15.
2. Cut wire at pin 17 and wire it to pin 11 of MAX3232.
3. Add a wire from pin 20 to the trace that was cut off above.

![Detail of top side](http://i.imgur.com/voyaFq7.png "Detail of top side")

1. Cut off wire going to D6 and D7.
2. Run wire from D6 and D7 to pin 11 of MAX3232.

![Bottom side with reworks](http://i.imgur.com/nOEWdYs.png "Bottom side with reworks")

1. Add 1k pull down resistors from pin 22-29 and pin 18 of CPU chip.
2. Cut off trace at pin 11 of MAX3232.

Pin 4 of J5 need to be connected to the bottom layer ground plane.

The current loop interface (J4) is wrongly designed and need rework to work correctly.
When testing the current loop interface it appeared that the design was seriously flawed. I tried to interface the boaard with a DLV-11F board (M8027). Looking at the schmetics for the DLV11-F showed that the current loop interface can be either active or passive. Active means that the board is supplying the line current while passive is a consumer. The revised the design makes use of the Passive transmitters and receivers using opto couplers on the DLV11-F board. However, Reader Run signal needto be active on the DLV11-F card. So we need to insert an opto coupler on the PC04 controller board. 

A number of cuts and jumpers had to be added as well as an extra 4N37 opto coupler to get it working. In the process of getting it to work there were certain emount of experimentation which lead to some unnecessary cuts done. Please note that only those marked need be cut. There are one wire that is not needed if one of those cuts aren't done.

![Top layer current loop reworks](http://i.imgur.com/JSQdU1X.jpg "Top layer current loop reworks")

![Bottom layer current loop reworks](http://i.imgur.com/NPIjn1B.jpg "Bottom layer current loop reworks")

With all these modifications it was possible to load and run the paper tape BASIC on a LSI-11/03 computer wita a DLV11-F board. The board was setup for address 177550 and vector 70. The speed was set to split speed 300/19200. It was also possible toi punch the resulting BASIC program to tape using the SAVE program.

#### Modifying the backplane

The PC04 need a small modifcation to support this card. The +5V power line at A pin in the top-most connector of the backplane need to be connected to the connector for the interface card so the card have a power supply.

#### Component list

The layout has several faults. Most notably the silkscreen has numbers of defects. Missing component designators etc.

|ID|Component| 
|-----------|-----------|
|  U3       | AtMega1284P|
|  U4       | MAXIM MAX3232CPE |
|  U5,U7,U8 | BC549B |
|  U7,U8    | BC307 |
|  R1       | 100 |
|  R2       | 3.3k |
|  R3       | 100 |
|  R4       | 5.6k |
|  R5       | 220  |
|  R6       | 3.3k |
|  R7       | 120  |
|  R8       | 1k  |
|  R9       | 1k  |
|  R10      | 680 |
|  R11      | 330 |
|  R12      | 330 |
|  R13      | 1k  |
|  C4       | 100n |
|  C10      | 33u Tantalum |
|  C crystal | 22p ceramic |
|  C other   | 10n ceramic |
|  Y1        | 16M Crystal |
| D1         | 5 mm LED |
| D2,D3,D4,D5,D6,D7 | IN4148 |
| J1,J2,J3,J4,J5 | pin header |

#### Jumpers and connectors

There are two jumpers that control what is connected to the Reader Run input and the Rx input of the microcontroller. Generally either the inputs come from the current loop interface or the MAX3232 converter. For Rx the middle pin is also connected to the software download connector. 

The pinout of the J1 connector from left to right, top view.

|Pin|Function|
|-----|----------|
|  1  | DTR/RESET|
|  2  |   RXI    |
|  3  |   TXO    |
|  4  |   VCC    |
|  5  |   GND    |
|  6  |   GND    |

The pinout of the J2 connector from left to right, top view.

| Pin | Function | Arduino pin |
|-----|----------|-------------|
|  1  |   VCC    |   VCC       |
|  2  |   GND    |   GND       |
|  3  |   RESET  | PIN10       |
|  4  |   MOSI   |    PIN11    |
|  5  |   MISO   |    PIN12    |
|  6  |   SCK    |    PIN13    |

The pinout of the J5 connector. Pin 1 is the top most pin.

| Pin | Function | RS232 9 pin DSUB |
|-----|----------|-------------|
|  1  |   DTR    |     4       |
|  2  |   RXD    |     3       |
|  3  |   TXD    |     2       |
|  4  |   GND    |     5       |

Arduino pin indicate the pin on the Arduino board to connect to when programming the boot loader.

#### Burning bootloader

I used another Arduino board wich I connect to J2 to download the bootloader. Basically I use the instructions from this [blogpost](http://maniacbug.wordpress.com/2011/11/27/arduino-on-atmega1284p-4/). The pinout of J2 is the same as the pinout of J2 in the blog post.

If you experience trouble burning the bootloader, avrdude reporting that the signature is not what was expected, this is likely due to two different versions of the 1284 chip, the 1284 and the 1284p. The boards.txt file now has 1284p configured. Change to 1284 to program older chips. 

Also make sure to change the boot loader so that it reports the right sinature. Since it has to be compiled and I didn't have the proper environment I simply patched the HEX-file.

Change:

```
:10FD700085E080CF813511F488E018D01DD080E176
```
To:
```
:10FD700086E080CF813511F488E018D01DD080E175
```

#### Download software

Be sure to download all 1284 libraries from the link above. Then attach the serial device to J1. The pinout is the same as JP1 in the blog post above. Make sure that the jumper closest to the CPU is removed while trying to brogram, otherwise the Rx signals will collide and the chip won't download.

#### Reader software

As of the later revisions of the firmware the reader software has been completely rewritten to allow start stop operation. That means that the it is possible to read at full speed and the stop reading and it will be possible to continue reading from next byte when ever the host wishes. In order to do so all of the reader code is completely changed. There is a more advanced state machine that understand what position the feed wheel has. It will always stop the feed with the feed hole over the reader head. 

However the disadvantage is that hsost using USB RS232 dongles will not read continously any longer. This has to do with the slow turn around time in the USB host driver. There are ways to improve the latency but it will not be perfect which will cause the read to start and stop all the time. This is no major problem though since everythings is read in as it should.

#### Punch software


The mainloop will check if the punch is done and that if there is a character available in the input queue it will read the character. Thus it will use the built in buffering routines in the serial library 

The punchInt interrupt routine reads the buffered data and write it to the punch solenoids and the punch done signal is activated. TIMER3 is initailized to do a 10 ms timeout.

The TIMER3 10 ms timeout will clear all signals to the punch soleniods.

#### Main loop

The main loop will only do forwarding of data from and to the serial ports.
