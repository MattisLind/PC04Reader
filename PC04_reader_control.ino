/*

PC04 paper tape reader program for Arduino

Mattis Lind 

  
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

 Mainloop waits for the semaphore to be active and transmits it over serial line. 
 
 The main loop checks if a character is available, it then reads it into a buffer and set the punch flag.
 
 The punch sync signal trigger an interrupt on the rising edge. If the punch flag is set in the interrupt routine
 it will output the data from the punch buffer and reset the pucn flag. It will also set the punch done signal
 which will punch the feed hole and forward the tape one step. 
 
 Then it will arm a single shot time out to interrupt after 10 ms. This interrupt routine will switch off all
 punch drivers and reinitate the external interrupt.

*/
#include <avr/io.h>
#include <avr/interrupt.h>

#define STEPPER_A0      0
#define STEPPER_A1      1
#define STEPPER_B0      3
#define STEPPER_B1      4
#define STEPPER_POWER   5
#define FEEDSWITCH      6  // active low
#define PUNCH_FEED      7
#define FEEDHOLE        2
#define TEST_OUT       15
#define PUNCH_DONE     12
#define READER_RUN     14
#define TIMER1_VALUE_1_6MS  65119   // preload timer1 65536-16MHz/1/600Hz
#define TIMER1_VALUE_2MS   65036        // preload timer1 65536-16000000/64/500Hz
#define TIMER1_VALUE_3MS   64785    // prelaod timer1 65536-16000000/64/333Hz
#define TIMER1_VALUE_5MS   64286     // preload timer1 65536-16000000/64/200Hz
#define TIMER1_VALUE_40MS  55536         // 65536-16000000/64/25Hz
#define NUM_TIMER1_STEPPING_VALUES 27 
//#define TIMER2_VALUE  206    // 256 - 16000000/64*200E-6
#define TIMER2_VALUE  243    // 256 - 16000000/64*50E-6

#define TIMER3_VALUE  45536   // preload timer3 65536-16MHz/8/100Hz)
#define RUN 0
#define STOPPING 1
#define STOPPED 2
#define STEPPER_ON    1
#define STEPPER_OFF 0
//#define DEBUG 1

#define STATE_IDLE 0
#define STATE_STOPPED_2 1
#define STATE_STOPPED_4 2
#define STATE_STOPPING_2 3
#define STATE_STOPPING_4 4
#define STATE_RUNNING_1 5
#define STATE_RUNNING_2 6
#define STATE_RUNNING_3 7
#define STATE_RUNNING_4 8

#define EVENT_INIT 0
#define EVENT_RUN 1
#define EVENT_TIMEOUT 2
#define EVENT_CLOCK 3
#define EVENT_FEED 4

#ifdef DEBUG
#define debugPrintValue(c,i) Serial.write(c); Serial.write('X'); Serial.print(i,HEX); Serial.write(c)
#define debugPrint(c) Serial.write(c)
#else
#define debugPrint(c)
#define debugPrinteValue(c,i)
#endif

#define debugPrintValue(c,i) Serial.write(c); Serial.write('X'); Serial.print(i,HEX); Serial.write('X')

void readerFSM(int);

int steppingIntervals [NUM_TIMER1_STEPPING_VALUES] = {65119,65100,65075, 65050,65025, 65000,64975,  64950,64925, 64900, 64875, 64850, 64825, 64800, 64775, 64750,64725, 64700, 64675, 64650, 64600, 64550, 64500, 64450, 64400, 64350, 64300}; 
volatile int punchData;
volatile int punchFlag=0;
volatile int notEmpty=4;
volatile int timeoutIndex;
volatile int readerRun;
volatile int stepsToRun;
volatile int feedStopping;
volatile int feedHole;
volatile int readerState;
volatile int feed = 0;

