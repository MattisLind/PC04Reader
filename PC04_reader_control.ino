/*

PC04 paper tape reader program for Arduino

Mattis Lind 

Ardruino Uno PINs.

Pins 10,11,12,13 is reserved for the Ethernet shield
Pin 4 is used to control the SD card

To use serial we need to use pin 4 and 10 for stepper rather than 0 and 1.
  
  Arduino pin      Use                      Direction     PC04 Reader Connector
  ------------------------------------------------------------------------------
  4                 Stepper Motor Coil A(0)     Out               P
  10                Stepper Motor Coil A(1)     Out               R
  2                 Stepper Motor Coil B(0)     Out               S
  3                 Feed hole detector          In                N

  5                 Stepper power enable        Out               U
  6                 Feed switch                 In                V
  7                 Stepper Motor Coil B(1)     Out               T
  8                 Hole 1 detector             In                D
  9                 Hole 2 detector             In                E
 A0                 Hole 3 detector             In                F 
 A1                 Hole 4 detector             In                H
 A2                 Hole 5 detector             In                J
 A3                 Hole 6 detector             In                K
 A4                 Hole 7 detector             In                L
 A5                 Hole 8 detector             In                M
 
                    Ground                      GND               C
                    
 Reader 300 cps. I.e 300 steps per second. Timer driven, one interrupt each 1.667 milisecond
 Use timer 1 to control the stepper motor.
 
 There need to be a slow turn on / turn off logic as well. The M940 module start
 at a 5 ms clock time and then ramps down to a 1.67 ms clock time. We will do 
 similar when starting and stopping.
 
 The Power line is just to decrease the power to the motor when it is in a stopped
 state. Thus to let the motor become completely lose we need to switch all stepper
 signals to off. 

 Feed hole input generate an edge triggered interrupt. The ISR will the initiate a timer to 
 expire within 200 microseconds.
 
 The timer 2 200 microseconds timeout ISR will sample the eight holes data and put them into
 a buffer and signal a semafor to the mainloop that data is available.

 Mainloop waits for the semaphore to be active and then reformats the data into hexadecimal 
 and transmits it over serial line. 

*/
#include <avr/io.h>
#include <avr/interrupt.h>

#define STEPPER_A0      4
#define STEPPER_A1      10
#define STEPPER_B0      2
#define STEPPER_B1      7
#define STEPPER_POWER   5
#define FEEDSWITCH      6
#define FEEDHOLE        3
#define HOLE_1          8
#define HOLE_2          9
#define HOLE_3         A0
#define HOLE_4         A1
#define HOLE_5         A2
#define HOLE_6         A3
#define HOLE_7         A4
#define HOLE_8         A5
#define TEST_OUT       11
#define HOLE_1_SHIFT   0
#define HOLE_2_SHIFT   1
#define HOLE_3_SHIFT   2
#define HOLE_4_SHIFT   3
#define HOLE_5_SHIFT   4
#define HOLE_6_SHIFT   5
#define HOLE_7_SHIFT   6
#define HOLE_8_SHIFT   7
#define MAX_RAMP       100
#define RAMP_FACTOR    360

#define TIMER1_VALUE  38869   // preload timer1 65536-16MHz/1/600Hz
//#define TIMER1_VALUE  2

#define TIMER2_VALUE  206    // 256 - 16000000/64*200E-6
#define STEPPER_ON    1
void setup ()
{
  noInterrupts();           // disable all interrupts
  Serial.begin (115200);
  // Setup Stepper pins as output
  pinMode(STEPPER_A0, OUTPUT);
  pinMode(STEPPER_A1, OUTPUT);
  pinMode(STEPPER_B0, OUTPUT);
  pinMode(STEPPER_B1, OUTPUT);
  pinMode(STEPPER_POWER, OUTPUT);
  pinMode(TEST_OUT, OUTPUT);
  // initialize timer1 

  pinMode(FEEDHOLE, INPUT);
  digitalWrite(FEEDHOLE, HIGH);    // Enable pullup resistor
  /*
  EIMSK |= (1 << INT1);     // Enable external interrupt INT1
  EICRA |= (1 << ISC11);    // Trigger INT1 on rising edge
  EICRA |= (1 << ISC10);
  */
  attachInterrupt(1,extInt,RISING);
  
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR2A = 0;
  TCCR2B = 0;

  TCNT1 = TIMER1_VALUE;            
  TCCR1B |= (1 << CS10);    // no prescaler 
  //TCCR1B |= (1 << CS11);
  //TCCR1B |= (1 << CS12);
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  
  //TCCR2B |= (1 << CS10);    // clk / 64 prescaler
  //TCCR2B |= (1 << CS11);
  TCCR2B |= (1 << CS12);
  digitalWrite(STEPPER_POWER, ~STEPPER_ON);

  interrupts();             // enable all interrupts

}

