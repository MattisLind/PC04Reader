/*

PC04 paper tape reader program for Arduino

Mattis Lind 

Ardruino Uno PINs.

Pins 10,11,12,13 is reserved for the Ethernet shield
Pin 4 is used to control the SD card

To use serial we need to use pin 4 and 10 for stepper rather than 0 and 1.
  
  Atmega1284 pin   Arduino Pin    Use                         Direction     PC04 Reader Connector
  --------------------------------------------------------------------------------------------
  1                 0             Stepper Motor Coil A(0)     Out               P
  2                 1             Stepper Motor Coil A(1)     Out               R
  4                 3             Stepper Motor Coil B(0)     Out               S
  3                 2             Feed hole detector          In                N

  6                 5             Stepper power enable        Out               U
  7                 6             Feed switch                 In                V
  5                 4             Stepper Motor Coil B(1)     Out               T
 40                24             Hole 1 detector             In                D
 39                25             Hole 2 detector             In                E
 38                26             Hole 3 detector             In                F 
 37                27             Hole 4 detector             In                H
 36                28             Hole 5 detector             In                J
 35                29             Hole 6 detector             In                K
 34                30             Hole 7 detector             In                L
 33                31             Hole 8 detector             In                M
 
                                  Ground                      GND               C
                                  +5V                                           A
                    
  Atmega1284 pin   Arduino pin      Use                      Direction     PC04 Punch Connector
  ---------------------------------------------------------------------------------------------   
  8                 7               Punch Feed Switch            In                A
                                    GND                                            B
 16                10               Punch Sync                   In                C
                                    GND                                            D
 18                12               Punch Done                   Out               E
 29                23               Hole 8 punch                 Out               H
 28                22               Hole 7 punch                 Out               J
 27                21               Hole 6 punch                 Out               K
 26                20               Hole 5 punch                 Out               L
                                    Bias to punch sync coil      Out               M
 25                19               Hole 4 punch                 Out               N
 24                18               Hole 3 punch                 Out               P
 23                17               Hole 2 punch                 Out               R
 22                16               Hole 1 punch                 Out               S
 19                13               Out of tape                  In                T
 
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

#define STEPPER_A0      0
#define STEPPER_A1      1
#define STEPPER_B0      3
#define STEPPER_B1      4
#define STEPPER_POWER   5
#define FEEDSWITCH      6
#define PUNCH_FEED      7
#define FEEDHOLE        2
#define HOLE_1         24
#define HOLE_2         25
#define HOLE_3         26
#define HOLE_4         27
#define HOLE_5         28
#define HOLE_6         29
#define HOLE_7         30
#define HOLE_8         31
#define TEST_OUT       11
#define HOLE_1_SHIFT   0
#define HOLE_2_SHIFT   1
#define HOLE_3_SHIFT   2
#define HOLE_4_SHIFT   3
#define HOLE_5_SHIFT   4
#define HOLE_6_SHIFT   5
#define HOLE_7_SHIFT   6
#define HOLE_8_SHIFT   7
#define PUNCH_DONE     18
#define MAX_RAMP       100
#define RAMP_FACTOR    360
#define READER_RUN     16
#define TIMER1_VALUE  38869   // preload timer1 65536-16MHz/1/600Hz
#define TIMER3_VALUE  45536   // preload timer3 65536-16MHz/8/100Hz)
//#define TIMER1_VALUE  2

#define TIMER2_VALUE  206    // 256 - 16000000/64*200E-6
#define STEPPER_ON    1

volatile int readerRunLastLevel;

void setup ()
{
  noInterrupts();           // disable all interrupts
  
  // setup the two USARTs
  Serial.begin (300); // Punch receive 300 bps,  transmit disabled
  UCSR0B &= ~TXEN0;
  Serial1.begin (4800);  // Reader transmit 4800 bps, serial receive disabled
  UCSR1B &= ~RXEN1;

  // Setup reader run input  
  PCMSK3 |= (1<<PCINT25);
  PCICR |= (1<<PCIE3);
  readerRunLastLevel=digitalRead(READER_RUN);
  
  
  // Setup Stepper pins as output
  pinMode(STEPPER_A0, OUTPUT);
  pinMode(STEPPER_A1, OUTPUT);
  pinMode(STEPPER_B0, OUTPUT);
  pinMode(STEPPER_B1, OUTPUT);
  pinMode(STEPPER_POWER, OUTPUT);
  pinMode(TEST_OUT, OUTPUT);
  // initialize timer1 
  DDRA = 0x00;  // Port A is inputs
  pinMode(FEEDHOLE, INPUT);
  digitalWrite(FEEDHOLE, HIGH);    // Enable pullup resistor

  attachInterrupt(2,extInt,RISING);
  
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
  TCCR2B |= (1 << CS22);
  digitalWrite(STEPPER_POWER, ~STEPPER_ON);

  // Punch init
  pinMode(PUNCH_DONE, OUTPUT);
  attachInterrupt(0,punchInt,RISING);  // punch sync signal on ping 16 which is INT2 external interrupts
  PORTD = 0x00; // Low out disables drives. External pull downs are used as well.
  DDRC = 0xff; // All PORTC as outputs;
  
  // Timer 3 init  
  TCCR3B |= (1<<CS31);   // prescaler set to divde by 8.
  
  
  interrupts();             // enable all interrupts

}

/*#define BUF_SIZE 8192

volatile char buf[BUF_SIZE];
voltaile int bufIndexIn, bufIndexOut;
volatile int bufferFull, bufferEmpty; */
volatile int punchData;
volatile int punchFlag;
volatile int data;
volatile int data_flag;
volatile int overrun;
volatile int reader_run = 0;

