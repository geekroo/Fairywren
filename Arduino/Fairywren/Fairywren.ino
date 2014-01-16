/*********************************************************************************
 * Geekroo Technologies - http://geekroo.com
 * 
 * Farywren 1.0
 * Farywren.ino     BY Herman.Liu
 *   
 * This program is based on "ARDEX 0.3"  by BLAVERY
 * Please refer to https://github.com/larsks/ardex
 *
 * | Using a Arduino clone(Nano) to do rPi I/O hardware interface expansion.
 * |
 * | Allow Pi (i2c master) talking to arduino (i2c slave) to control 3 Arduino IO pins.
 * | (Very modest smarts in the arduino. Real executive work still to do on the Pi!)
 * | Available:  d9-d11 digital i/o 
 * |            a0-a2 as analog in.
 * | Optionally, 3 pins can do PWM, is commonly RGB LED,
 * |
 * | Write Commands:
 * | M  NN  mode        Pin# NN (8-10) Set pin mode. Mode 'O'=output
 * | D  NN  W on.off    Pin# NN (8-10) Digital output control. even=off/LOW odd=on/HIGH   eg '0' & '1'
 * | P  NN  W val8      Write PWM value (pins 3 5 6 9 10 only)
 * |
 * | Read Preparation Commands:
 * | D  NN  R          Place digital value (1 byte) for pin NN into buffer for reading
 * | A  nn  R          Analog control. Read analog pin# nn (0-2) into buffer.
 * |
 * |
 * | Send by i2c as:  cmd  count  pin  job  param1  param2
 * | the extra byte "count" (array size, ie usually 4) comes from smbus.write_block_data()
 * | "param2" is not currently used
 * |
 * |
 * | Reads:
 * | (Read1)           Fetch buffer count. Buffer16 ready for read yet? (Loop here until non zero)
 * | (Read2)           Send back buffer16[8-10] 
 *
 *********************************************************************************
 */
#include "Analog.h"
#include "ibuf.h"

#include <Wire.h>
#include <Metro.h>

// Serial commands
#define CMD_SETPINMODE 'M'
#define CMD_DIGITAL    'D'
#define CMD_ANALOG     'A'
#define CMD_STATE      'S'

/*
 * not currently implemented:
#define CMD_PWM        'P'
#define CMD_SETCOLOUR  'C'
*/    

#define MODE_INPUT    'I'
#define MODE_OUTPUT   'O'

#define STATE_READY    'r'
#define STATE_WTD      'f'
#define STATE_POWEROFF 'p'

// There is no MODE_INPUT. All pins exc 13 are input by default.
// Philosophy is that rPi sets pins as required JUST ONCE per ArdEx reset, not swapping around!

#define JOB_WRITE      'W'
#define JOB_READ       'R'

#define I2C_SLAVE_ADDR  0x05   // i2c slave address (38)
#define buttonPin   4          // the number of the pushbutton pin
#define PWR_EN   7
#define PWR_OK   2
#define irPin   8
#define LED_R   5
#define LED_G  6
#define LED_B  3

int buffer16 = 0;            // results for read request, waiting to transmit to rPi
char readyflag = 0;          // buffer count available for reading by rPi. zero if not ready
char req_analog_cap = 0x55;  // 0x55 = idle.  0-2 = capture pending for that analog pin
char bufpointer = 0;         // 0 means readyflag will tx.  1,2 = byte of buffer16 will transmit next

// variables will change:
int buttoncont = 0; 
int sensorcont = 0;  
int ledcount = 0;
int pwmcount = 1;
boolean ledon = false;
boolean power_state = false;

boolean safe_power = false;
boolean pi_ok = false;
boolean pi_cls = false;
int picont = 0; 

int byteValue = 0;

int requestcmd = 0;

unsigned long lengthHeader;
unsigned long bitt;

Analog analog;                     // background analog read system
I2Cbuffer i2cBuffer;               // Holds incoming i2c packets until convenient to process.

Metro ledMetro = Metro(50);
Metro buttonMetro = Metro(100);
Metro checkMetro = Metro(1000);
Metro piclsMetro = Metro(100);

void initstate()
{
  boolean power_state = false;
  boolean safe_power = false;
  boolean pi_ok = false;
  boolean pi_cls = false;
  //Wire.begin(I2C_SLAVE_ADDR);      // init I2C Slave mode
  //delay(100);
}
void poweron()
{  
  digitalWrite(PWR_EN,1);
  power_state = true;
}

