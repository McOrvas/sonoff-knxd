/* 
 * **************************
 * *** WLAN and host name *** 
 * **************************
 */

// SSID and password
const char*  SSID             = "ssid";
const char*  PASSWORD         = "password";

// Host name and description
const String HOST_NAME        = "Sonoff-S20-1",
             HOST_DESCRIPTION = "Description";


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

/* Group addresses for all channels. Unused channels will be ignored.
 * Example:
 * Channel 1 switch: 31/0/0
 * Channel 1 lock:   31/0/1
 * Channel 1 status: 31/0/2
 */
const uint8_t  GA_SWITCH[][3] = {{31, 0,  0},  // Channel 1
                                 {31, 0,  3},  // Channel 2
                                 {31, 0,  6},  // Channel 3
                                 {31, 0,  9}}, // Channel 4

               GA_LOCK[][3]   = {{31, 0,  1},  // Channel 1
                                 {31, 0,  4},  // Channel 2
                                 {31, 0,  7},  // Channel 3
                                 {31, 0, 10}}, // Channel 4

               GA_STATUS[][3] = {{31, 0,  2},  // Channel 1
                                 {31, 0,  5},  // Channel 2
                                 {31, 0,  8},  // Channel 3
                                 {31, 0, 11}}; // Channel 4

// When a channel is being locked, it can be turned on or off.
const boolean  SWITCH_ON_WHEN_LOCKED     = false,
               SWITCH_OFF_WHEN_LOCKED    = false,
               SWITCH_ON_WHEN_UNLOCKED   = false,
               SWITCH_OFF_WHEN_UNLOCKED  = false;

// The (WLAN) LED of the device will be used to show the status of channel 1
const boolean  LED_SHOWS_RELAY_STATUS    = true,

// LED blinks when the device is connected to the EIBD/KNXD
               LED_BLINKS_WHEN_CONNECTED = false;
// Off time in ms during blinking
const uint32_t LED_BLINK_OFF_TIME_MS     = 900,
// On time in ms during blinking
               LED_BLINK_ON_TIME_MS      = 100;

// Waiting time in seconds between a closed connection to the EIBD/KNXD will be reestablished
const uint32_t CONNECTION_LOST_DELAY_S            =  10,
// Maximum time in milliseconds after which a confirmation of the newly established connection has to be received from the EIBD/KNXD
               CONNECTION_CONFIRMATION_TIMEOUT_MS = 500,
// If no telegram was received during this time in minutes, the connection to the EIBD/KNXD will be reestablished. Set to 0 to disable this function.
               NO_TELEGRAM_RECEIVED_TIMEOUT_MIN   = 0;


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

// Debounce time in milliseconds for the hardware button
const uint32_t BUTTON_DEBOUNCING_TIME_MS = 100;


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