void setup ()
{
  noInterrupts();           // disable all interrupts
  
  // setup the two USARTs
#ifdef DEBUG
  Serial.begin (115200);
#else
  Serial.begin (300); // Punch receive 300 bps,  transmit disabled
  UCSR0B &= ~(1<<TXEN0);
#endif
  Serial1.begin (19200);  // Reader transmit 19200 bps, serial receive disabled
  UCSR1B &= ~(1<<RXEN1);

  // Setup reader run input  
  PCMSK3 |= (1<<PCINT30);
  PCICR |= (1<<PCIE3);
  
  // Setup feed switch interrupt handling
  PCMSK1 |= (1<<PCINT14);
  PCICR |= (1<<PCIE1);  
  
  // Setup Stepper pins as output
  pinMode(STEPPER_A0, OUTPUT);
  pinMode(STEPPER_A1, OUTPUT);
  pinMode(STEPPER_B0, OUTPUT);
  pinMode(STEPPER_B1, OUTPUT);
  pinMode(STEPPER_POWER, OUTPUT);
  DDRA = 0x00;  // Port A is inputs
  pinMode(FEEDHOLE, INPUT);
  digitalWrite(FEEDHOLE, HIGH);    // Enable pullup resistor
  attachInterrupt(2,extInt,RISING); 
  // initialize timer1
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= (1 << CS10);    // clk/64 prescaler
  TCCR1B |= (1 << CS11);
  TIFR1 |= (1 << TOV1);
  // initialize timer2
  TCCR2A = 0;
  TCCR2B = 0;
  TCCR2B |= (1 << CS22);    // divide by 64 prescaler
  

  // Punch init
  pinMode(PUNCH_DONE, OUTPUT);
  attachInterrupt(0,punchInt,RISING);  // punch sync signal on pin 16 which is INT0 external interrupts
  PORTC = 0x00; // Low out disables drivers. External pull downs are used as well.
  DDRC = 0xff; // All PORTC as outputs;
  
  pinMode(PUNCH_DONE, OUTPUT);
  pinMode(PUNCH_FEED, INPUT);
  digitalWrite(PUNCH_FEED, HIGH);    // Enable pullup resistor  
  // Timer 3 init  
  TCCR3A = 0;
  TCCR3B = (1<<CS31);   // prescaler set to divde by 8.
  TCCR3C = 0;
  TIMSK3 = 0;
  TIFR3 |= (1 << TOV3);
  TIMSK1 = 0;
  readerState = STATE_IDLE;
  readerFSM(EVENT_INIT);
  interrupts();             // enable all interrupts

}


void step(int stepper_state)
{
  switch (stepper_state) {
    case 1:
      digitalWrite(STEPPER_A1, 0);
      digitalWrite(STEPPER_B0, 0);
      digitalWrite(STEPPER_B1, 1);
      digitalWrite(STEPPER_A0, 1); 
      debugPrint('1');  
      break;
    case 2:
      digitalWrite(STEPPER_B1, 0);       
      digitalWrite(STEPPER_A1, 0);
      digitalWrite(STEPPER_A0, 1);
      digitalWrite(STEPPER_B0, 1);
      debugPrint('2');  
      break;
    case 3:
      digitalWrite(STEPPER_B1, 0);
      digitalWrite(STEPPER_A0, 0);
      digitalWrite(STEPPER_A1, 1);
      digitalWrite(STEPPER_B0, 1); 
      debugPrint('3');       
      break;
    case 4:
      digitalWrite(STEPPER_A0, 0);
      digitalWrite(STEPPER_B0, 0);
      digitalWrite(STEPPER_B1, 1);
      digitalWrite(STEPPER_A1, 1);
      debugPrint('4'); 
      break;         
  }  
}


// 200 micro second timeout interrupt 
ISR(TIMER2_OVF_vect) {
  TIMSK2 &= ~(1 << TOIE2);  // Single shot disable interrupt from timer 2
  sei();
  if (digitalRead(FEEDHOLE)) {
    debugPrint('A');
    feedHole = 1;
    debugPrintValue('B',PINA);
    Serial1.write(PINA);
  }
  else {
    debugPrint('C');
  }
}




