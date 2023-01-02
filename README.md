This is a proof-of-concept for interacting with the Snapdragon Sensor Core
using the reverse-engineered protocol buffers. It was made to try to
initialize it, but the Pixel 3a is missing some sensors once the firmware is
loaded.

A best effort is made to prefix all log messages with `sensh: ` so it's clear
which lines were entered by the user.

# Requirements

 - gcc
 - libqrtr
 - make
 - protobuf-c
 - qmic (https://github.com/andersson/qmic)

# Installation

There is a Makefile to compile this:

    $ make

# Usage

Sensh doesn't automatically track, probe, or look up sensors; that is for a full
implementation. Instead, it expects you to look up the sensor and copy the ID
with your terminal emulator:

    lookup accel_cal
    sensh: accel_cal sensor found: A1392FDF217B7D9EI6648AED8C04DDFB9
    attr A1392FDF217B7D9EI6648AED8C04DDFB9
    sensh: name: ASH_CAL
    sensh: vendor: GOOGLE
    sensh: type: accel_cal
    sensh: version: 1
    sensh: api: sns_cal.proto
    sensh: rates: 10.000000
    sensh: stream type: 1
    sensh: physical sensor: 0
    sensh: available: 1

You can send 3 messages: lookup, attr, and watch. The lookup command returns
SUIDs for the passed data type. The attr command returns attributes for the
sensor. Finally, the watch command tells the sensor core to send events from a
sensor whenever the sensor's value changes. The watch command is only valid if
the stream type is 1 or 2.

An EOF (normally Ctrl+D) is enough to exit the shell unless the sensor core
died.

# References

The following sources were used as reference:

 - [https://android.googlesource.com/platform/system/chre](https://android.googlesource.com/platform/system/chre)
 - [https://github.com/calebccff/sensordaemon-re](https://github.com/calebccff/sensordaemon-re)
 - [https://gitlab.freedesktop.org/mobile-broadband/libqmi/-/merge\_requests/346](https://gitlab.freedesktop.org/mobile-broadband/libqmi/-/merge_requests/346)
 - [https://git.linaro.org/landing-teams/working/qualcomm/fastrpc.git](https://git.linaro.org/landing-teams/working/qualcomm/fastrpc.git)
