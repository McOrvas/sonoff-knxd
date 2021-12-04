/* 
 * **************************
 * *** WLAN and host name *** 
 * **************************
 */

// SSID and password
const char     *SSID                        = "ssid",
               *PASSWORD                    = "password",

// Host name and description
               *HOST_NAME                   = "Sonoff-S20-1",
               *HOST_DESCRIPTION            = "Description";

const uint32_t WIFI_CONNECTION_TIMEOUT_S    = 30,
               WIFI_CONNECTION_LOST_DELAY_S = 10;


/* 
 * *******************
 * *** KNX options *** 
 * *******************
 */               
               
// EIBD/KNXD
const char*    KNXD_IP   = "10.9.8.7";
const uint32_t KNXD_PORT = 6720;

// Number of active switching channels. 1 for S20 and Basic, 1-4 for 4CH (Pro)
const uint8_t  CHANNELS = 1;

/* Group addresses for all channels. Unused channels and 0/0/0 GAs will be ignored.
 * For the status exactly one GA must be defined per channel.
 * For switch and lock multiple GAs can be defined, see example below.
 * Remark: The number of GAs must be equal for all channels in switch respectively lock.
 *         Fill unused GAs for other channels with 0/0/0.
 * 
 * Example:
 * Channel 1 switch: 31/0/0
 * Channel 1 lock:   31/0/1
 * Channel 1 status: 31/0/2
 */
const uint8_t  GA_SWITCH[4][2][3] = {{{31, 0,  0}, {31, 1,  0}},  // Channel 1 (example with 2 GAs)
                                     {{31, 0,  3}, {31, 1,  3}},  // Channel 2 (example with 2 GAs)
                                     {{31, 0,  6}, {0,  0,  0}},  // Channel 3 (example with 1 GA)
                                     {{31, 0,  9}, {0,  0,  0}}}, // Channel 4 (example with 1 GA)

               GA_LOCK[4][1][3]   = {{{31, 0,  1}},               // Channel 1 (example with 1 GA)
                                     {{31, 0,  4}},               // Channel 2 (example with 1 GA)
                                     {{31, 0,  7}},               // Channel 3 (example with 1 GA)
                                     {{31, 0, 10}}},              // Channel 4 (example with 1 GA)

               GA_STATUS[][3] =      {{31, 0,  2},                // Channel 1 (always exactly one address)
                                      {31, 0,  5},                // Channel 2 (always exactly one address)
                                      {31, 0,  8},                // Channel 3 (always exactly one address)
                                      {31, 0, 11}},               // Channel 4 (always exactly one address)

               GA_TIME[]      =       { 0, 0,  0},                // GA for time, DPT 10.001 (3 bytes)
               GA_DATE[]      =       { 0, 0,  0};                // GA for date, DPT 11.001 (3 bytes)

// The telegrams for date and time should be requested after the connection has been established
const boolean  REQUEST_DATE_AND_TIME_INITIALLY = false;

// When a channel is being (un)locked, it can be turned on or off.
const boolean  SWITCH_ON_WHEN_LOCKED[]    = {false, false, false, false},
               SWITCH_OFF_WHEN_LOCKED[]   = {false, false, false, false},
               SWITCH_ON_WHEN_UNLOCKED[]  = {false, false, false, false},
               SWITCH_OFF_WHEN_UNLOCKED[] = {false, false, false, false},
               
// The lock can be inverted (0 = lock / 1 = unlock)
               LOCK_INVERTED[]            = {false, false, false, false};

// Waiting time in seconds between a closed connection to the EIBD/KNXD will be reestablished
const uint32_t CONNECTION_LOST_DELAY_S            =   10,
// Maximum time in milliseconds after which a confirmation of the newly established connection has to be received from the EIBD/KNXD
               CONNECTION_CONFIRMATION_TIMEOUT_MS =  500,
// If no telegram was received during this time in minutes, the connection to the EIBD/KNXD will be reestablished. Set to 0 to disable this function.
               MISSING_TELEGRAM_TIMEOUT_MIN       =   60,
// If a received telegram is not completed during this time, the connection to the EIBD/KNXD will be reestablished. Set to 0 to disable this function.
               INCOMPLETE_TELEGRAM_TIMEOUT_MS     = 5000;


/*
 * *******************
 * *** LED options ***
 * *******************
 */
// The (WLAN) LED of the device will be used to show the status of channel 1
const boolean  LED_SHOWS_RELAY_STATUS    = true,

// LED blinks when the device is connected to the EIBD/KNXD
               LED_BLINKS_WHEN_CONNECTED = false;
// Off time in ms during blinking
const uint32_t LED_BLINK_OFF_TIME_MS     = 900,
// On time in ms during blinking
               LED_BLINK_ON_TIME_MS      = 100;