void poweroff()
{  
  digitalWrite(PWR_EN,0);
  power_state = false;
}

void setup()  
{
   //wdt_disable();
   // initialize the serial communications:
  Serial.begin(9600);
  Serial.println("Fairywren v0.1");   
  Serial.println ("Arduino Hardware Expansion Slave.") ;
  for (int pin = 8 ; pin < 11 ; ++pin)
  {
    digitalWrite (pin, LOW) ;
    pinMode (pin, INPUT) ;
  } 
    
  pinMode (A0, INPUT) ;
  pinMode (A1, INPUT) ;
  pinMode (A2, INPUT) ;  
  
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(PWR_EN, OUTPUT);
  digitalWrite(PWR_EN, LOW);
  pinMode(buttonPin, INPUT_PULLUP);
  //pinMode(buttonPin, INPUT);
  pinMode(irPin,INPUT); 
  //digitalWrite(LEDPin, HIGH);
   
  
  power_state = false;  
  initstate();
  Wire.begin(I2C_SLAVE_ADDR);   // init I2C Slave mode
  Wire.onRequest(RequestEvent); //register I2C TX event ISR
  Wire.onReceive(ReceiveEvent); //register I2C RX event ISR
  //attachInterrupt(0,1,LOW);
  //attachInterrupt(1,2,LOW);
  //delay(100);
}

void loop() // run over and over
{ 
  if (ledMetro.check() == 1) 
  {
    if(power_state==false) 
    {
      ledcount=1;
      setColor(0, 0, 0);  // no color 
    }
    else LEDflash();
  }   
  
  if(buttonMetro.check() == 1)
  {
    ButtonCheck();
  }  
  
  if (piclsMetro.check() == 1) 
  {
    RpiCheck();
  }
  
  analog.run(); 
  processI2c_Incoming();    // i2c commands queuedS
}

void LEDflash()
{
   switch(ledcount) {
      case 1:
        if(pwmcount>=255){
           ledcount++;
           setColor(pwmcount, 0, 0);  // red
           pwmcount = 1;
        }
        else {
          setColor(pwmcount, 0, 0);  // red
          pwmcount++;
         }
        break;
      case 2:
        if(pwmcount>=255){
           ledcount++;
           setColor(0, pwmcount, 0);  // green
           pwmcount = 1;
        }
        else {
          setColor(0, pwmcount, 0);  // green
          pwmcount++;
         }
        break;
      case 3:
        if(pwmcount>=255){
           ledcount++;
           setColor(0, 0, pwmcount);  // blue
           pwmcount = 1;
        }
        else {
          setColor(0, 0, pwmcount);  // blue
          pwmcount++;
         }
        break;
      case 4:
        if(pwmcount>=255){
           ledcount++;
           setColor(pwmcount, pwmcount, 0); 
           pwmcount = 1;
        }
        else {
          setColor(pwmcount, pwmcount, 0);
          pwmcount++;
         }
        break;
      case 5:
        if(pwmcount>=255){
           ledcount++;
           setColor(pwmcount, 0, pwmcount); 
           pwmcount = 1;
        }
        else {
          setColor(pwmcount, 0, pwmcount);
          pwmcount++;
         }
        break;
      case 6:
        if(pwmcount>=255){
           ledcount++;
           setColor(0, pwmcount, pwmcount); 
           pwmcount = 1;
        }
        else {
          setColor(0, pwmcount, pwmcount);
          pwmcount++;
         }
        ledcount=1;
        break;
      default:
        ledcount=1;
        setColor(0, 0, 0);  // no color 
        break;
    }  
}
  
void ButtonCheck()
{
  if (digitalRead(buttonPin) == 0)
    { 
       if((buttoncont>=3)&&(safe_power== false)&&(power_state==true)&&(pi_ok==true))
          {  
             safe_power = true;               
             buttoncont=0; 
             Serial.println("safe_power");
          }
        if((buttoncont>10)&&(power_state==false))
         {  
             initstate();
             poweron();
             buttoncont=0;
             Serial.println("power_on");
         }
      if((buttoncont>=30)&&(power_state==true))
          {
           pi_ok=false;
           safe_power = false;
           poweroff();
           buttoncont=0;
           Serial.println("power_off");  
          }
          /*
       if((buttoncont>=30)&&(power_state==false))
         {  
             initstate();
             poweron();
             buttoncont=0;
         }*/
     else
      {  
        buttoncont++;  
        //digitalWrite(PWR_EN, HIGH);
      }  
    }
  else buttoncont=0; 
}

