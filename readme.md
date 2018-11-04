# Arduino-Async-Modem

If you've ever used one of the many modem-handling libraries that exist, you're familiar with the frustration that is waiting for a response from a long-running command. Between sending your command and receiving a response (or worse -- that command timing out), your program is halted, and your microcontroller is wasiting valuable cycles. This library aims to fix that problem by allowing you to queue commands that will be asynchronously sent to your device without blocking your microcontroller loop.

## Supported Devices

* SIM7000

## Examples

### Initialization

The setup procedure for an LTE modem involves sending a series of commands and
waiting for their responses.  Using most libraries, this would mean that your
microcontroller would be halted waiting to compare the returned responses with
what it was expecting.  Using `AsyncModem`, `setup` will complete immediately
and the LTE modem will be initialized automatically behind the scenes:

```c++
#include <AsyncModem.h>

AsyncModem::SIM7000 lte = AsyncModem::SIM7000();

void setup() {
    lte.begin(&Serial1);  // If your modem is on a different serial port, use that instead
    lte.enableGPRS("hologram");
}

void loop() {
    lte.loop();
}
```

### Sending an SMS

Executing the below will queue relevant commands for dispatching the SMS message:

```c++
lte.sendSMS("+15555555555", "My Message");
```

If you would like to display a message when the message is sent (or fails to send),
you can pass callacks to do so:

```c++
lte.sendSMS(
    "+15555555555",
    "My Message",
    [](MatchState ms) {
        Serial.println("Sent :-)");
    },
    [](AsyncDuplex::Command* cmd) {
        Serial.println("Failed to send :-(");
    },
);
```

### Sending arbitrary commands

Say that you want to turn on GPS; on the SIM7000, that is controlled by sending the
command `AT+CGNSPWR=1`; you can send that command by running:

```c++
lte.execute("AT+CGNSWPR=1")
```

But what if you wanted to make sure it was successful?  Looking at the docs,
this can return either "OK" or "ERROR".  To help out with that, `execute` accepts
a few more optional parameters:

* **Command** (`char*`): The command to send; above: `AT+CGNSPWR=1`.
* Expectation Regex (`char*`; default: `""`): What you hope to see -- in this case: `OK`.
* Success (`std::function<void(MatchState)>`; default: `NULL`): A function to execute
  once the output matches your expectation regex.  Note that the passed-in `MatchState`
  instance can be used for extracting data from capture groups that you might have
  defined in your expectation regex.
* Failure (`std::function<void(AsyncDuplex::Command*)`; default: `NULL`): A function to execute
  if the output doesn't match your expectation regex in time (see "Timeout").  A
  pointer to the failed command is given to you so you can easily handle retry logic.
* Timeout (ms) (`uint16_t`; default: `2500`): Allow up to this many milliseconds
  to pass before giving up and calling the defined failure function.
* Delay (ms) (`uint32_t`; default: `0`): Do not execute this command until this many
  milliseconds has elapsed after queueing the command.

For example; to first turn on GPS and then, if it's successful, fetch the latest
GPS coordinates, you can write this call:

```c++
float latitude = 0;
float longitude = 0;

lte.execute(
    "AT+CGNSWPR=1",
    "OK",
    [&lte,&latitude,&longitude](MatchState ms) {
        lte.AsyncExecute(
            "AT+CGNSINF",
            "%+CGNSINF:[%d]+,[%d]+,[%d]+,([%d.%+%-]+),([%d.%+%-]+),.*",
            [](MatchState ms) {
                char latitudeBuffer[12];
                char longitudeBuffer[12];
                ms.GetCapture(latitudeBuffer, 0);
                ms.GetCapture(longitudeBuffer, 0);

                latitude = atof(latitudeBuffer);
                longitude = atof(latitudeBuffer);

                Serial.print("Latitude: ");
                Serial.println(latitude);
                Serial.print("Longitude: ");
                Serial.println(longitude);
            },
            [](Command*) {
                "Did not receive the expected response from the LTE modem."
            }
        )
    },
    [](Command*) {
        Serial.println("Failed to turn on GPS.")
    }
)
```

For more options regarding how to execute and chain together commands,
see the documentation for [Arduino Async Duplex](https://github.com/coddingtonbear/arduino-async-duplex).

## Requirements

* std::functional: This is available in the standard library for most non-AVR Arduino
  cores, but should you be attempting to use this on an AVR microcontroller (e.g. an
  atmega328p), you may find what you need in this repository:
  https://github.com/SGSSGene/StandardCplusplus
* arduino-async-duplex: This is built atop this duplex stream management library; you
  can find that library here: https://github.com/coddingtonbear/arduino-async-duplex
* Regexp (https://github.com/nickgammon/Regexp)