// 200 micro second timeout interrupt 
ISR(TIMER2_OVF_vect) {
  digitalWrite(TEST_OUT,0);
  TIMSK2 &= ~(1 << TOIE2);  // Single shot disable interrupt from timer 2
  EIMSK |= (1 << INT1);     // Re-enable external interrupt INT1
  data = PORTA;
  if (data_flag) {
    // data_flag was not cleared by main loop. Overrun detected set overrun flag!
    overrun = 1;
  } 
  else {
    data_flag = 1;
  }
  if (reader_run) reader_run--;
}


volatile int rampup=MAX_RAMP;


// Edge triggered feed hole interrupt routine
void extInt () {
  data_flag = 0;
  digitalWrite(TEST_OUT,1);
  if (rampup < MAX_RAMP) { 
    TCNT2 = TIMER2_VALUE;      // Set Timer 2 to the 200 micro second timeout
    TIMSK2 |= (1 << TOIE2);    // now enable timer 2 interrupt
    EIMSK &= ~(1 << INT1);     // Disable external interrupt INT1 untill timeout has occured
                            // filtering out spurious interrupts
    
  }
}


// The Pin chnage interrupt that handles the READER RUN signaö from the interface.
ISR (PCINT3_vect) {
  int readerRunLevel = digitalRead(READER_RUN); 
  if ((readerRunLevel != readerRunLastLevel) && readerRunLevel) {
    // pin changed and it is high, set flag !
    reader_run = 2;
  }
}

int state;

// Stepper motor interrupt routine. 300 Hz
ISR (TIMER1_OVF_vect) {
   if (!digitalRead(FEEDSWITCH)||reader_run) {
     if (rampup>0) rampup--;
     TCNT1 = TIMER1_VALUE-rampup*RAMP_FACTOR;            // preload timer
     //TCNT1 = TIMER1_VALUE;  
   }
   else {
     if (rampup<MAX_RAMP) rampup++;
     TCNT1 = TIMER1_VALUE-rampup*RAMP_FACTOR;            // preload timer
     //TCNT1 = TIMER1_VALUE;  
   }
   if (rampup < MAX_RAMP) {
     digitalWrite(STEPPER_POWER, STEPPER_ON);
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

void punchInt () { 
  if (punchFlag) { 
    PORTD = punchData;    // Output the byte from the buffer.
    digitalWrite(PUNCH_DONE,1); // switch on feed hole and move forward solenoid
    punchFlag=0;          // Clear the punch sempaphore
  }
  if (!digitalRead(PUNCH_FEED)) {
    digitalWrite(PUNCH_DONE,1);
  }
  TCNT3 = TIMER3_VALUE; // reset the timer value for the 10 ms timout
  TIMSK3 |= (1<<TOIE3); // enable timer 3 interrupts on overflow. 
}

// The TIMER 3 ISR that switches the punch solenoids off.
ISR (TIMER3_OVF_vect) {
  PORTD = 0x00;          // switch off solenoids
  digitalWrite(PUNCH_DONE,0); // switch off feed hole and move forward solenoid
  TIMSK3 &= ~(1<<TOIE3); // Disable timer 3 interrupts
  EIMSK |= (1 << INT2);  // enable punch sync interrupts
}


void loop () {
  int ch;
  if (Serial.available()) {
    ch = Serial.read();
    if (!punchFlag) {  // The one char buffer is already full. We haven't punched anything yet.
      punchData = ch;
      punchFlag = 1;
    } 
  }
  if(data_flag) {
    Serial1.write(data);
    data_flag = 0;
  }
}


