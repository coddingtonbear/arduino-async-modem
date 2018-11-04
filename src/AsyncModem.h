#pragma once

#include <functional>

#include <Arduino.h>
#undef min
#undef max
#include <AsyncDuplex.h>

#define ASYNC_MODEM_COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

namespace AsyncModem {
    class SIM7000: public AsyncDuplex {
        public: 
            enum NETWORK_STATUS {
                NOT_YET_READY,
                UNEXPECTED_RESULT,

                NOT_REGISTERED,
                REGISTERED_HOME,
                SEARCHING,
                REGISTRATION_DENIED,
                UNKNOWN,
                REGISTERED_ROAMING
            };

            bool begin(
                Stream* stream,
                Stream* errorStream=&Serial,
                uint8_t ATAttempts=10,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );

            bool enableGPRS(
                char* apn,
                char* username=NULL,
                char* password=NULL,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );

            bool getNetworkStatus(
                NETWORK_STATUS* status=NULL,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );
            bool getRSSI(
                int8_t* rssi=NULL,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );
            bool sendSMS(
                char* msisdn,
                char* message,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );

            bool modemIsInitialized();
            bool gprsIsEnabled();
        protected:
            bool modemInitialized = false;
            bool gprsEnabled = false;
    };
};