// Edge triggered feed hole interrupt routine
// Edge triggered feed hole interrupt routine - detect if not empty
void extInt () {  
  notEmpty = 6; 
  EIFR |= (1 << INTF2);
  EIMSK &= ~(1 << INT2);     // Disable external interrupt INT2 untill timeout has occured
  sei();
  TIFR2 |= (1<<TOV2);         // clear pending interrupts
  TCNT2 = TIMER2_VALUE;      // Set Timer 2 to the 200 micro second timeout  
  TIMSK2 |= (1 << TOIE2);    // now enable timer 2 interrupt  
                             // filtering out spurious interrupts  
  debugPrint('D');  
}


ISR (PCINT1_vect) {                                // Pressing the FEED switch
  readerFSM(EVENT_FEED);
}

// The Pin change interrupt that handles the READER RUN signal from the interface.
ISR (PCINT3_vect) { 
    if (digitalRead(READER_RUN) && notEmpty) { 
        readerFSM(EVENT_RUN);           
    }
}

void readerFSM (int event) {
  if (readerState == STATE_IDLE) {
    if (event == EVENT_INIT) {
      step(2);
      digitalWrite(STEPPER_POWER, STEPPER_OFF); // no stepper power.
      readerState = STATE_STOPPED_2;
    }
  }
  else if (readerState == STATE_STOPPED_2) {
    if (event == EVENT_RUN) {
      timeoutIndex=NUM_TIMER1_STEPPING_VALUES-1;                // set slowest possible speed  
      TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses 
      TIFR1 |= (1 << TOV1);                                     // Reset all pending timer1 interrupts  
      TIMSK1 |= (1 << TOIE1);                                   // enable timer overflow interrupt  
      sei();  
      readerRun=0;
      debugPrintValue('E',PINA);  
      Serial1.write(PINA);                                     // over the feed hole already - Read in and write to serial buffer
      readerState = STATE_RUNNING_3;
      digitalWrite(STEPPER_POWER, STEPPER_ON);                  // Switch on full power to stepper
      step(3);
    }
    else if (event == EVENT_CLOCK) {
      debugPrint('F');
      TIMSK1 &= ~(1 << TOIE1);                                 // disable timer overflow interrupt 
    }
    else if (event == EVENT_FEED) {
      if (!digitalRead(FEEDSWITCH)) {
        readerState = STATE_RUNNING_3;
        feed = 1;
        timeoutIndex=NUM_TIMER1_STEPPING_VALUES-1;                // set slowest possible speed  
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses  
        TIFR1 |= (1 << TOV1);                                                // Reset all pending timer1 interrupts.
        TIMSK1 |= (1 << TOIE1);                                   // enable timer overflow interrupt       
        digitalWrite(STEPPER_POWER, STEPPER_ON);                  // Switch on full power to stepper 
        debugPrint('G');
        step(3);                                                   // Step one step forward   
      } 
    }
  }
  else if (readerState == STATE_STOPPED_4) {
    if (event == EVENT_RUN) {
      timeoutIndex=NUM_TIMER1_STEPPING_VALUES-1;                // set slowest possible speed  
      TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses 
      TIFR1 |= (1 << TOV1);                                     // Reset all pending timer1 interrupts  
      TIMSK1 |= (1 << TOIE1);                                   // enable timer overflow interrupt  
      sei();  
      readerRun=0;
      debugPrintValue('H',PINA);
      Serial1.write(PINA);                                     // over the feed hole already - Read in and write to serial buffer
      readerState = STATE_RUNNING_1;
      digitalWrite(STEPPER_POWER, STEPPER_ON);                  // Switch on full power to stepper
      step(1);
    }
    else if (event == EVENT_CLOCK) {
      debugPrint('I');
      TIMSK1 &= ~(1 << TOIE1);                                 // disable timer overflow interrupt 
    }
    else if (event == EVENT_FEED) {
      if (!digitalRead(FEEDSWITCH)) {
        readerState = STATE_RUNNING_1;
        feed = 1;
        timeoutIndex=NUM_TIMER1_STEPPING_VALUES-1;                // set slowest possible speed  
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses  
        TIFR1 |= (1 << TOV1);                                                // Reset all pending timer1 interrupts.
        TIMSK1 |= (1 << TOIE1);                                   // enable timer overflow interrupt       
        digitalWrite(STEPPER_POWER, STEPPER_ON);                  // Switch on full power to stepper 
        debugPrint('J');
        step(1);        // Step one step forward   
      } 
    }
  }
  else if (readerState == STATE_STOPPING_2) {
    if (event == EVENT_RUN) {
      readerRun = 1;
      debugPrint('K');
    }
    else if (event == EVENT_CLOCK) {
      if (readerRun) {
        timeoutIndex=NUM_TIMER1_STEPPING_VALUES-1;                // set slowest possible speed  
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses 
        TIFR1 |= (1 << TOV1);                                     // Reset all pending timer1 interrupts  
        TIMSK1 |= (1 << TOIE1);                                   // enable timer overflow interrupt  
        sei();  
        readerRun=0;
        debugPrintValue(',',PINA);  
        Serial1.write(PINA);                                     // over the feed hole already - Read in and write to serial buffer
        readerState = STATE_RUNNING_3;
        step(3);
      } 
      else {
        readerState = STATE_STOPPED_2;
        digitalWrite(STEPPER_POWER, STEPPER_OFF);
        TIMSK1 &= ~(1 << TOIE1);                                 // disable timer overflow interrupt
        notEmpty = 6;
        debugPrint('L');
      }
    }
    else if (event == EVENT_FEED) {
      feed = !digitalRead(FEEDSWITCH);
      debugPrint('M');
    }
  }
  else if (readerState == STATE_STOPPING_4) {
    if (event == EVENT_RUN) {
      readerRun = 1;
      debugPrint('N');
    }
    else if (event == EVENT_CLOCK) {
      if (readerRun) {
        timeoutIndex=NUM_TIMER1_STEPPING_VALUES-1;                // set slowest possible speed  
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses 
        TIFR1 |= (1 << TOV1);                                     // Reset all pending timer1 interrupts  
        TIMSK1 |= (1 << TOIE1);                                   // enable timer overflow interrupt  
        sei();  
        readerRun=0;
        debugPrintValue('.',PINA);  
        Serial1.write(PINA);                                     // over the feed hole already - Read in and write to serial buffer
        readerState = STATE_RUNNING_1;
        step(1);
      } 
      else {
        readerState = STATE_STOPPED_4;
        digitalWrite(STEPPER_POWER, STEPPER_OFF);
        TIMSK1 &= ~(1 << TOIE1);                                 // disable timer overflow interrupt
        notEmpty = 6;
        debugPrint('O');
      }
    }
    else if (event ==EVENT_FEED) {
      feed = !digitalRead(FEEDSWITCH);
      debugPrint('P');
      
    }
  }
  else if (readerState == STATE_RUNNING_1) {
    if (event == EVENT_RUN) {
      readerRun = 1;
      debugPrint('Q');
    }
    else if (event == EVENT_CLOCK) {
      if (readerRun) {
        readerRun=0;
        readerState = STATE_RUNNING_2;
        if (timeoutIndex) timeoutIndex--;
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses
        EIFR |= (1 << INTF2);
        EIMSK |= (1 << INT2);                                     // Re-enable external interrupt INT2
        sei();
        step(2);
        debugPrint('R');
      } else if (feed) {
        readerState = STATE_RUNNING_2;
        if (timeoutIndex) timeoutIndex--;
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses
        sei();
        step(2);
        debugPrint('S');
      }
      else {
        readerState = STATE_STOPPING_2;
        TCNT1 = TIMER1_VALUE_40MS;
        step(2);
        debugPrint('T');
      }     
    }
    else if (event == EVENT_FEED) {
      feed = !digitalRead(FEEDSWITCH);
      debugPrint('U');
    }
  }
  else if (readerState == STATE_RUNNING_2) {
    if (event == EVENT_RUN) {
      readerRun = 1;
      debugPrint('V');
    }
    else if (event == EVENT_CLOCK) {
      readerState = STATE_RUNNING_3;
      if (timeoutIndex) timeoutIndex--;
      TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses
      step(3);
      debugPrint('W');
    }
    else if (event == EVENT_FEED) {
      feed = !digitalRead(FEEDSWITCH);
      debugPrint(':');
    }
  }
  else if (readerState == STATE_RUNNING_3) {
    if (event == EVENT_RUN) {
      readerRun = 1;
      debugPrint('Y');
    }
    else if (event == EVENT_CLOCK) {
      if (readerRun) {
        readerRun=0;
        readerState = STATE_RUNNING_4;
        if (timeoutIndex) timeoutIndex--;
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses
        EIFR |= (1 << INTF2);
        EIMSK |= (1 << INT2);                                     // Re-enable external interrupt INT2
        sei();
        step(4);
        debugPrint('Z');
      } else if (feed) {
        readerState = STATE_RUNNING_4;
        if (timeoutIndex) timeoutIndex--;
        TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses
        sei();
        step(4); 
       debugPrint('!'); 
      }
      else {
        readerState = STATE_STOPPING_4;
        TCNT1 = TIMER1_VALUE_40MS;
        step(4);
        debugPrint('%');
      }
    }
    else if (event == EVENT_FEED) {
      feed = !digitalRead(FEEDSWITCH);
      debugPrint('&');
    }
  }
  else if (readerState == STATE_RUNNING_4) {
    if (event == EVENT_RUN) {
      readerRun = 1;
      debugPrint('?');
    }
    else if (event == EVENT_CLOCK) {
      readerState = STATE_RUNNING_1;
      if (timeoutIndex) timeoutIndex--;
      TCNT1 = steppingIntervals[timeoutIndex];                  // Set timeout value for generating pulses
      step(1);
      debugPrint('+');
    }
    else if (event == EVENT_FEED) {
      feed = !digitalRead(FEEDSWITCH);
      debugPrint('*');
    }
  }
}

