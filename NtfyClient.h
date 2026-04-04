#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>

class NtfyClient {
public:
    enum class State : uint8_t {
        IDLE,
        CONNECTING,
        SENDING,
        WAITING_RESPONSE,
        SUCCESS,
        ERROR,
        RETRY_WAIT,
        QUEUE_DELAY
    };

    static const uint8_t QUEUE_SIZE = 10;
    static const uint8_t MAX_RETRIES = 60;
    static const uint16_t RETRY_DELAY_MS = 10000;
    static const uint16_t QUEUE_DELAY_MS = 1000;

    struct Message {
        char title[128];
        char body[256];
        uint8_t retries;
    };

    NtfyClient(const char* ip, uint16_t port, const char* topic);

    void begin();
    void loop();

    bool enqueue(const char* title, const char* message);

    State getState() const;

private:
    WiFiClient client;

    char serverIp[32];
    uint16_t serverPort;
    char topic[64];

    Message queue[QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;

    Message currentMsg;
    bool hasCurrent;

    State state;
    uint32_t stateTimestamp;
    bool requestSent;

    void changeState(State newState);
    bool dequeue(Message& msg);
    void resetClient();
};