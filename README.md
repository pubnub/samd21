# PubNub for Atmel's SAM D21 Usage Notes and Client SDK Examples

The examples in `examples` directory are using the Atmel's ASF libraries. No RTOS is used.

The Pubnub client is derived from the Contiki OS client, using the winc socket interface.
Its source code is in `examples/Code/pubnubAtmelExample/src/Pubnub.c`. In the header (`Pubnub.h`) there are a few (documented) macros that the user can change to suit a particular application. For buffer/message size constraints, look at `PUBNUB_BUF_MAXLEN` and `PUBNUB_REPLY_MAXLEN`.

This Pubnub client support only the basic features: *publish* and *subscribe* to a channel (or a list of channels, but no channel group(s)) and *leave* (a (list of) channel(s)), which is suitable for most embedded applications.

Detailed instructions on how to connect all the HW, compile & upload the SW/FW and run the examples are in `examples/README.md`.
