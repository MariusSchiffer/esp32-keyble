#include <Arduino.h>
#include <string>

#include <BLEDevice.h>
#include <ctime>
#include <sstream>

#include "eQ3.h"
#include "eQ3_util.h"

using namespace std;

void tickTask(void *params) {
    auto * inst = (eQ3*) params;
    while(inst->onTick()) {
        yield();
    }
}

// TODO proper logging


// Important: Initialize BLEDevice::init(""); yourself
eQ3::eQ3(std::string ble_address, std::string user_key, unsigned char user_id) {
    state.user_key = hexstring_to_string(user_key);
    Serial.println(state.user_key.length());
    state.user_id = user_id;

    mutex = xSemaphoreCreateMutex();

    this->address = ble_address;

    // init BLE scan

    bleScan = BLEDevice::getScan();
    bleScan->setAdvertisedDeviceCallbacks(this);
    bleScan->setActiveScan(true);
    bleScan->setInterval(100);
    bleScan->setWindow(99);


    // TODO move this out to an extra init?
    bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks((BLEClientCallbacks *)this);

    // pin to core 1 (where the Arduino main loop resides), priority 1
    xTaskCreatePinnedToCore(&tickTask, "worker", 10000, this, 1, nullptr, 1);

}

void eQ3::onConnect(BLEClient *pClient) {
    Serial.println("connected");
    state.connectionState = CONNECTING;
}

void eQ3::onDisconnect(BLEClient *pClient) {
    Serial.println("disconnected");
    state.connectionState = DISCONNECTED;
    recvFragments.clear();
    sendQueue = std::queue<eQ3Message::MessageFragment>(); // clear queue
    queue.clear();
    sendChar = recvChar = nullptr;
}

bool eQ3::onTick() {
    // end task when disconnected?
    //if (state.connectionState == DISCONNECTED)
    //    return false;
    if (xSemaphoreTake(mutex, 0)) {
        if (state.connectionState == FOUND) {
            Serial.println("connecting in tick");
            bleClient->connect(BLEAddress(address), BLE_ADDR_TYPE_PUBLIC);
        } else if (state.connectionState == CONNECTING) {
            Serial.println("connected in tick");
            BLERemoteService *comm;
            comm = bleClient->getService(BLEUUID(BLE_UUID_SERVICE));
            sendChar = comm->getCharacteristic(BLEUUID(BLE_UUID_WRITE)); // write buffer characteristic
            recvChar = comm->getCharacteristic(BLEUUID(BLE_UUID_READ)); // read buffer characteristic
            recvChar->setNotifyCallbacks((BLERemoteCharacteristicCallbacks*)this);
            lastActivity = time(NULL);
            state.connectionState = CONNECTED;
            auto queueFunc = queue.find(CONNECTED);
            if (queueFunc != queue.end()) {
                queue.erase(CONNECTED);
                //xSemaphoreGive(mutex); // function will take the semaphore again
                queueFunc->second();
            }
        } else {
            sendNextFragment();
            lastActivity = time(NULL);
        }
        // TODO disconnect if no answer for long time?
        if (state.connectionState >= CONNECTED && difftime(lastActivity, time(NULL)) > LOCK_TIMEOUT && sendQueue.empty()) {
            bleClient->disconnect();
        }
        xSemaphoreGive(mutex);
    }

    return true;
}

void eQ3::onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == "KEY-BLE") { // TODO: Make name and address variable
        Serial.println("found advertised");
        Serial.println(advertisedDevice.getAddress().toString().c_str());
        bleScan->stop();
        state.connectionState = FOUND;
    }
}

void eQ3::setOnStatusChange(std::function<void(LockStatus)> cb) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    onStatusChange = cb;
    xSemaphoreGive(mutex);
}

void eQ3::exchangeNonces() {
    state.local_session_nonce.clear();
    for (int i = 0; i < 8; i++)
        state.local_session_nonce.append(1,esp_random());
    auto *msg = new eQ3Message::Connection_Request_Message;
    Serial.println("exchanging nonces");
    sendMessage(msg);
}

void eQ3::connect() {
    state.connectionState = SCANNING;
    bleScan->start(20, nullptr, false);
    Serial.println("searching...");
    //state.connectionState = FOUND;
    //Serial.println("connecting directly...");
}

