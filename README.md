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

[Webpage](http://www.datormuseum.se/reading-paper-tapes)

