/*
**************************************************************************  
Perlini Bottling System v3 Arduino MICRO Controller

RECENT CHAGNGES

10/29 ADDED PIN FOR 2ND PRESSURE SENSOR 
12/7  THIS FILE CONTAINS THE CURRENT PINOUTS FOR MICRO--I THINK

12/9 change
12/9 change

**************************************************************************  
*/


#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <math.h> //pressureOffset
#include "floatToString.h"

LiquidCrystal_I2C lcd(0x3F,20,4);  

const int button1Pin =  0;     // pin for button 1 B1 (Raise platform) RX=0; B1 on PBS is B3 on Touchsensor
const int button2Pin =  1;     // pin for button 2 B2 (Fill/purge)     TX=1; B2 on PBS is B2 on Touchsensor
// 2 SDA for LCD        2  
// 3 SDL for LCD        3
const int button3Pin =  4;     // pin for button 3 B3 (Depressurize and lower) B3 on PBS is B1 on Touchsensor

const int relay1Pin =   5;     // pin for relay1 S1 (liquid fill)
const int relay2Pin =   6;     // pin for relay2 S2 (bottle gas)
const int relay3Pin =   7;     // pin for relay3 S3 (bottle gas vent)
const int relay4Pin =   8;     // pin for relay4 S4 (gas in lift)
const int relay5Pin =   9;     // pin for relay5 S5 (gas out lift)
const int relay6Pin =  10;     // pin for relay6 S6 (door lock solenoid) //Oct 18

const int switch1Pin = 11;     // pin for fill switch1 F1 // 10/7 Zack changed from 13 to 7 because of shared LED issue on 13. Light3Pin put on 13
const int magnet1Pin = 12;     // pin for magnet switch
const int buzzerPin  = 13;     // pin for buzzer

const int sensor1Pin = A0;     // pin for preasure sensor1 P1
const int sensor2Pin = A1;     // pin for pressure sensor2 P2 ADDED 10/29
//                     A2      // future cleaning switch
const int light1Pin =  A3;     // pin for button 1 light 
const int light2Pin =  A4;     // pin for button 2 light
const int light3Pin =  A5;     // pin for button 3 light


// Inititialize button states
int button1State = 1;
int button2State = 1; 
int button3State = 1;        
//int light1State = 1;
//int light2State = 1;
//int light3State = 1;
int switch1State = 1; //Should this be high?
int magnet1State = 1;

int relay1State = HIGH; //TOTC
int relay2State = HIGH;
int relay3State = HIGH;
int relay4State = HIGH;
int relay5State = HIGH;
int relay6State = HIGH;

// Initialize variables
int P1 = 0;                         // Current pressure reading from sensor
int startPressure = 0;              // Pressure at the start of filling cycle
int pressureOffset = 55;            // changed from 37 t0 60 w new P sensor based on 0 psi measure
int pressureDeltaUp = 50;           // changed from 20 to 50 OCT 23 NOW WORKS
int pressureDeltaDown = 40;  
int pressureDeltaAutotamp = 250;    // This basically gives a margin to ensure that S1 can never open without pressurized bottle
int pressureNull = 450;             // This is the threshold for the controller deciding that no gas source is attached. 
float PSI = 0;
float startPressurePSI = 0;
float bottlePressurePSI = 0;

boolean inPlatformLoop = false;
boolean inFillLoop = false;
boolean inPressurizeLoop = false;
boolean inDepressurizeLoop = false;
boolean inPlatformLowerLoop = false;
boolean inPressureNullLoop = false;
boolean inOverFillLoop = false;
boolean inPurgeLoop = false;

boolean button3StateUp = false; //Boolean variables for B3 toggle state
boolean button3StateNew = LOW;  //Boolean variables for B3 toggle state

// Debug variables to be deleted later
int N = 0; // number of times through the mainloop;
int M = 0;

char buffer[25]; // just give it plenty to write out any values for flost to str (i dont think i need this anymore)

//FUNCTION: To simplify string output

String currentLcdString[3];
void printLcd (int line, String newString){
  if(!currentLcdString[line].equals(newString)){
    currentLcdString[line] = newString;
    lcd.setCursor(0,line);
    lcd.print("                    ");
    lcd.setCursor(0,line);
    lcd.print(newString);
  }
}

