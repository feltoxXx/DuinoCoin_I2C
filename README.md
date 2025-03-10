# DuinoCoinI2C

> [!TIP]
> Using 14 RP2040 workers i see 8.8 shares/s. Almost 0.64 shares/s that you can get with the same worker with USB!

This project design to mine [Duino-Coin](https://github.com/revoxhere/duino-coin) using an Esp32 as a master and Arduino/RP2040 as a slave. 

Using the I2C communication to connect all the boards and make a scalable communication between the master and the slaves.

Visit youtube for video of [How to Make the DuinoCoinI2C Mining Rig](https://youtu.be/ErpdIaZk9EI)

Watch [Programming ESP-01 video](https://youtu.be/M6N-3RDhHj0)

## Version

DuinoCoinI2C Version 4.3

# Arduino - Slave

All Slaves have the same code and should set the I2C Address for each one.

Try with the [DuinoCoin_RPI_Tiny_Slave](https://github.com/JK-Rolling/DuinoCoinI2C_RPI/tree/main/DuinoCoin_RPI_Tiny_Slave) sketch

# RP2040 - Slave

RP2040 support dual core dual I2C. Dual core is counted as 2 workers

Try with the [DuinoCoin_RPI_Pico](https://github.com/feltoxXx/DuinoCoin_I2C/tree/main/workers/RP2040_Code) sketch

# ATTiny - Slave

ATTiny85 is tested to be working. You may try other ATTiny chip (modification maybe needed)

Waiting for test

## Library Dependency

* [ArduinoUniqueID](https://github.com/ricaun/ArduinoUniqueID) (Handle the chip ID)
* [StreamJoin](https://github.com/ricaun/StreamJoin) (StreamString for AVR)


## Automatic I2C Address 

This feature is strongly **NOT RECOMMENDED**

Instead use manual address assignment, change the value on the define for each device **(RECOMMENDED)**

```C
#define DEV_INDEX 0  // increment 1 per device
#define I2CS_START_ADDRESS 8
```

# Esp32 - Master

The master requests the job on the `DuinoCoin` server and sends the work to the slave (Arduino).

After the job is done, the slave sends back the response to the master (Esp8266/Esp32) and then sends back to the `DuinoCoin` server.

## Library Dependency

* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (Request Pool Version 2.6)

## Max Client/Slave

The code supports 15 clients due to a restriction on the server side. Mabe can handle 30 without problemsa.

Can be changed on the define:

```
#define CLIENTS 15
```

## AsyncWebserver

Not implemented - Next movement

# Connection Pinouts

As stated on the ESP32 pinout

---
