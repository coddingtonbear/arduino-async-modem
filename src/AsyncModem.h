#pragma once

#include <functional>

#include <Arduino.h>
#undef min
#undef max
#include <AsyncDuplex.h>

#define AUTOREFRESH_INTERVAL 10000

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
                bool _autoRefresh = true,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );
            void loop();

            bool enableGPRS(
                char* apn,
                char* username=NULL,
                char* password=NULL,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );
            bool sendSMS(
                char* msisdn,
                char* message,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );

            bool getRSSI(
                int8_t* rssi=NULL,
                std::function<void(MatchState)> success=NULL,
                std::function<void(Command*)> failure=NULL
            );

            bool enableAutoRefresh(bool enabled=true);
            NETWORK_STATUS getNetworkStatus();

            bool modemIsInitialized();
            bool gprsIsEnabled();
        protected:
            bool autoRefresh = false;
            uint32_t nextAutoRefresh = 0;

            bool modemInitialized = false;
            bool gprsEnabled = false;

            NETWORK_STATUS getNetworkStatusForInt(uint8_t);

            NETWORK_STATUS networkStatus = NOT_YET_READY;
    };
};