// FUNCTION: This was added so that the relay states could easily be changed from HI to LOW

void relayOn(int pinNum, boolean on){
  if(on){
    //turn relay on
    digitalWrite(pinNum, LOW);
  }
  else{
    //turn relay off
    digitalWrite(pinNum, HIGH);
  }
}

// FUNCTION: Routine to convert pressure from parts in 1024 to psi

float pressureConv(int P1) {
float pressurePSI;
// Subtract actual offset from P1 first before conversion:
// pressurePSI = (((P1 - pressureOffset) * 0.0048828)/0.009) * 0.145037738; //This was original equation
pressurePSI = (P1 - pressureOffset) * 0.078688; 
return pressurePSI;
}

void setup() {
  
  //setup LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); 
  lcd.print("Perlini Bottling");
  lcd.setCursor(0,1);
  lcd.print("System, version 1.0");
  delay(1000);  
  lcd.setCursor(0,3);
  lcd.print("Initializing...");
  delay(1000);

  Serial.begin(9600);
  
  //setup pins
  pinMode(relay1Pin, OUTPUT);      
  pinMode(relay2Pin, OUTPUT);
  pinMode(relay3Pin, OUTPUT);      
  pinMode(relay4Pin, OUTPUT);
  pinMode(relay5Pin, OUTPUT);  
  pinMode(relay6Pin, OUTPUT);  
  
  //pinMode(light1Pin, OUTPUT); //ADDED AT TOTC
  //pinMode(light2Pin, OUTPUT);
  //pinMode(light3Pin, OUTPUT);

  //set all pins to high which is "off" for this controller
  digitalWrite(relay1Pin, HIGH);
  digitalWrite(relay2Pin, HIGH);
  digitalWrite(relay3Pin, HIGH);
  digitalWrite(relay4Pin, HIGH);
  digitalWrite(relay5Pin, HIGH);
  digitalWrite(relay6Pin, HIGH);

  pinMode(button1Pin, INPUT);  //EDITED BUTTONS 1,2,3 TO PULLUP
  pinMode(button2Pin, INPUT);
  pinMode(button3Pin, INPUT);  
  pinMode(switch1Pin, INPUT_PULLUP); 
  pinMode(magnet1Pin, INPUT_PULLUP);
  
  pinMode(buzzerPin, OUTPUT); //Added Oct 16 for buzzer


  P1 = analogRead(sensor1Pin); // Initial pressure difference reading from sensor. High = unpressurized bottle
  startPressure = P1;  
  startPressurePSI = pressureConv(P1);
  
  
  Serial.print("Starting pressure: ");
  Serial.print (startPressurePSI);
  Serial.print (" psi");
  Serial.println("");
  
  Serial.print("Starting pressure: ");
  Serial.print (P1);
  Serial.print (" units");
  Serial.println("");
  

  //String output = "Pressure: " + String(startPressure); 
  //printLcd(3, output); 
  
  //FINALLY WORKS
  String (convPSI) = floatToString(buffer, startPressurePSI, 1);
  String (outputPSI) = "Pressure: " + convPSI + " psi";
  printLcd(3, outputPSI); 
  
  //========================================================
  // Check for stuck pressurized bottle or implausibly low gas pressure at start 
  //=========================================================
  
  while ( P1 < pressureNull ){
    relayOn(relay3Pin, true); //Open bottle vent
    relayOn(relay4Pin, true); //Keep platform up while depressurizing
    
    inPressureNullLoop = true;
    
    printLcd(0, "Bottle pressurized");
    printLcd(1, "Or gas off");
    
    P1 = analogRead(sensor1Pin); 
    
    bottlePressurePSI = pressureConv(P1);
    String (convPSI) = floatToString(buffer, bottlePressurePSI, 1);
    String (outputPSI) = "Pressure diff: " + convPSI;
    printLcd(3, outputPSI);       
    
  }  
  
    if ( inPressureNullLoop){
    //delay(1000);
    relayOn(relay4Pin, false); //drop platform
    relayOn(relay5Pin, true);
    P1 = analogRead(sensor1Pin); 
    startPressure = P1;  
    String output = "Pressure NEW: " + String(startPressure); 
    inPressureNullLoop = false;
  }
  
  delay(1000);
  lcd.setCursor(0,0);
  lcd.print("Insert bottle,     ");
  lcd.setCursor(0,1);
  lcd.print("Press button 1     ");
  
}

