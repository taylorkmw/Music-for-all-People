#include <PololuLedStrip.h>

// Create an ledStrip object and specify the pin it will use.
PololuLedStrip<12> ledStrip;

// Create a buffer for holding the colors (3 bytes per color).
#define LED_COUNT 150

rgb_color colors[LED_COUNT];
rgb_color off;

uint16_t brightness;





//clipping indicator variables
boolean clipping = 0;

//data storage variables
byte newData = 0;
byte prevData = 0;
unsigned int time = 0;//keeps time and sends vales to store in timer[] occasionally
int timer[10];//sstorage for timing of events
int slope[10];//storage for slope of events
unsigned int totalTimer;//used to calculate period
unsigned int period;//storage for period of wave
byte index = 0;//current storage index
float frequency;//storage for frequency calculations
int maxSlope = 0;//used to calculate max slope as trigger point
int newSlope;//storage for incoming slope data

//variables for decided whether you have a match
byte noMatch = 0;//counts how many non-matches you've received to reset variables if it's been too long
byte slopeTol = 3;//slope tolerance- adjust this if you need
int timerTol = 10;//timer tolerance- adjust this if you need

//variables for amp detection
unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;
byte ampThreshold = 10;//raise if you have a very noisy signal

void setup(){
  // Off.
  off.red = 0;
  off.green = 0;
  off.blue = 0;

  // Initialize all LEDs to off.
  for(uint16_t i = 0; i < LED_COUNT; i++) {
    colors[i] = off;
  }

  
  
  Serial.begin(9600);
  
  pinMode(13,OUTPUT);//led indicator pin
  pinMode(12,OUTPUT);//output pin
  
  cli();//diable interrupts
  
  //set up continuous sampling of analog pin 0 at 38.5kHz
 
  //clear ADCSRA and ADCSRB registers
  ADCSRA = 0;
  ADCSRB = 0;
  
  ADMUX |= (1 << REFS0); //set reference voltage
  ADMUX |= (1 << ADLAR); //left align the ADC value- so we can read highest 8 bits from ADCH register only
  
  ADCSRA |= (1 << ADPS2) | (1 << ADPS0); //set ADC clock with 32 prescaler- 16mHz/32=500kHz
  ADCSRA |= (1 << ADATE); //enabble auto trigger
  ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN); //enable ADC
  ADCSRA |= (1 << ADSC); //start ADC measurements
  
  sei();//enable interrupts
}
double loudness = 0.0;
ISR(ADC_vect) {//when new ADC value ready
  
  PORTB &= B11101111;//set pin 12 low
  prevData = newData;//store previous value
  newData = ADCH;//get value from A0
  //Serial.println(newData);
  if (prevData < 127 && newData >=127){//if increasing and crossing midpoint
    newSlope = newData - prevData;//calculate slope
    if (abs(newSlope-maxSlope)<slopeTol){//if slopes are ==
      //record new data and reset time
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){//new max slope just reset
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
        index++;//increment index
        loudness = abs(newData - 127) * 0.5 + loudness*0.5;
      }
      else if (abs(timer[0]-timer[index])<timerTol && abs(slope[0]-newSlope)<slopeTol){//if timer duration and slopes match
        //sum timer values
        totalTimer = 0;
        for (byte i=0;i<index;i++){
          totalTimer+=timer[i];
        }
        period = totalTimer;//set period
        brightness = int(loudness);
        //reset new zero index values to compare with
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1;//set index to 1
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
        //loudness = 0.0;
      }
      else{//crossing midpoint but not match
        index++;//increment index
        if (index > 9){
          reset();
        }
      }
    }
    else if (newSlope>maxSlope){//if new slope is much larger than max slope
      maxSlope = newSlope;
      time = 0;//reset clock
      noMatch = 0;
      index = 0;//reset index
      loudness = abs(newData - 127) * 0.3 + loudness*0.7;
    }
    else{//slope not steep enough
      noMatch++;//increment no match counter
      if (noMatch>9){
        reset();
      }
    }
  }
    
  if (newData == 0 || newData == 1023){//if clipping
    PORTB |= B00100000;//set pin 13 high- turn on clipping indicator led
    clipping = 1;//currently clipping
  }
  
  time++;//increment timer at rate of 38.5kHz
  
  ampTimer++;//increment amplitude timer
  if (abs(127-ADCH)>maxAmp){
    maxAmp = abs(127-ADCH);
  }
  if (ampTimer==1000){
    ampTimer = 0;
    checkMaxAmp = maxAmp;
    maxAmp = 0;
  }
  
}

void reset(){//clea out some variables
  index = 0;//reset index
  noMatch = 0;//reset match couner
  maxSlope = 0;//reset slope
}


