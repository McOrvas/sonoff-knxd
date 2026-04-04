#include "NtfyClient.h"
#include <string.h>

NtfyClient::NtfyClient(const char* ip, uint16_t port, const char* topicName)
    : serverPort(port), head(0), tail(0), count(0), hasCurrent(false), state(State::IDLE), requestSent(false) {
    strncpy(serverIp, ip, sizeof(serverIp));
    serverIp[sizeof(serverIp)-1] = '\0';

    strncpy(topic, topicName, sizeof(topic));
    topic[sizeof(topic)-1] = '\0';
}

void NtfyClient::begin() {
    head = 0;
    tail = 0;
    count = 0;

    hasCurrent = false;
    requestSent = false;

    client.stop();

    changeState(State::IDLE);
}

void NtfyClient::changeState(State newState) {
    state = newState;
    stateTimestamp = millis();
}

bool NtfyClient::enqueue(const char* title, const char* message) {
    if (count >= QUEUE_SIZE) {
        return false;
    }

    strncpy(queue[tail].title, title, sizeof(queue[tail].title));
    queue[tail].title[sizeof(queue[tail].title)-1] = '\0';

    strncpy(queue[tail].body, message, sizeof(queue[tail].body));
    queue[tail].body[sizeof(queue[tail].body)-1] = '\0';

    queue[tail].retries = 0;

    tail = (tail + 1) % QUEUE_SIZE;
    count++;

    return true;
}

bool NtfyClient::dequeue(Message& msg) {
    if (count == 0) return false;

    msg = queue[head];
    head = (head + 1) % QUEUE_SIZE;
    count--;

    return true;
}

NtfyClient::State NtfyClient::getState() const {
    return state;
}

void NtfyClient::resetClient() {
    client.stop();
    requestSent = false;
}

void NtfyClient::loop() {
    switch (state) {
        case State::IDLE:
            if (!hasCurrent && dequeue(currentMsg)) {
                hasCurrent = true;
                changeState(State::CONNECTING);
            }
            break;

        case State::CONNECTING:
            if (client.connect(serverIp, serverPort)) {
                changeState(State::SENDING);
            } else {
                changeState(State::ERROR);
            }
            break;

        case State::SENDING:
            if (!requestSent) {
                client.print("POST /");
                client.print(topic);
                client.println(" HTTP/1.1");
                client.print("Host: ");
                client.println(serverIp);
                client.println("Content-Type: text/plain");
                client.print("Title: ");
                client.println(currentMsg.title);
                client.print("Content-Length: ");
                client.println(strlen(currentMsg.body));
                client.println();
                client.print(currentMsg.body);

                requestSent = true;
                changeState(State::WAITING_RESPONSE);
            }
            break;

        case State::WAITING_RESPONSE:
            if (client.available()) {
                char line[64];
                size_t len = client.readBytesUntil('\n', line, sizeof(line)-1);
                line[len] = '\0';

                if (strncmp(line, "HTTP/1.1 200", 12) == 0) {
                    changeState(State::SUCCESS);
                } else if (strncmp(line, "HTTP/1.1", 8) == 0) {
                    changeState(State::ERROR);
                }
            }

            if (millis() - stateTimestamp > 5000) {
                changeState(State::ERROR);
            }
            break;

        case State::SUCCESS:
            resetClient();
            hasCurrent = false;
            changeState(State::QUEUE_DELAY);
            break;

        case State::ERROR:
            resetClient();

            if (hasCurrent && currentMsg.retries < MAX_RETRIES) {
                currentMsg.retries++;
                changeState(State::RETRY_WAIT);
            } else {
                hasCurrent = false;
                changeState(State::QUEUE_DELAY);
            }
            break;

        case State::RETRY_WAIT:
            if (millis() - stateTimestamp >= RETRY_DELAY_MS) {
                changeState(State::CONNECTING);
            }
            break;

        case State::QUEUE_DELAY:
            if (millis() - stateTimestamp >= QUEUE_DELAY_MS) {
                changeState(State::IDLE);
            }
            break;
    }
}