void loop(){

  // read the state of buttons and sensors
  button1State = !digitalRead(button1Pin); //BOOLEAN NOT added JUL 12
  button2State = !digitalRead(button2Pin); //BOOLEAN NOT added JUL 12
  button3State = !digitalRead(button3Pin); //BOOLEAN NOT added JUL 12
  switch1State = digitalRead(switch1Pin);
  magnet1State = digitalRead(magnet1Pin);
  delay(50);  

  if(magnet1State == HIGH){      //////////////////////////////////////////////
    relayOn(relay6Pin, false);
  }  
  else
  {
    relayOn(relay6Pin, true);
  }  
 
 
  // **************************************************************************  
  // BUTTON 1 FUNCTIONS 
  // **************************************************************************  

  // PLATFORM RAISING LOOP ****************************************************

  //while B1 is pressed, Raise bottle platform
  M = 0; // We're going to use this as a time counter. We'll do with pressure later.
  int counter = 25;
  
  while (button1State == LOW && P1 >= startPressure - pressureDeltaUp ){ // I.e. no bottle or bottle unpressurized
    // Added pressure condition to make sure this button doesn't act when bottle is pressurized--else could lower bottle when pressurized
    inPlatformLoop = true; 
    //printLcd(2,"Raising platform...");   
    //open S4
    relayOn(relay4Pin, true);
    relayOn(relay5Pin, false);
    digitalWrite(light1Pin, HIGH); //UNCOMMENTED
  
    
    M = M + 1;
    
    String numMLoops = "Raising... " + String(counter - M);
    if (M < counter) {
      printLcd(2, numMLoops); 
    }
    else
    {
      printLcd(0, "To fill bottle,"); 
      printLcd(1, "Press Button 2"); 
      printLcd(2, "");
      printLcd(2, "Ready to fill.");
    }
    //see if button is still pressed
    button1State = !digitalRead(button1Pin); 
  }  

  if (inPlatformLoop){
    if (M >= counter)
    {
    //run end actions of platformLoop
      relayOn(relay4Pin, true); // EW: Slight leak is causing platform to fall--leave open 
      relayOn(relay5Pin, false); // 
      //digitalWrite(light1Pin, HIGH); //TOTC
      relay4State = LOW;
    }  
    else    
    {
      relayOn(relay4Pin, false); // 
      relayOn(relay5Pin, true); // Drop platform
      //digitalWrite(light1Pin, LOW); //TOTC
      relay4State = HIGH;
      printLcd(2, "");
    }
    inPlatformLoop = false;
  }

  // **************************************************************************    
  // BUTTON 2 FUNCTIONS 
  // **************************************************************************

  // PRESSURIZE LOOP***********************************************************

  // While B2 is pressed, purge or pressurize the bottle
  
  while(button2State == LOW && (P1 >= pressureOffset + pressureDeltaUp) && relay4State == HIGH){  // Platform not up so no bottle, so must be purging
    inPurgeLoop = true;
    //relayOn(relay3Pin, false); //close S3 if not already
    relayOn(relay2Pin, true); //open S2 to purge
    //digitalWrite(light2Pin, HIGH); //TOTC
    button2State = !digitalRead(button2Pin); 
    delay(50);  
 
    printLcd(2,"Purging..."); 
  }
    if(inPurgeLoop){
    //close S2 when button lifted
    relayOn(relay2Pin, false);
    //digitalWrite(light2Pin, LOW); //TOTC

    inPurgeLoop = false;
    printLcd(2,""); 
  }
  
  
  
    /*  
    Serial.print("Current Pressure: ");
    Serial.print (P1);
    Serial.print (" units");
    Serial.println("");
    */  
 
  while(button2State == LOW && (P1 >= pressureOffset + pressureDeltaUp) && relay4State == LOW){ 
    inPressurizeLoop = true;
    
    relayOn(relay3Pin, true); //close S3 if not already
    relayOn(relay2Pin, true); //open S2 to pressurize
    //digitalWrite(light2Pin, LOW); //TOTC
      
    printLcd(2,"Pressurizing..."); 
    bottlePressurePSI = startPressurePSI - pressureConv(P1);
    String (convPSI) = floatToString(buffer, bottlePressurePSI, 1);
    String (outputPSI) = "Bottle Press: " + convPSI;
    printLcd(3, outputPSI);       
   
   
    Serial.print("Pressure Diff: ");
    Serial.print (P1);
    Serial.print (" units");
    Serial.println("");
  
    
    //do sensor reads and button check
    button2State = !digitalRead(button2Pin); 
    delay(50); //Debounce
    P1 = analogRead(sensor1Pin);
  }

  if(inPressurizeLoop){
    //close S2 when the presures are equal
    relayOn(relay2Pin, false);
    //digitalWrite(light2Pin, LOW); //TOTC
    inPressurizeLoop = false;
    
    printLcd(2,""); 
  }

  //FILL LOOP ***************************************************
  
  boolean button2StateUp = false; //Boolean variables for B2 toggle state
  boolean button2StateNew = LOW;
  
  pinMode(switch1Pin, INPUT_PULLUP); // 10-7 Zach
  //Serial.println("starting fill loop");
  
  while(button2State == LOW && switch1State == HIGH && P1 < pressureOffset + pressureDeltaUp + pressureDeltaAutotamp){ 
    
    pinMode(switch1Pin, INPUT_PULLUP); // 10-7 Zac
    //Serial.println("running fill loop");
    
    //while the button is still pressed, fill the bottle
    //open S1 and S3
    inFillLoop = true;
    //relayOn(relay3Pin, false); //Close S3 if not already
    relayOn(relay1Pin, true);
    //digitalWrite(light2Pin, HIGH); //TOTC

    relayOn(relay3Pin, true);
   
    printLcd(2,"Filling..."); 
    P1 = analogRead(sensor1Pin);
    
    float pressurePSI = pressureConv(P1);
    String (convPSI) = floatToString(buffer, pressurePSI, 1);
    String (outputPSI) = "Press diff: " + convPSI + " psi";
    printLcd(3, outputPSI); 
    delay(100);

    //see if B2 has been toggled
    button2StateNew = !digitalRead(button2Pin); 
    if (button2StateNew == HIGH && button2StateUp == false) 
    {
      button2StateUp = true; // User released button -- toggle "up" state of button
    }
    if (button2StateNew == LOW && button2StateUp == true)
    {
      button2State = HIGH; // User pushed button again, so exit loop
      button2StateUp = false;
    }      
    switch1State = digitalRead(switch1Pin); //Check fill sensor
  }

  if(inFillLoop){
    
    Serial.print("End Fill Loop Pres: ");
    Serial.print (P1);
    Serial.print (" units");
    Serial.println("");
    
    //either B2 was released, or FS1 was tipped/switched, or pressure dipped too much -- Autotamp in all three cases
    relayOn(relay1Pin, false);
    //digitalWrite(light2Pin, LOW); //TOTC
    
    relayOn(relay3Pin, false);
    printLcd(2,""); 
    //do the preasure loop again for the auto foam tamping

    while(P1 >= pressureOffset + pressureDeltaUp){
      inPressurizeLoop = true;
      //open S2 to equalize
      relayOn(relay2Pin, true);
      //digitalWrite(light2Pin, HIGH); //TOTC

      //do sensor read again to check
      P1 = analogRead(sensor1Pin);
      printLcd(2,"Repressurizing...");
      //delay(500); 
    }

    if(inPressurizeLoop){ 
      //close S2 when the preasures are equal
      relayOn(relay2Pin, false);
      //digitalWrite(light2Pin, LOW); //TOTC

      inPressurizeLoop = false; 
      printLcd(2,"");
    } 

    if(inFillLoop && switch1State == LOW) 
    {
      //inOverFillLoop = true; // Add functionality to make it impossible to fill after overfill
      relayOn(relay1Pin, true);
      relayOn(relay2Pin, true);
      //digitalWrite(light2Pin, HIGH); //TOTC
      
      printLcd(2,"Fixing Overfill..."); 
      delay(2500);
      printLcd(2,"");       
      relayOn(relay1Pin, false);
      relayOn(relay2Pin, false);
      //digitalWrite(light2Pin, LOW); //TOTC
      
      switch1State = digitalRead(switch1Pin); 
    }  
    else

    inFillLoop = false;
    printLcd(0, "Press Button 3");  
    printLcd(1, "To depressurize");  
  }
  switch1State = digitalRead(switch1Pin); 
  button2State = 1; // TEMP: THIS MAKES AUTOFILL!!


  // *********************************************************************
  // BUTTON 3 FUNCTIONS 
  // *********************************************************************

  // DEPRESSURIZE LOOP ***************************************************


  while(button3State == LOW && switch1State == HIGH && P1 <= startPressure - pressureDeltaDown){  // Don't need to take into account offset, as it is in both P1 and Startpressure
    inDepressurizeLoop = true;

    printLcd(2, "Depressurizing...");  
    //String output = "Pressure diff: " + String(P1);
    //printLcd(3,output);
    
    float pressurePSI = startPressurePSI - pressureConv(P1);
    String (convPSI) = floatToString(buffer, pressurePSI, 1);
    String (outputPSI) = "Bottle Press: " + convPSI;
    printLcd(3, outputPSI); 

    relayOn(relay3Pin, true);
    //digitalWrite(light3Pin, HIGH); //TOTC

    P1 = analogRead(sensor1Pin);
    //button3State = !digitalRead(button3Pin); // TEMP TO MAKE TOGGLE
    switch1State = digitalRead(switch1Pin); 

    //see if B3 has been toggled
    button3StateNew = !digitalRead(button3Pin); 
    if (button3StateNew == HIGH && button3StateUp == false) 
    {
      button3StateUp = true; // User released button first time-- toggle "up" state of button
    }
    if (button3StateNew == LOW && button3StateUp == true) 
    {
      button3StateNew = HIGH; // Toggle button state variables back
      button3StateUp = false;
      button3State = HIGH; // User pushed button again, so exit loop and set B3 HIGH
    }
  }

  if(inDepressurizeLoop){ 
    //If in this loop, button was released, or pressure is lowered, or sensor tripped. Find out which reason and fix.
    printLcd(2, "");

    relayOn(relay3Pin, false);     //Close bottle exhaust relay
    //digitalWrite(light3Pin, LOW); //TOTC

    if (button3State == HIGH)
    { 
      printLcd(2,"Repressurizing...");
      while(P1 >= pressureOffset + pressureDeltaUp)
      {
        relayOn(relay2Pin, true);         //open S2 to repressurize, probably to tamp down foam
        //digitalWrite(light2Pin, HIGH); //TOTC
        
        P1 = analogRead(sensor1Pin);
      }
      relayOn(relay2Pin, false); 
      //digitalWrite(light2Pin, LOW); //TOTC
      
      printLcd(2,"Repressurized");
      float pressurePSI = startPressurePSI - pressureConv(P1);
      String (convPSI) = floatToString(buffer, pressurePSI, 1);
      String (outputPSI) = "Bottle Press: " + convPSI;
      printLcd(3, outputPSI); 
      delay(500);      
    }

    if (switch1State == LOW)
    {
      printLcd(2, "Foam... wait");
      while(P1 >= pressureOffset + pressureDeltaUp)
      {
        relayOn(relay2Pin, true);         //open S2 to repressurize and tamp down foam
        //digitalWrite(light2Pin, HIGH); //TOTC
        
        P1 = analogRead(sensor1Pin);
      }  

      relayOn(relay2Pin, false);
      //digitalWrite(light2Pin, LOW); //TOTC

      delay(1000); // Wait a bit before accepting button inputs      
      printLcd(2, "Ready");
    }

    if (P1 >= startPressure - pressureDeltaDown){
      // If we reach here, the pressure must have reached threshold. Go to Platform lower loop
      // Close the cylinder input relay (this was opened in pressurize loop combat slow leak
      relayOn(relay3Pin, true); // may well leave on
      
      button3State = HIGH;
      printLcd(0,"To lower platform,");
      printLcd(1,"Press button 3");
      delay(500);
      
      digitalWrite(buzzerPin, HIGH); 
      delay(500);
      digitalWrite(buzzerPin, LOW);
      
      relayOn(relay4Pin, false);
      relayOn(relay5Pin, true);
      delay(1000);
      relayOn(relay5Pin, false);
      
    }  

  inDepressurizeLoop = false;
  }

  // PLATFORM LOWER LOOP *************************************************

  while(button3State == LOW && P1 > startPressure - pressureDeltaDown){
    //when pressure is close to 0, close S3 and lower platform with S5
    printLcd(2,"Lowering platform...");
    inPlatformLowerLoop = true;
    relayOn(relay3Pin, true); // May as well leave this open? YES. Liquid is still outgassing.
    relayOn(relay4Pin, false); // Need to close this if we got here by not going through depressurize loop--i.e., bottle was never pressurized
    relayOn(relay5Pin, true);
    //digitalWrite(light3Pin, HIGH); //TOTC
    relay4State = HIGH; //TOTC

    P1 = analogRead(sensor1Pin);
    button3State = !digitalRead(button3Pin);
    delay(50);
  }

  if(inPlatformLowerLoop){
    //close platform release 
    printLcd(2,"");
    relayOn(relay3Pin, false);
    relayOn(relay5Pin, false);
    //digitalWrite(light3Pin, LOW); //TOTC

    inPlatformLowerLoop = false;   

    //End of main loop cleanup

    printLcd(0,"Insert Bottle");
    printLcd(1,"Press button 1");

    P1 = analogRead(sensor1Pin); //If we've lowered the platform, assume we've gone through a cycle and store the startPressure again
    startPressure = P1;
    
    startPressurePSI = pressureConv(P1); 
    String (convPSI) = floatToString(buffer, startPressurePSI, 1);
    String (outputPSI) = "Reg. Press: " + convPSI + " psi";
    printLcd(3, outputPSI); 
    
    
    Serial.print("New Start pressure: ");
    Serial.print (P1);
    Serial.print (" units");
    Serial.println("");
  
    
    //digitalWrite(light1Pin, LOW); //TOTC
  } 
}