void checkClipping(){//manage clipping indicator LED
  if (clipping){//if currently clipping
    PORTB &= B11011111;//turn off clipping indicator led
    clipping = 0;
  }
}

double current = 0.0;
double currentLow = 0.0;
double currentHigh = 0.0;
double contenderLow = 0.0;
double contender = 0.0;
double contenderHigh = 0.0;
int contenderCount = 0;
int bin = 1;
int prevBin = 20;
void loop(){
  checkClipping();
  
  if (checkMaxAmp>ampThreshold){
    frequency = 38462/float(period);//calculate frequency timer rate/period
    //Serial.println(frequency);
    if (frequency > 40 && frequency < 1500) {
      //print results
      if (frequency > currentLow && frequency < currentHigh) {
        current = current*0.8 + frequency*0.2;
        currentLow = current - current*0.12;
        currentHigh = current + current*0.12;
        contender = current;
        contenderLow = currentLow;
        contenderHigh = currentHigh;
        contenderCount = 0;
      } else {
        if (frequency > contenderLow && frequency < contenderHigh) {
          contenderCount += 1;
          //Serial.print("contender: ");
          //Serial.println(contender);
          if (contenderCount >= 2) {
            contenderCount = 0;
            current = contender;
            currentLow = contenderLow;
            currentHigh = contenderHigh;
          } else {
            contender = contender*0.8 + frequency*0.2;
            contenderLow = contender - contender*0.12;
            contenderHigh = contender + contender*0.12;
          }
        } else {
            contender = frequency;
            contenderLow = contender - contender*0.12;
            contenderHigh = contender + contender*0.12;
            contenderCount = 1;
        }
      }
      
    }
    
  }
  
  //delay();//delete this if you want

  //do other stuff here
  
  // incoming audio is newData
  
  if (loudness < 1) {
    brightness = 0;
  } else {
    brightness = 40 + int((loudness * 2.7));
  }
  //Serial.println(brightness);
  
  // Given frequency, determine color weightings.
  prevBin = bin;
  bin = int(min(double(current - 40.0) / 1000 * 255, 255.0)*0.8 + prevBin*0.2);
//  Serial.println(bin);
//  Serial.print(current);
//  Serial.println(" hz");
  rgb_color test = Wheel(bin, brightness);
//  Serial.print(test.red);
//  Serial.print(" ");
//  Serial.print(test.green);
//  Serial.print(" ");
//  Serial.println(test.blue);
  
//  float red_weight = 0;
//  float green_weight = 0;
//  float blue_weight = 0;
//  if(max_bin < 120) {
//    red_weight = 1;
//  } else if(max_bin < 240) {
//    red_weight = -((float)1/365) * max_bin + 2;
//    green_weight = ((float)1/365) * max_bin - 1;
//  } else if(max_bin < 550) {
//    green_weight = -((float)1/365) * max_bin + 3;
//    blue_weight = ((float)1/365) * max_bin - 2;
//  } else {
//    blue_weight = 1;
//  }

  // Determine colors[1:]
  for(uint16_t i = LED_COUNT - 1; i > 0; i--) {
    colors[i] = colors[i - 1];    
  }

  // Determine colors[0]
//  rgb_color c;
//  c.red = (uint16_t)(red_weight * brightness);
//  c.green = (uint16_t)(green_weight * brightness);
//  c.blue = (uint16_t)(blue_weight * brightness);
//  if(c.red > 0 || c.green > 0 || c.blue > 0) {
//
//    Serial.print(c.red);
//    Serial.print(" ");
//    Serial.print(c.green);
//    Serial.print(" ");
//    Serial.println(c.blue);
//  }

  colors[0] = test;
  
  ledStrip.write(colors, LED_COUNT);
}


rgb_color Wheel(int pos, int b) {
  rgb_color rv;
  if (pos < 23) {
    rv.red = int(pos*2.5);
    rv.green = 0;
    rv.blue = int(pos*2.75);
  } else if (pos < 76) {
    rv.red = 86 - pos;
    rv.green = 10;
    rv.blue = 110 + int(pos*1.25);
  } else if ( pos < 129) {
    rv.red = int(pos*0.12);
    rv.blue = 260 - pos*2;
    rv.green = int(pos * 2.5) - 200;
  } else if (pos < 172) {
    rv.red = int(pos * 5) - 650;
    rv.blue = 10;
    rv.green = 240;
  } else if (pos < 220) {
    rv.red = int(pos * 5) - 650;
    rv.blue = 10;
    rv.green = 660 - pos*3;
  } else {
    rv.red = 200;
    rv.blue = 10 + (pos - 260)*8;
    rv.green = 160 + (pos - 260) * 3;
  }
   int s = rv.red + rv.green + rv.blue;
   rv.red = rv.red * b / s;
   rv.green = rv.green * b / s;
   rv.blue = rv.blue * b / s;
   return rv;
}