bool eQ3::sendMessage(eQ3Message::Message *msg) {
    std::string data;
    if (msg->isSecure()) {
        if (state.connectionState < NONCES_EXCHANGED) {
            // TODO check if slot for nonces_exchanged is already set?
            queue.insert(make_pair(NONCES_EXCHANGED,[this,msg]{
                sendMessage(msg);
            }));
            exchangeNonces();
            return true;
        }

        string padded_data;
        padded_data.append(msg->encode(&state));
        int pad_to = generic_ceil(padded_data.length(), 15, 8);
        if (pad_to > padded_data.length()) {
            padded_data.append(pad_to - padded_data.length(), 0);
        }
        crypt_data(padded_data, msg->id, state.remote_session_nonce, state.local_security_counter,
                   state.user_key);
        data.append(1, msg->id);
        data.append(crypt_data(padded_data, msg->id, state.remote_session_nonce, state.local_security_counter,
                               state.user_key));
        data.append(1, (char) (state.local_security_counter >> 8));
        data.append(1, (char) state.local_security_counter);
        data.append(
                compute_auth_value(padded_data, msg->id, state.remote_session_nonce,
                                   state.local_security_counter,
                                   state.user_key));
        state.local_security_counter++;
    } else {
        if (state.connectionState < CONNECTED) {
            // TODO check if slot for connected is already set?
            queue.insert(make_pair(CONNECTED,[this,msg]{
                sendMessage(msg);
            }));
            connect();
            return true;
        }
        data.append(1, msg->id);
        data.append(msg->encode(&state));
    }
    // fragment
    int chunks = data.length() / 15;
    if (data.length() % 15 > 0) {
        chunks += 1;
    }
    for (int i = 0; i < chunks; i++) {
        eQ3Message::MessageFragment frag;
        frag.data.append(1, (i ? 0 : 0x80) + (chunks - 1 - i)); // fragment status byte
        frag.data.append(data.substr(i * 15, 15));
        if (frag.data.length() < 16) {
            // padding
            frag.data.append(16 - (frag.data.length() % 16), 0);
        }
        sendQueue.push(frag);
    }

    free(msg);
    return true;
}


void eQ3::onNotify(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    eQ3Message::MessageFragment frag;
    lastActivity = time(NULL);
    frag.data = std::string((char *) pData, length);
    recvFragments.push_back(frag);
    Serial.println(string_to_hex(frag.data).c_str());
    if (frag.isLast()) {
        if (!sendQueue.empty())
            sendQueue.pop();
        // concat message
        std::stringstream ss;
        auto msgtype = recvFragments.front().getType();
        for (auto &received_fragment : recvFragments) {
            ss << received_fragment.getData();
        }
        std::string msgdata = ss.str();
        recvFragments.clear();
        if (eQ3Message::Message::isTypeSecure(msgtype)) {

            auto msg_security_counter = static_cast<uint16_t>(msgdata[msgdata.length() - 6]);
            msg_security_counter <<= 8;
            msg_security_counter += msgdata[msgdata.length() - 5];
            Serial.println((int)msg_security_counter);
            if (msg_security_counter <= state.remote_security_counter) {
                Serial.println(msg_security_counter);
                Serial.println(state.remote_security_counter);
                Serial.println("Wrong remote counter");
                xSemaphoreGive(mutex);
                return;
            }
            state.remote_security_counter = msg_security_counter;
            string msg_auth_value = msgdata.substr(msgdata.length() - 4, 4);
            Serial.print("auth value: ");
            Serial.println(string_to_hex(msg_auth_value).c_str());
            //std::string decrypted = crypt_data(msgdata.substr(0, msgdata.length() - 6), msgtype,
            std::string decrypted = crypt_data(msgdata.substr(1, msgdata.length() - 7), msgtype,
                                               state.local_session_nonce, state.remote_security_counter,
                                               state.user_key);
            Serial.print("crypted data: ");
            Serial.println(string_to_hex(msgdata.substr(1, msgdata.length() - 7)).c_str());
            std::string computed_auth_value = compute_auth_value(decrypted, msgtype,
                                                                 state.local_session_nonce,
                                                                 state.remote_security_counter, state.user_key);
            if (msg_auth_value != computed_auth_value) {
                Serial.println("Auth value mismatch");
                xSemaphoreGive(mutex);
                return;
            }
            msgdata = decrypted;
            Serial.println("Decrypted:");
            Serial.println(string_to_hex(msgdata).c_str());
        }

        switch (msgtype) {
            case 0: {
                // fragment ack, remove first
                if (!sendQueue.empty()) {
                    sendQueue.pop();
                }
                xSemaphoreGive(mutex);
                return;
            }

            case 0x81: // answer with security
                // TODO call callback to user that pairing succeeded
                break;


            case 0x01: // answer without security
                // TODO report error
                break;

            case 0x05: {
                // status changed notification
                // TODO request status
                auto * message = new eQ3Message::StatusRequestMessage;
                sendMessage(message);
                break;
            }

            case 0x03: {
                // connection info message
                eQ3Message::Connection_Info_Message message;
                message.data = msgdata;
                state.user_id = message.getUserId();
                state.remote_session_nonce = message.getRemoteSessionNonce();
                assert(state.remote_session_nonce.length() == 8);

                state.local_security_counter = 1;
                state.remote_security_counter = 0;
                state.connectionState = NONCES_EXCHANGED;

                Serial.println("nonces exchanged");
                auto queueFunc = queue.find(NONCES_EXCHANGED);
                if (queueFunc != queue.end()) {
                    queue.erase(queueFunc);
                    //xSemaphoreGive(mutex); // function will take the semaphore again
                    queueFunc->second();
                }
                xSemaphoreGive(mutex);
                return;
            }

            case 0x83: {
                // status info
                eQ3Message::Status_Info_Message message;
                message.data = msgdata;
                Serial.println("New Lock status");
                Serial.println(message.getLockStatus());
                onStatusChange((LockStatus)message.getLockStatus());
                break;
            }

            default:
            case 0x8f: { // user info
                xSemaphoreGive(mutex);
                return;
            }

        }

    } else {
        // create ack
        Serial.println("sending ack");
        eQ3Message::FragmentAckMessage ack(frag.getStatusByte());
        sendQueue.push(ack);
    }
    xSemaphoreGive(mutex);
}

