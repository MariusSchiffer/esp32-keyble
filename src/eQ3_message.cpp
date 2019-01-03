#include <math.h>
#include <string>
#include <sstream>
#include "eQ3_message.h"

using namespace std;

char eQ3Message::MessageFragment::getStatusByte() {
    return data[0];
}

int eQ3Message::MessageFragment::getRemainingFragmentCount() {
    return getStatusByte() & 0x7F;
}

bool eQ3Message::MessageFragment::isFirst() {
    return static_cast<bool>(getStatusByte() & (1 << 7));
}

bool eQ3Message::MessageFragment::isLast() {
    return getRemainingFragmentCount() == 0;
}

bool eQ3Message::MessageFragment::isComplete() {
    return isFirst() && isLast();
}

char eQ3Message::MessageFragment::getType() {
    if (!isFirst()) {
        throw "Not first";
    } else {
        return data[1];
    }
}

std::string eQ3Message::MessageFragment::getData() {
    return data.substr(1);
}

eQ3Message::Message::Message(string data) {
    this->data = data;
}

string eQ3Message::Message::encode(ClientState *state) {
    return "";
}

bool eQ3Message::Message::isSecure() {
    return static_cast<bool>(id & (1 << 7));
}

bool eQ3Message::Message::isTypeSecure(char type) {
    return static_cast<bool>(type & (1 << 7));
}

void eQ3Message::Message::decode() {

}

eQ3Message::Connection_Info_Message::Connection_Info_Message() {
    id = 0x03;
}

char eQ3Message::Connection_Info_Message::getUserId() {
    return data[1];
}

string eQ3Message::Connection_Info_Message::getRemoteSessionNonce() {
    return data.substr(2, 8);
}

char eQ3Message::Connection_Info_Message::getBootloaderVersion() {
    return data[11];
}

char eQ3Message::Connection_Info_Message::getAppVersion() {
    return data[12];
}

eQ3Message::Status_Changed_Message::Status_Changed_Message() {
    id = 0x05;
}

eQ3Message::Status_Info_Message::Status_Info_Message() {
    id = 0x83; // TODO: only fits in unsigned char
}

int eQ3Message::Status_Info_Message::getLockStatus() {
    return data[2] & 0x07;
}

int eQ3Message::Status_Info_Message::getUserRightType() {
    return (data[1] & 0x30) >> 4;
}

eQ3Message::StatusRequestMessage::StatusRequestMessage() {
    id = 0x82; // TODO: only fits in unsigned char
}

std::string eQ3Message::StatusRequestMessage::encode(ClientState *state) {
    std::stringstream ss;
    time_t theTime = time(nullptr);
    struct tm *aTime = localtime(&theTime);
    ss.put((char) (aTime->tm_year - 2000));
    ss.put((char) (aTime->tm_mon + 1));
    ss.put((char) (aTime->tm_mday));

    ss.put((char) (aTime->tm_hour));
    ss.put((char) (aTime->tm_min));
    ss.put((char) (aTime->tm_sec));
    return ss.str();
}

eQ3Message::Connection_Close_Message::Connection_Close_Message() {
    id = 0x06;
}


eQ3Message::Connection_Request_Message::Connection_Request_Message() {
    id = 0x02;
}

std::string eQ3Message::Connection_Request_Message::encode(ClientState *state) {
    std::stringstream ss;
    ss.put(state->user_id);
    ss << state->local_session_nonce;
    return ss.str();
}

eQ3Message::CommandMessage::CommandMessage(char command) {
    this->command = command;
    id = 0x87; // TODO: only fits in unsigned char
}

std::string eQ3Message::CommandMessage::encode(ClientState *state) {
    std::stringstream ss;
    ss.put(this->command);
    return ss.str();
}

eQ3Message::AnswerWithoutSecurityMessage::AnswerWithoutSecurityMessage() {
    id = 0x01;
}


eQ3Message::AnswerWithSecurityMessage::AnswerWithSecurityMessage() {
    id = 0x81; // TODO: only fits in unsigned char
}

bool eQ3Message::AnswerWithSecurityMessage::getA() {
    return (data[1] & 0x80) == 0;
}

bool eQ3Message::AnswerWithSecurityMessage::getB() {
    return (data[1] & 0x81) == 0;
}


eQ3Message::PairingRequestMessage::PairingRequestMessage() {
    id = 0x04;
}

std::string eQ3Message::PairingRequestMessage::encode(ClientState *state) {
    return data;
}

eQ3Message::FragmentAckMessage::FragmentAckMessage(char fragment_id) {
    std::stringstream ss;
    ss << id;
    ss << fragment_id;
    data = ss.str();
}