/*
 * ************************
 * *** Auto off options ***
 * ************************
 */

// Time after which the relay is automatically switched off again. A value of 0 deactivates the function.
const uint32_t AUTO_OFF_TIMER_S[] = {
                                     0, // Channel 1
                                     0, // Channel 2
                                     0, // Channel 3
                                     0  // Channel 4
                                    };

// Determine if the auto off timer should override an active lock or not.
const boolean AUTO_OFF_TIMER_OVERRIDES_LOCK = true;

// Time after which the channel is automatically unlocked again. A value of 0 deactivates the function.
const uint32_t AUTO_UNLOCK_TIMER_S[] = {
                                     0, // Channel 1
                                     0, // Channel 2
                                     0, // Channel 3
                                     0  // Channel 4
                                    };


/* 
 * ***************************
 * *** GPIO pin assignment ***
 * ***************************
 */

const uint8_t  GPIO_LED      =   13,               
               GPIO_BUTTON[] = {
                                  0, // Button 1 (S20, Basic and 4CH [Pro])
                                  9, // Button 2 (4CH [Pro]) 
                                 10, // Button 3 (4CH [Pro])
                                 14  // Button 4 (4CH [Pro])
                               },
               GPIO_RELAY[]  = {
                                 12, // Relay 1 (S20, Basic and 4CH [Pro])
                                  5, // Relay 2 (4CH [Pro])
                                  4, // Relay 3 (4CH [Pro])
                                 15  // Relay 4 (4CH [Pro])
                               };


/* 
 * ****************************
 * *** Relay type (NO / NC) ***
 * ****************************
 */
const boolean RELAY_NORMALLY_OPEN[] = {true,  true,  true,  true};


/* 
 * ****************************
 * *** Hardware button mode ***
 * ****************************
 */
// True  = Toggle mode (the relay toggles at each pressure)
// False = Switch mode (as long as the button is pressed, the relay switches)
const boolean  BUTTON_TOGGLE             = true,
// Inverts the button
               BUTTON_INVERTED           = false;
// Debounce time in milliseconds for the hardware button
const uint32_t BUTTON_DEBOUNCING_TIME_MS = 50;


/* 
 * *********************
 * *** TCP keepalive *** 
 * *********************
 */

// Keepalive time is the duration between two keepalive transmissions in idle condition.
const uint32_t KA_IDLE_S                          = 1800,
// Keepalive interval is the duration between two successive keepalive retransmissions, if acknowledgement to the previous keepalive transmission is not received.
               KA_INTERVAL_S                      =   30,
// Keepalive retry is the number of retransmissions to be carried out before declaring that remote end is not available.
               KA_RETRY_COUNT                     =   10;


/* 
 * ********************************
 * *** SCD30 air sensor options *** 
 * ********************************
 */  

// Set to true to activate the SCD30 air sensor options. Without SCD30, this whole section can be deleted from the configuration.
#define SCD30_ENABLE false
// Set to true to activate the LCD to display air quality values. The LCD can only be used together with the SCD30.
#define LCD_ENABLE   false

const uint8_t  GA_AIR_TEMPERATURE[] = {31, 1, 0}, // DPT 9.001 (2 bytes)
               GA_AIR_HUMIDITY[]    = {31, 1, 1}, // DPT 9.007 (2 bytes)
               GA_AIR_CO2[]         = {31, 1, 2}; // DPT 9.008 (2 bytes)
               
               // How often the SCD30 measures internally (seconds)
const uint16_t AIR_SENSOR_MEASUREMENT_INTERVAL_S     =  10,
               // How often the sensor is polled for new measurement data (seconds)
               AIR_SENSOR_POLL_INTERVAL_S            =   1,
               // The altitude in meters above sea level
               AIR_SENSOR_ALTITUDE_COMPENSATION_M    =   0,
               // The CO2 concentration of the atmosphere for the manual calibraton (~400 ppm)
               AIR_SENSOR_CO2_CALIBRATION_PPM        = 400;
               // A temperature offset for measurement correction (Kelvin)
const float    AIR_SENSOR_TEMPERATURE_OFFSET_K       = 0.0;
               
               // The maximum time between two KNX transmissions (seconds)
const uint16_t KNX_SEND_INTERVAL_CO2_S               = 900,
               KNX_SEND_INTERVAL_TEMPERATURE_S       = 900,
               KNX_SEND_INTERVAL_HUMIDITY_S          = 900, 
               // The maximum value difference between two KNX transmissions
               KNX_SEND_DIFFERENCE_VALUE_CO2         = 50;
const float    KNX_SEND_DIFFERENCE_VALUE_TEMPERATURE = 0.2,
               KNX_SEND_DIFFERENCE_VALUE_HUMIDITY    = 1.0;
