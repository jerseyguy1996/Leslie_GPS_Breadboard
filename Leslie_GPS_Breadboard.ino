#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>
/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
 
  This example code is in the public domain.
 */
 

byte index = 0;
byte control = 2;
unsigned long last_data_received = 0; //is the GPS transmitting
boolean GPS_transmitting = false;
unsigned long last_valid_data = 0; //is the GPS transmitting valid data
boolean data_valid = false;
boolean buttonpress = true;
boolean last_button_press = false;
char buffer[90];
boolean sentenceBegins = false;
boolean data_index = false; //use this to alternate between processing RMC or GGA messages
char messageID[6];
char time[11];
char satsUsed[3];
char GPSstatus[2];
char date[9];
char Dummy[12];
signed long segdist = 0;
signed long distance = 0;
signed long last_latitude = 0;
signed long last_longitude = 0;
signed long latDegrees, longDegrees, latFract, longFract;
char lat_fract[7];
char long_fract[7];
char segdist_fract[5];
char dist_fract[5];
char n_s[2];
char e_w[2];
const int milesPerLat = 6900; //length per degree of latitude is 69 miles
  const int milesPerLong = 5253; //length per degree of longitude at
                                  //40.68 degrees of latitude is 52.53 miles.  It would
                                  //be better if I could put this conversion
                                  //in code so that it could be dynamically
                                  //calculated based on the location data from
                                  //the GPS.

//adafruit OLED stuff
#define OLED_DC 16
#define OLED_CS 18
#define OLED_CLK 15
#define OLED_MOSI 14
#define OLED_RESET 17
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);


#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

SoftwareSerial mySerial(3,4);


// the setup routine runs once when you press reset:
void setup() {                

  Serial.begin(115200);
  mySerial.begin(9600);
  digitalWrite(A5, HIGH);  //pin 28 on the PDIP
  pinMode(A5, OUTPUT);   //On/Off control on the GPS
  pinMode(2, INPUT);      //pin 4 on the PDIP
  digitalWrite(2, HIGH);//button control
  
 
  
    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(0,0);
  display.print("GPS Off");
  display.setCursor(70,0);
  display.print("Invalid");
  display.display();
  attachInterrupt(0, button_press, LOW);

  
}

// the loop routine runs over and over again forever:
void loop() 
{
  check_for_buttonpress();
  
  check_GPS_Status();
  
  check_for_updated_data();
}

void check_for_buttonpress()
{
  if(buttonpress)
  {
    Serial.println("button pressed");
    display.clearDisplay();
    display.setCursor(0,16);
    display.setTextSize(2);
    display.print("Stopping");
    display.display();
    delay(2000);
    digitalWrite(A5, HIGH);  turn off peripherals (A5 connected to the gate of a p-channel FET
    sleep_enable();
    attachInterrupt(0, button_press, LOW);  //ISR simply sets buttonpress to true
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    power_adc_disable();
    sleep_cpu();
    
    //code starts back up here after wake up
    sleep_disable();
    digitalWrite(A5, LOW);   //turn the peripherals back on
    power_adc_enable();
    display.begin(SSD1306_SWITCHCAPVCC);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0,16);
    display.print("Starting");
    display.display();
    delay(2000);
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0,0);
    display.print("GPS Off");
    display.setCursor(70,0);
    display.print("Invalid");
    display.display();
    index = 0;        //set everything back to zero because it is as if we are turning it on for the first time
    data_index = false;
    sentenceBegins = false;
    buttonpress = false;
    segdist = 0;
    distance = 0;
    last_latitude = 0;
    last_longitude = 0;
    attachInterrupt(0, button_press, LOW);
  }
}

//this just lets me know if I am receiving data from the GPS
//and if the data is valid or not.
void check_GPS_Status()
{
  if(timer(last_data_received))
  {
    if(GPS_transmitting)
    {
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      display.setCursor(0,0);
      display.print("GPS Off   ");
      display.display();
      GPS_transmitting = false;
    }
  }
  
  else
  
  {
    if(!GPS_transmitting)
    {
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      display.setCursor(0,0);
      display.print("GPS On   ");
      GPS_transmitting = true;
      display.display();
    }
  }
  
  if(timer(last_valid_data))
  {
    if(data_valid)
    {
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      display.setCursor(70,0);
      display.print("Invalid ");
      display.display();
      data_valid = false;
    }
  }
  
  else
  
  {
    if(!data_valid)
    {
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      display.setCursor(70,0);
      display.print("Valid    ");
      display.display();
      data_valid = true;
    }
  }
}

