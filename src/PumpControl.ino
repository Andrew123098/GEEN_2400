/* Water Pressure Sensor Control System : Team Crunch : 2/23/2020 --> 4/25/2020
 *
 * The purpose of this program is to automate the water pump at Jacob Springs Farm. This is done using a pressure transducer
 * and two Particle Photon microcontrollers -- one at the pump to turn the pump on/off & one at the main house to take the
 * pressure data from his pipes. Below is a brief summary of how this program works.
 *
 * -> Reads analog data from a Pressure Transducer using a Particle Photon Wifi-Enabled Microcontroller.
 * -> Does statistical analysis on data in order to "smooth over" noise using a moving average.
 * -> Prints mean of most recent 30 data points to LCD screen through I2C Bus.
 * -> Turns on the water pump when water pressure indicates the water tanks are nearly empty.
 * -> Turns off the water pump when water pressure indicateds the water tanks are nearly full.
 * -> Attempts to minimize number of times the pump is turned on/off to extend pump useful life.
 * -> Provides override on/off functions for Andre via the Particle.io app.
 *
 *      *******PLEASE SEE _____________.INO FOR THE PUMP CONTROL PROGRAM*******
 *
 */

 // Include neccessary libraries.
 #include <LiquidCrystal_I2C_Spark.h>
 #include <math.h>

 //Declare Functions
 double getMean (double array[], int arrayLength);
 double getStdev (double array[], int arrayLength, double mean);
 void calibration();
 void sendCommandToPump (double mean);
 void printPumpStatus();
 void checkTimeAndPressure();
 int overrideOn(String command);
 int overrideOff(String command);

 //Define LCD Object
 LiquidCrystal_I2C *lcd;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialize Variables //
//////////////////////////
 #define SENSORPIN A5                     // Pin taking Data

 int interval = (1/12) * (60 * 1000);     // Interval between readings (First number is minutes)

 // For statistical analysis
 const int numData = 30;                  // Defines number of datapoints stored
 double recentData[numData];              // Array stores recent pieces of data
 double mean;                             // Stores the average of the recentData array
 double stdev;                            // Stres the standard deviation of the recentData array

 // Constraints
 int tankEmpty = 1600;                    // Analog value when tank is at minimum acceptable capacity (aka, turn on pump)
 int tankFull = 2100;                     // Analog value when tank is at maximum acceptable capacity (aka, turn off pump)

 // For data publishing & communication
 int analogVal = 0;                       // Stores analog values to be turned into a string for publishing
 bool pumpCommand = FALSE;                // Tracks pump status to be turned into a string for publishing (initialized to off)
 String sAnalogVal;                       // Stores analog values as a string for publishing
 String sMean;                            // Stores mean values as a string for publishing
 String sStdev;                           // Stores standard deviation values as a string for publishing
 String localPumpStatus = "OFF";          // Stores "ON" or "OFF" based the pump staus for printing to LCD
 String lastLocalPumpStatus;              // Stores the previous pump status for printing to LCD only when a change in status is detected

 // For timing
 unsigned long pumpTimer;                 // Used for timing how long the pump is ON.
 unsigned int maxTimeOn = 90 * 60 * 1000; // Pump should only be on for 90 minutes, max.

 // For Override timing
 unsigned long overrideUntil;              // Variable is equal to current time plus overrideTime
 int overrideTime = (1/6) * (60 * 1000);      // How long does the pump stay on when override function is called (90 minutes)
 bool checkTimeAndPressureVar = FALSE;     // Used to ensure pump turns off after Override Time or if it gets too full

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP FUNCTION:                   //
// -> Initialize various necessities //
// -> Initialize LCD screen w/ I2C   //
// -> Run Calibration function       //
// -> Calculate Mean of Values       //
// -> Calculate Standard Deviation   //
///////////////////////////////////////