void RpiCheck()
{
  if((pi_ok==true)&&(power_state==true)&&(pi_cls==false))
      {  
         picont++;   
      }
    else 
     {
        pi_cls=false;
        picont=0;
     }
   if (picont>=300)
     {
        if (safe_power==true)
        {
           pi_ok=false;
           safe_power = false; 
           poweroff();
           Serial.println("Power_safe_down");
           picont=0;
        }
        else ;
       
     }
   
}

// event service function that executes when i2c data packet was received from master (rPi). 
void ReceiveEvent(int howMany)
{
    unsigned char job;
    // the bytes are queued for processing later, either to usual buffer or to special lcd buffer
    job = i2cBuffer.put();
    if(job == 'R' || job == 'L')
        // ie read of any kind (incl logical analog read)
        readyflag=0;
        // readyflag MUST be put off immediately, before we exit the interrupt service.
        // readyflag goes >0 only when its fetching routine loads buffer16 with return value;
    return;

}

void processI2c_Incoming(void)
{
    // This processes one (buffered) 5 byte incoming I2C data packet, if available.
   
    unsigned char pin, cmd, job, val1, val2;

    //    exit if buffer empty
    if (! i2cBuffer.get(&cmd, &pin, &job, &val1, &val2)) // extract from buffer.
        return;
    switch (cmd)
    {
      case CMD_SETPINMODE:
        if ((pin < 9) || (pin > 10))
        break;
        switch (job)   
          {
            case MODE_INPUT:
              pinMode (pin, INPUT) ;
              break;

            case MODE_OUTPUT:
              pinMode (pin, OUTPUT) ;    
              break;
          }
        break;
      
      case CMD_DIGITAL:   
        if ((job == JOB_READ) && (pin >= 8) && (pin <= 10))
        {
            buffer16 = digitalRead(pin) << 8;
            readyflag=1;
            //Serial.print("READ_DIGITAL:");
            //Serial.println(val1);
            break;
        }
        if ((job == JOB_WRITE) && (pin >= 8) && (pin <= 10))
           digitalWrite (pin, val1 & 1) ;
           //Serial.print("WRITE_DIGITAL:");
           //Serial.println(val1);
       break;

    case CMD_ANALOG:
    // uses a0-2 numbering !
      if ((pin > 2) && (pin <0))
      break;
      switch (job)
        {
          case JOB_READ:
            buffer16 = analog.read(pin);
            //buffer16 = analogRead(pin);
            // this is maintained by analog class
            readyflag=2;
            break;       
        }
      break;     
    case CMD_STATE:
      switch(job)
        {
          case 0:
            //Serial.println("STATE_TEST"); 
            break;
         case STATE_READY:
           pi_ok = true;
           //Serial.println("STATE_READY");  
           break;
         case STATE_WTD:
           pi_cls = true;
           //Serial.println("STATE_WTD");
           break;
         case STATE_POWEROFF:
           if (safe_power==true)
            { 
              buffer16 = 0x55;
              Serial.println("STATE_POWEROFF");      
              readyflag = 2;
            }
           break;
        }
      break;
    while (Wire.available()>0) Wire.read();    // flush away anything left? Should be nothing.
    return;
 }
}
 void RequestEvent()
{
    // rPi has requested a byte of data   
    // bufpointer (0 or 1 or 2) is the controller of what byte is sent back. (state machine)
    // initially (bufpointer=0) only readyflag is returned. If "not ready" then next write will be readyflag again.
    // if "ready" then bufpointer is incremented, and the following 2 writes will be the buffer16 halves
    //Serial.print("RequestEvent:"); 
    if (readyflag > 0)   // buffered data is ready for reading by rPi
        bufpointer = 0;
        
    switch (bufpointer)
    {
      case 0:
        Wire.write(readyflag);
        if (readyflag > 0)   // only get one chance to read this flag
        {
            bufpointer = 1;
            readyflag = 0;
        }       
        break;

      case 1:
        Wire.write(highByte(buffer16));
        bufpointer = 2;
        break;

      case 2:
        Wire.write(lowByte(buffer16));
        bufpointer=0;

      default:
        bufpointer = 0;
        readyflag = 0;
        break;     

    }    
}
void setColor(int red, int green, int blue)
{
  analogWrite(LED_R, red);
  analogWrite(LED_G, green);
  analogWrite(LED_B, blue);  
}

 
 

