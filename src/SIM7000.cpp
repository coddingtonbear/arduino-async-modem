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
    bool _autoRefresh,
    std::function<void(MatchState)> success,
    std::function<void(Command*)> failure
) {
    AsyncDuplex::begin(_stream, _errorStream);
    autoRefresh = _autoRefresh;
    nextAutoRefresh = millis() + AUTOREFRESH_INTERVAL;

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
                    execute(cmd);
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
            "SIM7000.*\r\nOK\r\n",
            NULL,
            [this](Command* command) {
                emitErrorMessage(
                    "Warning: this does not appear to be a SIM7000 device!"
                );

                // Execute further members of the chain anyway
                if(command->success) {
                    MatchState empty;
                    command->success(empty);
                }
            }
        ),
        Command(
            "AT+CLTS=1",
            "OK",
            [this, success](MatchState ms) {
                modemInitialized = true;
                if(success) {
                    success(ms);
                }
            }
        )
    };
    uint8_t commandCount = ASYNC_MODEM_COUNT_OF(commands);

    // Capture network time announcements so we can set the clock
    registerHook(
        // *PSUTTZ: 18/11/04,22:38:07","-32",0
        "%*PSUTTZ: ([%d]+/[%d]+/[%d]+,[%d]+:[%d]+:[%d]+).*\"([+-][%d]+)\"",
        [this](MatchState ms) {
            char cclk[64];
            char datetime[32];
            char zone[8];

            ms.GetCapture(datetime, 0);
            ms.GetCapture(zone, 1);
            sprintf(cclk, "AT+CCLK=\"%s%s\"", datetime, zone);

            stripMatchFromInputBuffer(ms);

            execute(
                cclk,
                "OK"
            );
        }
    );

    return executeChain(
        commands,
        commandCount,
        NULL,
        failure
    );
}

void AsyncModem::SIM7000::loop() {
    AsyncDuplex::loop();
    if(autoRefresh && millis() > nextAutoRefresh) {
        nextAutoRefresh = millis() + AUTOREFRESH_INTERVAL;

        // Refresh network status
        execute(
            "AT+CREG?",
            "%+CREG: [%d]+,([%d]+)",
            [this](MatchState ms) {
                char result[32];
                ms.GetCapture(result, 0);
                networkStatus = getNetworkStatusForInt(atoi(result));
            }
        );
    }
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

    return executeChain(
        commands,
        commandCount,
        NULL,
        failure
     );
}

AsyncModem::SIM7000::NETWORK_STATUS AsyncModem::SIM7000::getNetworkStatus() {
    return networkStatus;
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
    return execute(
        "AT+CSQ",
        "%+CSQ: ([%d]+),[%d]+.*\n",
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

    return execute(
        "AT+CMGF=1",
        "OK",
        [this,success,failure,msisdn,message](MatchState ms) {
            char atCmgs[30];
            sprintf(atCmgs, "AT+CMGS=\"%s\"", msisdn.c_str());
            execute(
                atCmgs,
                ">",
                AsyncDuplex::Timing::NEXT,
                [this,success,message](MatchState ms) {
                    for (uint16_t i = 0; i < message.length(); i++) {
                        AsyncModem::SIM7000::write(message.c_str()[i]);
                    }
                    AsyncModem::SIM7000::write(0x1a);
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
}

bool AsyncModem::SIM7000::enableAutoRefresh(bool enabled) {
    autoRefresh = enabled;
}

AsyncModem::SIM7000::NETWORK_STATUS AsyncModem::SIM7000::getNetworkStatusForInt(uint8_t value) {
    switch(value) {
        case 0:
            return NETWORK_STATUS::NOT_REGISTERED;
        case 1:
            return NETWORK_STATUS::REGISTERED_HOME;
        case 2:
            return NETWORK_STATUS::SEARCHING;
        case 3:
            return NETWORK_STATUS::REGISTRATION_DENIED;
        case 4:
            return NETWORK_STATUS::UNKNOWN;
        case 5:
            return NETWORK_STATUS::REGISTERED_ROAMING;
        default:
            return NETWORK_STATUS::UNEXPECTED_RESULT;
    }
}

bool AsyncModem::SIM7000::modemIsInitialized(){
    return modemInitialized;
}

bool AsyncModem::SIM7000::gprsIsEnabled(){
    return gprsEnabled;
}