//if we have the correct NMEA string we can go ahead and update the display
void check_for_updated_data()
{
  if(checkforSentence()) 
  {
    Serial.println(buffer);
    if(Process_message() && strcmp(messageID, "GPGGA") == 0)
    {
      //if we go longer than 2 seconds without a valid fix then we will
      //show this on the display
      last_valid_data = millis() + 2000;
      //display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(WHITE, BLACK);
      display.setCursor(8,16);
//      display.print("Lat = ");
//      display.print(n_s);
//      display.print(latDegrees/1000000UL);
//      display.print(lat_fract);
//      display.print(latFract);
//      display.println("  ");
//      display.print("Long = ");
//      display.print(e_w);
//      display.print(longDegrees/1000000UL);
//      display.print(long_fract);
//      display.print(longFract);
//      display.println("  ");
//      display.setCursor(0,24);
//      display.print("Dist = ");
      display.print(distance/10000);
      display.print(dist_fract);
      display.print(distance - ((distance/10000)*10000));
      display.println("  ");
      display.display();
    }
  }
}


//just read in characters until we reach a $ sign.  Then start recording to
//buffer[] until we reach a /r
boolean checkforSentence()
{
  char c;
  while(mySerial.available())
  {
    last_data_received = millis() + 2000;
    c = mySerial.read();
    //Serial.print(c);
    
    if(sentenceBegins && c == '\r') //we have a full sentence
    {
      sentenceBegins = false;
      return true;
    }
    
    if(sentenceBegins) //store characters to buffer
    {
      buffer[index] = c;
      index++;
      buffer[index] = '\0';
    }
    
    if(c == '$') //beginning of sentence...start saving to buffer
    {
      sentenceBegins = true;
      index = 0;
    }
   
  }
  return false;
}

//function to break apart the NMEA strings into pieces that I can
//handle
const char* mytok(char* pDst, const char* pSrc, char sep = ',')
{
    while ( *pSrc )
    {
        if ( sep == *pSrc )
        {
            *pDst = '\0';

            return ++pSrc;
        }

        *pDst++ = *pSrc++;
    }

    *pDst = '\0';

    return NULL;
}