// Stepper motor interrupt routine. 300 Hz
ISR (TIMER1_OVF_vect) {
  //sei();
  notEmpty--;
  readerFSM (EVENT_CLOCK);
}

void punchInt () {   
  if (Serial.available()) { 
    PORTC = Serial.read();    // Output the byte from the buffer.
    digitalWrite(PUNCH_DONE,1); // switch on feed hole and move forward solenoid
  }
  if (!digitalRead(PUNCH_FEED)) {
    digitalWrite(PUNCH_DONE,1);
  }
  TCNT3 = TIMER3_VALUE; // reset the timer value for the 10 ms timout
  TIFR3 |= (1 << TOV3) ;            // Reset all pending timer3 interrupts.
  TIMSK3 |= (1<<TOIE3); // enable timer 3 interrupts on overflow.
  EIMSK &= ~(1<< INT0) ; // disable punchInt
}

// The TIMER 3 ISR that switches the punch solenoids off.
ISR (TIMER3_OVF_vect) {
  PORTC = 0x00;          // switch off solenoids
  digitalWrite(PUNCH_DONE,0); // switch off feed hole and move forward solenoid
  TIMSK3 &= ~(1<<TOIE3); // Disable timer 3 interrupts
  EIMSK |= (1 << INT0);  // enable punch sync interrupts
}

void loop () {
  debugPrint('-');
  delay(5);
}