void eQ3::pairingRequest(std::string cardkey) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (state.connectionState < NONCES_EXCHANGED) {
        // TODO check if slot for nonces_exchanged is already set?

        // TODO callback when pairing finished, or make blocking?
        queue.insert(make_pair(NONCES_EXCHANGED,[this,cardkey]{
            pairingRequest(cardkey);
        }));
        exchangeNonces();
        xSemaphoreGive(mutex);
        return;
    }
    auto *message = new eQ3Message::PairingRequestMessage();
    //return concatenated_array([data.user_id], padded_array(data.encrypted_pair_key, 22, 0), integer_to_byte_array(data.security_counter, 2), data.authentication_value);
    message->data.append(1, state.user_id);
    Serial.println((int) message->id);

    // enc pair key
    assert(state.remote_session_nonce.length() == 8);
    assert(state.user_key.length() == 16);

    std::string card_key = hexstring_to_string(cardkey);

    string encrypted_pair_key = crypt_data(state.user_key, 0x04, state.remote_session_nonce,
                                           state.local_security_counter, card_key);
    if (encrypted_pair_key.length() < 22) {
        encrypted_pair_key.append(22 - encrypted_pair_key.length(), 0);
    }
    assert(encrypted_pair_key.length() == 22);
    message->data.append(encrypted_pair_key);


    // counter
    message->data.append(1, (char) (state.local_security_counter >> 8));
    message->data.append(1, (char) (state.local_security_counter));

    // auth value
    string extra;
    extra.append(1, state.user_id);
    extra.append(state.user_key);
    if (extra.length() < 23) {
        extra.append(23 - extra.length(), 0);
    }
    assert(extra.length() == 23);
    string auth_value = compute_auth_value(extra, 0x04, state.remote_session_nonce,
                                           state.local_security_counter, card_key);
    message->data.append(auth_value);
    assert(message->data.length() == 29);

    sendMessage(message);

    xSemaphoreGive(mutex);
}

void eQ3::sendNextFragment() {
    if (sendQueue.empty())
        return;
    if (sendQueue.front().sent && std::difftime(sendQueue.front().timeSent, std::time(NULL)) < 5)
        return;
    sendQueue.front().sent = true;
    Serial.println("sending actual fragment:");
    string data = sendQueue.front().data;
    sendQueue.front().timeSent = std::time(NULL);
    Serial.println(string_to_hex(data).c_str());
    assert(data.length() == 16);
    sendChar->writeValue((uint8_t *) (data.c_str()), 16, true);
}

void eQ3::sendCommand(CommandType command) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    Serial.println("take semaphore sendcommand");
    auto msg = new eQ3Message::CommandMessage(command);
    sendMessage(msg);
    Serial.println("unlock semaphore");
    xSemaphoreGive(mutex);
}

void eQ3::unlock() {
    sendCommand(UNLOCK);
}

void eQ3::lock() {
    sendCommand(UNLOCK);
}

void eQ3::open() {
    sendCommand(UNLOCK);
}

void eQ3::updateInfo() {

}