void setup() {

    // Initialize various necessities
    pinMode(SENSORPIN, INPUT);                      // Define the analog pin as an input
    Particle.variable("analogVal", sAnalogVal);     // Set up IFTTT variable tracking
    Particle.variable("Pressure", mean);            // Track the mean variable in the Particle cloud.

    // Create override functions in Particle Cloud for Andre to call at any point from his phone.
    Particle.function("overrideOn", overrideOn);
    Particle.function("overrideOff", overrideOff);


    // Begin running the LCD Screen via I2C protocols
    lcd = new LiquidCrystal_I2C(0x3F, 16, 2);
     lcd->init();
     lcd->backlight();
     lcd->clear();


     // Run Calibration Function
     calibration();

     // Store Mean Value of Array
     mean = getMean(recentData, numData);

     // Store Standard Deviation of Array
     stdev = getStdev(recentData, numData, mean);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP FUNCTION:                              //
// Reads new data from the pressure transducer //
// If the new value is w/in X standard         //
// deviations of the mean, add it to the end   //
// of the array and create and publish the new //
// mean and standard deviation. Otherwise,     //
// quickly tske some more data and test again. //
/////////////////////////////////////////////////

void loop() {

    analogVal = analogRead(SENSORPIN);  // Define variable to store analog sensor data
    sAnalogVal = String(analogVal);     // Convert analog data to string for publishing

    unsigned long takeDataDelayTime;    // Stores time of previous data taking

    // If current time minus previous time of data taken is greater then our data taking interval...
    if (millis() - takeDataDelayTime >= interval || millis() < (1 * 1000)) {
        // If new data taken is w/in X standard deviations of the mean...
        if (mean- 1*stdev < analogVal && analogVal < mean + 2.5*stdev) {
            // Shifts entire array to the right, and enter enw value in the
            for (int i= numData; i> 0; i--) {
                recentData[i]= recentData[i-1];
                }
            recentData[0]= analogVal;
            // Get new mean and standard deviation
            mean = getMean(recentData, numData);
            stdev = getStdev(recentData, numData, mean);
            // Convert new mean and standard deviation to string for publishing
            sMean = String(mean);
            sStdev = String(stdev);

            // Print new PSI value to LCD screen
            lcd->clear();
            lcd->setCursor(0,0);
            lcd->print((mean/4096)*15);
            lcd->print(" PSI");

            // Publish new mean to the Particle Cloud
            Particle.publish("mean", sMean, PUBLIC);

            // Determine if new mean requires turning the pump ON/OFF and send new command to pump.
            sendCommandToPump(mean);

            // Set previous data time to current time
            takeDataDelayTime = millis();
        } else {
            // In 5 seconds, take a new reading.
            lcd->clear();
            lcd->setCursor(0,0);
            lcd->print("Out of Range");
            lcd->setCursor(0,1);                // Print pump status b/c delay (@ end of else statement) stops the status from updating the LCD.
            lcd->print("Pump Status: ");
            lcd->print(localPumpStatus);
            delay(5 * 1000);
        }

    }
    //Execute functions that do stuff intermittently and independently of the above if statement
    printPumpStatus();      // Prints the pump status if a change in status is detected
    checkTimeAndPressure(); // Makes sure if the pump is on, it does not overflow or stay on for more than the max set time limit
    syncTime();             // Refreshes onboard clock with Particle.io Cloud time
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CALIBRATION FUNCTION:                         //
// Fills the array used for statistical analysis //
// with data on startup and prints the data to   //
// the LCD display.                              //
///////////////////////////////////////////////////

void calibration () {

    //Print to the LCD Screen
    lcd->setCursor(0,0);
    lcd->print("Starting");
    lcd->setCursor(0,1);
    lcd->print("Calibration");
    delay(5 * 1000);


    //Store first 30 values in recentData array
    for(int i = 0 ; i <= 30 ; i++){

        recentData[i] = analogRead(SENSORPIN);
        lcd->clear();
        lcd->setCursor(0,0);
        lcd->print("Calibrating");
        lcd->setCursor(0,1);
        lcd->print((recentData[i]/4096)*15);
        lcd->print(" PSI");
        delay(1 * 1000);               //CHANGE UPON IMPLEMENTATION
    }

    //Print "DONE!" to the LCD screen
    lcd->clear();
    lcd->print("DONE!");
    delay(10 * 1000);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MEAN FUNCTION:                           //
// Takes in an array of a given length and  //
// outputs the Average of the array values. //
//////////////////////////////////////////////
double getMean (double array[], int arrayLength) {
     double sum = 0;
     for (int i = 0 ; i < numData ; i++) {
         sum += array[i];
     }
    return sum / arrayLength;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STANDARD DEVIATION FUNCTION:             //
// Takes in an array of a given length and  //
// the average of that array and returns    //
// its standard deviation.                  //
//////////////////////////////////////////////
double getStdev (double array[], int arrayLength, double mean) {
    double tempArray[arrayLength];
    for (int i = 0 ; i < arrayLength ; i++) {
        tempArray[i] = pow((array[i] - mean), 2);
    }
    return sqrt(getMean(tempArray, arrayLength));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TOGGLE PUMP FUNCTION:                  //
// Toggles the pump status based on mean  //
// and publishes the new pump status to   //
// the Particle cloud for communication   //
// with the pump Photon.                  //
////////////////////////////////////////////
void sendCommandToPump (double mean) {
    bool lastPumpCommand = pumpCommand;                                    // Keep track of previous pump command
    if (mean <= tankEmpty) {                                               // If the tank is empty (lower than our "tankEmpty" value)...
        pumpCommand = TRUE;                                                // Set pump to ON
        pumpTimer = millis();                                              // Start a timer
    } else if (millis() - pumpTimer >= maxTimeOn || mean >= tankFull) {    // If the pump has been on past the max time, OR  the tanks are full...
        pumpCommand = FALSE;                                               // Set pump to OFF
        pumpTimer = 0;                                                     // Turn off timer and reset to 0
    }

    if (pumpCommand != lastPumpCommand) {                                  // If new pump command is different from the previous pump command...
        if(pumpCommand == TRUE) {                                              // If command is to turn ON
            Particle.publish("turnPumpOn", PRIVATE);                           // Send command to turn the pump on
            localPumpStatus = "ON ";                                           // Set pump status to "ON" for printing to LCD
        } else {                                                               // Otherwise
            Particle.publish("turnPumpOff", PRIVATE);                          // Send command to turn the pump off
            localPumpStatus = "OFF";                                           // Set pump status to "OFF" for printing to LCD
        }
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OVERRIDE ON FUCNTION:                 //                    ***NOTE: Particle.fucntions require the input to be a string and the output to be an int.***
// Sends command to particle cloud to    //                                                (see documentation for more info)
// turn the pump on, but only for a      //
// set time or until the tanks are full. //
///////////////////////////////////////////
int overrideOn(String command) {
    if (mean < tankFull) {                                               // If tank is less than full...
        Particle.publish("turnPumpOn", PRIVATE);                         // Send command to turn the pump on
        localPumpStatus = "ON ";                                         // Set pump status to "ON" for printing to LCD
    }
    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Error here: else if statement not running

void checkTimeAndPressure() {
    if (lastLocalPumpStatus == "OFF" && lastLocalPumpStatus != localPumpStatus && checkTimeAndPressureVar == FALSE) {       // If the pump just turned on & first time calling function...
        overrideUntil = millis() + overrideTime;                                                                            // Sets time in the future for when pump should turn off
        checkTimeAndPressureVar = TRUE;                                                                                     // Makes sure we cannot go through this if statement again
    } else if ((millis() > overrideUntil || mean >= tankFull) && checkTimeAndPressureVar == TRUE) {                         // If current time exceeds max-pump-on time, or tank is full & and we have gone through the first if statement...
        overrideOff("");                                                                                                    // Call override off function (which turns off the pump)
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OVERRIDE OFF FUCNTION:              //
// Sends command to particle cloud to  //
// turn the pump off.                  //
/////////////////////////////////////////
int overrideOff(String command) {
    Particle.publish("turnPumpOff", PRIVATE);       // Send command to turn the pump off
    localPumpStatus = "OFF";                        // Set pump status to "OFF" for printing to LCD
    checkTimeAndPressureVar = FALSE;                // Make sure we can go through checkTimeAndPressure if statement again
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SYNC TIME FUNCTION:                       //
// Syncs the onboard clock with the Particle //
// cloud clock every 3 hours.                //
///////////////////////////////////////////////
void syncTime() {
    unsigned long refreshTime;                      // Initialize variable to store time ever 3 hours
    if (millis() - refreshTime > 3600000) {         // If current time minus previous time of sync is greater than 3 hours...
        Particle.syncTime();                        // Refresh the onboard clock with the Particle.io cloud realtime clock
        refreshTime = millis();                     // Set refreshTime to current time
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PRINT PUMP STATUS FUNCTION:                //
// Prints the pump status to the LCD screen   //
// if a change in status is detected.         //
////////////////////////////////////////////////
void printPumpStatus() {
    if (localPumpStatus != lastLocalPumpStatus) {   // If the current status is different from the previous status...
        lcd->setCursor(0,1);
        lcd->print("Pump Status: ");
        lcd->print(localPumpStatus);
        lastLocalPumpStatus = localPumpStatus;
    }
}