volatile int data;
volatile int data_flag;
volatile int overrun;
// 200 micro second timeout interrupt 
ISR(TIMER2_OVF_vect) {
  digitalWrite(TEST_OUT,0);
  TIMSK2 &= ~(1 << TOIE2);  // Single shot disable interrupt from timer 2
  EIMSK |= (1 << INT1);     // Re-enable external interrupt INT1
  data = digitalRead(HOLE_1);
  data |= (digitalRead(HOLE_2) << HOLE_2_SHIFT);
  data |= (digitalRead(HOLE_3) << HOLE_8_SHIFT);
  data |= (digitalRead(HOLE_4) << HOLE_7_SHIFT);
  data |= (digitalRead(HOLE_5) << HOLE_6_SHIFT);
  data |= (digitalRead(HOLE_6) << HOLE_5_SHIFT);
  data |= (digitalRead(HOLE_7) << HOLE_4_SHIFT);
  data |= (digitalRead(HOLE_8) << HOLE_3_SHIFT);
  if (data_flag) {
    // data_flag was not cleared by main loop. Overrun detected set overrun flag!
    overrun = 1;
  } 
  else {
    data_flag = 1;
  }
}

volatile int reader_run = 0;
volatile int rampup=MAX_RAMP;


// Edge triggered feed hole interrupt routine
void extInt () {
  data_flag = 0;
  digitalWrite(TEST_OUT,1);
  if (reader_run) { 
    TCNT2 = TIMER2_VALUE;      // Set Timer 2 to the 200 micro second timeout
    TIMSK2 |= (1 << TOIE2);    // now enable timer 2 interrupt
    EIMSK &= ~(1 << INT1);     // Disable external interrupt INT1 untill timeout has occured
                            // filtering out spurious interrupts
    
  }
}

int state;

// Stepper motor interrupt routine. 300 Hz
ISR (TIMER1_OVF_vect) {
   if (!digitalRead(FEEDSWITCH)||reader_run) {
     digitalWrite(STEPPER_POWER, STEPPER_ON);
     if (rampup>0) rampup--;
     TCNT1 = TIMER1_VALUE-rampup*RAMP_FACTOR;            // preload timer
     //TCNT1 = TIMER1_VALUE;  
     switch (state) {
       case 0:
         digitalWrite(STEPPER_A1, 0);
         digitalWrite(STEPPER_B0, 0);
         digitalWrite(STEPPER_B1, 1);
         digitalWrite(STEPPER_A0, 1);      
         state = 1;
         break;
       case 1:
         digitalWrite(STEPPER_B1, 0);       
         digitalWrite(STEPPER_A1, 0);
         digitalWrite(STEPPER_A0, 1);
         digitalWrite(STEPPER_B0, 1); 
         state = 2;
         break;
       case 2:
         digitalWrite(STEPPER_B1, 0);
         digitalWrite(STEPPER_A0, 0);
         digitalWrite(STEPPER_A1, 1);
         digitalWrite(STEPPER_B0, 1);       
         state = 3;
         break;
       case 3:
         digitalWrite(STEPPER_A0, 0);
         digitalWrite(STEPPER_B0, 0);
         digitalWrite(STEPPER_B1, 1);
         digitalWrite(STEPPER_A1, 1);
         state = 0;
         break;         
     }
   }
   else {
     digitalWrite(STEPPER_POWER, !STEPPER_ON);
     rampup=MAX_RAMP;
     digitalWrite(STEPPER_A0, 1);
     digitalWrite(STEPPER_B0, 1);
     digitalWrite(STEPPER_B1, 1);
     digitalWrite(STEPPER_A1, 1);
     state=0;
     
   }
}


void loop () {
  int ch;
  if (Serial.available()) {
    ch = Serial.read();
    if (ch == 'R') { // If pressing R the reader will start
       reader_run = 1;
    }
    if (ch == 'S') { // Pressing S will cause it to stop.
       reader_run = 0;
    }
    
  }
  if(data_flag) {
    Serial.write(data);
    data_flag = 0;
  }
  if (overrun) {    // If overrun occurs it will stop reading and print error message.
    overrun=0;
    reader_run = 0;
    Serial.println("ERROR: OVERRUN OCCURED");
  }
  delay(1);
}