//function that does all of the work
boolean Process_message()
{
  char latit1[5];
  char latit2[6];
  char longit1[6];
  char longit2[6];
  char NS[2];
  char EW[2];
  
  const char*     ptr;
  
  //Serial.println(buffer);
  //check message ID to see what kind of message we got
  ptr = mytok(messageID, buffer);
  //Serial.println(messageID);
  
  //if it is GGA, read in the data and write to SDCard if
  //the data is valid and an SD file has been created
  if(strcmp(messageID, "GPGGA") == 0 && data_index==true)
  {
    ptr = mytok(time, ptr); if(ptr == NULL) return false;
    ptr = mytok(latit1, ptr, '.'); if(ptr == NULL) return false; //get the first half of latitude
    ptr = mytok(latit2, ptr); if(ptr == NULL) return false;//get the second half of latitude
    ptr = mytok(NS, ptr); if(ptr == NULL) return false;
    ptr = mytok(longit1, ptr, '.'); if(ptr == NULL) return false;
    ptr = mytok(longit2, ptr); if(ptr == NULL) return false;
    ptr = mytok(EW, ptr); if(ptr == NULL) return false;
    ptr = mytok(Dummy, ptr); if(ptr == NULL) return false;
    ptr = mytok(satsUsed, ptr); if(ptr == NULL) return false;
//    ptr = mytok(Dummy, ptr); if(ptr == NULL) return false;
//    ptr = mytok(MSLalt, ptr); if(ptr == NULL) return false;
//    ptr = mytok(Geoid, ptr); if(ptr == NULL) return false;
//    //ptr = mytok(GeoUnits, ptr); if(ptr == NULL) {Serial.println("Out13"); return;}
    


    if(atoi(satsUsed) < 4) return false;
    
    //we use this to preserve resolution in our fixed point math
    unsigned long multiplier = 1000000UL;
//    const int mins = 60;
//    unsigned long temp;

    
    latDegrees = (atoi(latit1))/100;  //isolate degrees
    latFract = (atoi(latit1)) - (latDegrees * 100);  //isolate whole minutes
    latDegrees *= multiplier;    //scale for fixed point math
    latFract *= multiplier;      //scale for fixed point math
    latFract += ((atoi(latit2))*100UL);  //add the fractions of a minute
    latFract = latFract/60;  //convert minutes to fractions of a degree
    latDegrees += latFract;
    
    longDegrees = (atoi(longit1))/100;  //isolate degrees
    longFract = (atoi(longit1)) - (longDegrees * 100);  //isolate whole minutes
    longDegrees *= multiplier;    //scale for fixed point math
    longFract *= multiplier;      //scale for fixed point math
    longFract += ((atoi(longit2))*100UL);  //add the fractions of a minute
    longFract = longFract/60;  //convert minutes to fractions of a degree
    longDegrees += longFract;
    
    //figure out how many zeros we need after the decimal
    if(latFract<10) {strcpy(lat_fract, ".00000");}
    else if (latFract<100) {strcpy(lat_fract, ".0000");}
    else if (latFract<1000) {strcpy(lat_fract, ".000");}
    else if (latFract<10000) {strcpy(lat_fract, ".00");}
    else if (latFract<100000) {strcpy(lat_fract, ".0");}
    else {strcpy(lat_fract, ".");}

    if(longFract<10) {strcpy(long_fract, ".00000");}
    else if (longFract<100) {strcpy(long_fract, ".0000");}
    else if (longFract<1000) {strcpy(long_fract, ".000");}
    else if (longFract<10000) {strcpy(long_fract, ".00");}
    else if (longFract<100000) {strcpy(long_fract, ".0");}
    else {strcpy(long_fract, ".");}
    
    if(NS[0] == 'S') strcpy(n_s, "-");

    if(EW[0] == 'W') strcpy(e_w, "-");
    
    //***********************
    //calculate the distance
    //***********************
    
    //if its the first valid reading just store it as last
    if(last_latitude == 0)
    {
      last_latitude = latDegrees;
      last_longitude = longDegrees;
    }

    if(!(last_latitude == 0)) //just check to make sure that it isn't the first reading
    //calculate distance of segment
    {
      long temp = 0;
      segdist = sqrt(pow(((latDegrees - last_latitude)*milesPerLat)/10000L,2) + pow(((longDegrees - last_longitude)*milesPerLong)/10000L,2));
      
      //this filters out some of the jitter/noise associated with GPS
      //sure we could just make the resolution courser when doing our
      //fixed point math but then I couldn't tell people that I can 
      //calculate out the distance to 4 decimal places :-)
      
      if(segdist<100)
      {
        segdist = 0;
      }
      temp = segdist - ((segdist/10000) * 10000); //isolate the fractional amount
      
      //figure out how many zeros we need after the decimal
      if(temp<10) {strcpy(segdist_fract, ".000");}
      else if (temp<100) {strcpy(segdist_fract, ".00");}
      else if (temp<1000) {strcpy(segdist_fract, ".0");}
      else {strcpy(segdist_fract, ".");}
      
      distance+=segdist;
      temp = 0;
      temp = distance - ((distance/10000)*10000);
      
      //figure out how many zeros we need after the decimal
      if(temp<10) {strcpy(dist_fract, ".000");}
      else if (temp<100) {strcpy(dist_fract, ".00");}
      else if (temp<1000) {strcpy(dist_fract, ".0");}
      else {strcpy(dist_fract, ".");}
      
      if(!(segdist<100))
      {
        last_latitude = latDegrees;
        last_longitude = longDegrees;
      }

      return true;


    }
    
    
    //add the segment distance to the total distance



  }
  
  if(strcmp(messageID, "GPRMC") == 0 && data_index == false)
  {
    char tempdate[7];

    
    ptr = mytok(time, ptr); if(ptr == NULL) return false;
    ptr = mytok(GPSstatus, ptr); if(ptr == NULL) return false;
    ptr = mytok(latit1, ptr, '.'); if(ptr == NULL) return false;//get the first half of latitude
    ptr = mytok(latit2, ptr); if(ptr == NULL) return false;//get the second half of latitude
    ptr = mytok(NS, ptr); if(ptr == NULL) return false;
    ptr = mytok(longit1, ptr, '.'); if(ptr == NULL) return false;
    ptr = mytok(longit2, ptr); if(ptr == NULL) return false;
    ptr = mytok(EW, ptr); if(ptr == NULL) return false;
    ptr = mytok(Dummy, ptr); if(ptr == NULL) return false;
    ptr = mytok(Dummy, ptr); if(ptr == NULL) return false;
    ptr = mytok(tempdate, ptr); if(ptr == NULL) return false;
    
  
    //GPSstatus tells us if data is valid
    if(GPSstatus[0] == 'V') return false;
    
    //re-order the date so that it will be in a nicer format for
    //sorting on the sd card
    date[0] = tempdate[4];
    date[1] = tempdate[5];
    date[2] = '_';
    date[3] = tempdate[2];
    date[4] = tempdate[3];
    date[5] = '_';
    date[6] = tempdate[0];
    date[7] = tempdate[1];
    date[8] = '\0';
    
    data_index = true; //We should have a valid date.  Now begin receiving GGA data
    return true;
  }
  return false;
}

//void wakeup()
//{
  //data_index = false;
  

//}

//void sleep()
//{
  //turn off gps
  //turn off oled and sd card
  //sleep processor
//}  
  
void button_press()
{
      sleep_disable();
      detachInterrupt(0);
      buttonpress = true;

}

//keep track of time and handle millis() rollover
boolean timer(unsigned long timeout)
  {
    return (long)(millis() - timeout) >= 0;
  }
