#include <functional>

#include <Arduino.h>
#undef min
#undef max

#include "AsyncModem.h"

uint8_t beginATAttemptCount = 0;

bool AsyncModem::SIM7000::begin(
    Stream* _stream,
    Stream* _errorStream,
    uint8_t ATAttempts,
    std::function<void(MatchState)> success,
    std::function<void(Command*)> failure
) {
    AsyncDuplex::begin(_stream, _errorStream);

    Command commands[] = {
        Command(
            "AT",
            "OK",
            [&beginATAttemptCount](MatchState ms) {
                beginATAttemptCount = 0;
            },
            [this, failure, &beginATAttemptCount, ATAttempts](Command* cmd) {
                beginATAttemptCount++;
                if(beginATAttemptCount < ATAttempts) {
                    cmd->delay = 1000;
                    asyncExecute(cmd);
                } else {
                    emitErrorMessage("Failed to connect to modem!");
                    if(failure) {
                        failure(cmd);
                    }
                }
            }
        ),
        Command(
            "ATE0",
            "OK"
        ),
        Command(
            "ATI",
            "SIM7000.*\r\nOK\r\n"
        ),
        Command(
            "AT+CPMS=\"SM\",\"SM\",\"SM\"",
            "%+CPMS:.*\n",
            [this, success](MatchState ms){
                modemInitialized = true;
                if(success) {
                    success(ms);
                }
            }
        )
    };
    uint8_t commandCount = ASYNC_MODEM_COUNT_OF(commands);

    return asyncExecuteChain(
        commands,
        commandCount,
        Timing::ANY,
        NULL,
        failure
    );
}

bool AsyncModem::SIM7000::enableGPRS(
    char* apn,
    char* username,
    char* password,
    std::function<void(MatchState)> success,
    std::function<void(Command*)> failure
) {
    char atSapbrApn[64];
    sprintf(atSapbrApn, "AT+SAPBR=3,1,\"APN\",\"%s\"", apn);
    char atCstt[128];
    if(username && password) {
        sprintf(
            atCstt,
            "AT+CSTT=\"%s\",\"%s\",\"%s\"",
            apn,
            username,
            password
        );
    } else {
        sprintf(atCstt, "AT+CSTT=\"%s\"", apn);
    }

    Command commands[] = {
        Command(
            "AT+CIPSHUT",
            "OK"
        ),
        Command(
            "AT+CGATT=1",
            "OK"
        ),
        Command(
            "AT+SAPBR=0,1",
            // Either response ('ERROR', 'OK') is fine; 'ERROR'
            // just means that the bearer was already closed
            ".+\r\n"
        ),
        Command(
            "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",
            "OK"
        ),
        Command(
            atSapbrApn,
            "OK"
        ),
        Command(
            atCstt,
            "OK"
        ),
        Command(
            "AT+SAPBR=1,1",
            "OK"
        ),
        Command(
            "AT+CIICR",
            "OK",
            [this, success](MatchState ms){
                gprsEnabled = true;
                if(success) {
                    success(ms);
                }
            }
        )
    };
    uint8_t commandCount = ASYNC_MODEM_COUNT_OF(commands);

    return asyncExecuteChain(
        commands,
        commandCount,
        Timing::ANY,
        NULL,
        failure
     );
}

bool AsyncModem::SIM7000::getNetworkStatus(
    NETWORK_STATUS* status,
    std::function<void(MatchState)> success,
    std::function<void(Command*)> failure
) {
    *status = NETWORK_STATUS::NOT_YET_READY;
    return asyncExecute(
        "AT+CREG?",
        "%+CREG: [%d]+,([%d]+).*\n",
        Timing::ANY,
        [&status, success](MatchState ms) {
            if(status) {
                char stateBuffer[3];
                ms.GetCapture(stateBuffer, 0);
                uint8_t value = atoi(stateBuffer);
                switch(value) {
                    case 0:
                        *status = NETWORK_STATUS::NOT_REGISTERED;
                        break;
                    case 1:
                        *status = NETWORK_STATUS::REGISTERED_HOME;
                        break;
                    case 2:
                        *status = NETWORK_STATUS::SEARCHING;
                        break;
                    case 3:
                        *status = NETWORK_STATUS::REGISTRATION_DENIED;
                        break;
                    case 4:
                        *status = NETWORK_STATUS::UNKNOWN;
                        break;
                    case 5:
                        *status = NETWORK_STATUS::REGISTERED_ROAMING;
                        break;
                    default:
                        *status = NETWORK_STATUS::UNEXPECTED_RESULT;
                        break;
                }
            }
            if(success) {
                success(ms);
            }
        },
        [failure](Command* cmd) {
            if(failure) {
                failure(cmd);
            }
        }
    );
}

bool AsyncModem::SIM7000::getRSSI(
    int8_t* rssi,
    std::function<void(MatchState)> success,
    std::function<void(Command*)> failure
) {
    // RSSI values are defined using this mapping:
    //  0: -115 dBm or less
    //  1: -111 dBm
    //  2...30: -110... -54 dBm
    //  31: -52 dBm or greater
    //  99: not known or not detectable
    *rssi = -1;
    return asyncExecute(
        "AT+CSQ",
        "%+CSQ: ([%d]+),[%d]+.*\n",
        Timing::ANY,
        [&rssi,success](MatchState ms) {
            if(rssi) {
                char stateBuffer[3];
                ms.GetCapture(stateBuffer, 0);
                *rssi = atoi(stateBuffer);
            }
            if(success) {
                success(ms);
            }
        },
        [failure](Command* cmd) {
            if(failure) {
                failure(cmd);
            }
        }
    );
}

bool AsyncModem::SIM7000::sendSMS(
    char* _msisdn,
    char* _message,
    std::function<void(MatchState)> success,
    std::function<void(Command*)> failure
) {
    String msisdn = _msisdn;
    String message = _message;

    return asyncExecute(
        "AT+CMGF=1",
        "OK",
        AsyncDuplex::Timing::ANY,
        [this,success,failure,msisdn,message](MatchState ms) {
            char atCmgs[30];
            sprintf(atCmgs, "AT+CMGS=\"%s\"", msisdn.c_str());
            asyncExecute(
                atCmgs,
                ">",
                AsyncDuplex::Timing::NEXT,
                [this,success,failure,message](MatchState ms) {
                    char messageBuffer[message.length() + 2];
                    sprintf(messageBuffer, "%s\x1a", message.c_str());
                    asyncExecute(
                        messageBuffer,
                        "OK",
                        AsyncDuplex::Timing::NEXT,
                        [success](MatchState ms) {
                            if(success) {
                                success(ms);
                            }
                        },
                        [failure](Command* cmd) {
                            if(failure) {
                                failure(cmd);
                            }
                        }
                    );
                },
                [failure](Command* cmd) {
                    if(failure) {
                        failure(cmd);
                    }
                }
            );
        },
        [failure](Command* cmd) {
            if(failure) {
                failure(cmd);
            }
        }
    );
}

bool AsyncModem::SIM7000::modemIsInitialized(){
    return modemInitialized;
}

bool AsyncModem::SIM7000::gprsIsEnabled(){
    return gprsEnabled;
}
