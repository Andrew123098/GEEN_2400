/* Water Pressure Sensor Control System : Team Crunch : 2/23/2020 --> 4/25/2020
 *
 * The purpose of this program is to automate the water pump at Jacob Springs Farm. This is done using a pressure transducer
 * and two Particle Photon microcontrollers -- one at the pump to turn the pump on/off & one at the main house to take the
 * pressure data from his pipes.
 *
 * ***NOTE: This program is an extension of _______________.INO -- Please see main program for more information.***
 *
 * -> Subscribes to events on Particle.io Cloud which turn the water pump ON/OFF
 * -> Tracks the status of the water pump and uploads status to Particle.io Cloud.
 * -> Turns water pump ON/OFF
 *
 */

 // Define which pin the pump is connected to. (Controls the relay which controls the contactor which controls the pump)
 #define PUMP_PIN  0

 String pumpStatus;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP FUNCTION:                                       //
// This function does 4 things:                          //
// -> Initializes the pump status for Particle.io Cloud. //
// -> Subscribes to events on Particle.io Cloud which    //
//    turn the water pump ON/OFF.                        //
// -> Initializes the pump pin as an Output.             //
///////////////////////////////////////////////////////////
void setup() {
    // Initialize variable for tracking on Particle.io Cloud
    Particle.variable("pumpStatus", pumpStatus);

    // Subscibe to pump command events in the Particle Cloud
    Particle.subscribe("turnPumpOn", pumpOn, MY_DEVICES);         // Execute pumpOn function, when "turnPumpOn" event is published
    Particle.subscribe("turnPumpOff", pumpOff, MY_DEVICES);       // Execute pumpOff function, when "turnPumpOff" event is published

    // Initialize the pump pin on the Photon as an Output pin.
    pinMode(PUMP_PIN, OUTPUT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP FUNCTION:                //
// Does not need to do anything. //
///////////////////////////////////
void loop() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUMP ON FUNCTION:                                  //    ***NOTE: The arguments const char *event & const char *data are required for handler functions.***
// This fucntion turns the pump pin to HIGH           //                             (even though they are not used in this program)
// and changes the pump status to "ON".               //
// This function is only called when the "turnPumpOn" //
// event is published to the Prticle.io cloud         //
////////////////////////////////////////////////////////

void pumpOn (const char *event, const char *data) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpStatus = "ON";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUMP ON FUNCTION:                                   //    ***NOTE: The arguments const char *event & const char *data are required for handler functions.***
// This fucntion turns the pump pin to LOW             //                             (even though they are not used in this program)
// and changes the pump status to "OFF".               //
// This function is only called when the "turnPumpOff" //
// event is published to the Prticle.io cloud          //
/////////////////////////////////////////////////////////
void pumpOff (const char *event, const char *data) {
    digitalWrite(PUMP_PIN, LOW);
    pumpStatus = "OFF";
}