//**********************************************************************
// END
//**********************************************************************

/*

 CODE FRAGMENTS *********************************************************

/* THIS FUNCTION ADDED BY MATT
int ReadButton(const int buttonId)
{
  if (buttonId == button3Pin)
  {
    !digitaRead(buttonId);
  }
  else
  {
    digitalRead(buttonId);
  }
}
//example:
//button1State = ReadButton(button1Pin);
*/


   /*
  digitalWrite(light1Pin, HIGH);
  digitalWrite(light2Pin, HIGH);
  digitalWrite(light3Pin, HIGH);
  delay(500);
  digitalWrite(light1Pin, LOW);
  digitalWrite(light2Pin, LOW);
  digitalWrite(light3Pin, LOW);
  delay(500);
  digitalWrite(light1Pin, HIGH);
  digitalWrite(light2Pin, HIGH);
  digitalWrite(light3Pin, HIGH);
  delay(500);
  digitalWrite(light1Pin, LOW);
  digitalWrite(light2Pin, LOW);
  digitalWrite(light3Pin, LOW); 
  
  delay(500);
  */
  /*
  digitalWrite(light1Pin, HIGH);
  delay(200);
  digitalWrite(light2Pin, HIGH);
  delay(400);
  digitalWrite(light3Pin, HIGH);
  delay(800);
  digitalWrite(light1Pin, LOW);
  digitalWrite(light2Pin, LOW);
  digitalWrite(light3Pin, LOW);
  delay(500);
  digitalWrite(light1Pin, HIGH);
  delay(200);
  digitalWrite(light2Pin, HIGH);
  delay(400);
  digitalWrite(light3Pin, HIGH);
  delay(800);
  digitalWrite(light1Pin, LOW);
  digitalWrite(light2Pin, LOW);
  digitalWrite(light3Pin, LOW); 
  */
  
  //Make sure bottle isn't in stuck, pressurized state
  //relayOn(relay3Pin, true);
  //delay(4000);
  //relayOn(relay5Pin, true);
  //delay(2000);
  //relayOn(relay3Pin, false);
  //relayOn(relay5Pin, false);
  
  /*
  //This is for show--cycle platform
  delay(500);
  relayOn(relay4Pin, true);
  delay(1000);
  relayOn(relay4Pin, false);
  delay(500);
  relayOn(relay5Pin, true);
  delay(2000);  
  relayOn(relay5Pin, false);  
  
  //Test door lock and buzzer
  relayOn(relay6Pin, true);
  delay(1000);
  relayOn(relay6Pin, false);

  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
   
  */
  /*
  lcd.setCursor(0,2);
  lcd.clear();
  lcd.setCursor(0,2);
  lcd.print("Ready.");
  */
 
 



