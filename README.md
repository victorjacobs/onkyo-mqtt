# Onkyo-mqtt

Controls an Onkyo A-9010 amplifier over MQTT using an ESP8266 dev board. Commands are sent over the amplifier's RI port.

- [Onkyo-mqtt](#onkyo-mqtt)
    - [Setup](#setup)
    - [Usage](#usage)
    - [Home Assistant](#home-assistant)
    - [How does it work](#how-does-it-work)

## Setup

1. Download [this library](https://github.com/docbender/Onkyo-RI/tree/master/Onkyo_send_blocking) to `~/Documents/Arduino/libraries/OnkyoRI`
2. Fill in the required constants in `onkyo-mqtt.ino`
3. Flash `onkyo-mqtt.ino` to an ESP8266 dev board
4. Create a cable that has a 3.5mm mono audio jack at one end, and header pins at the other
5. Connect the ground of the 3.5mm jack to any GND on the dev board, connect the data to any of the GPIO pins
    - The labeling of the GPIO pins on the NodeMCU board [is a bit weird](https://iotbytes.wordpress.com/nodemcu-pinout/)
    - Change the `ONKYO_PIN` constant in `onkyo-mqtt.ino` to the desired pin. By default it's set to 5, which on the NodeMCU board is `D1`
6. Plug the 3.5mm jack in the amp's RI port
7. Depending on which sources you want to use (analogue or digital), change the RI-related switches at the back of the amp (see next section)

## Usage

To send commands to the amplifier, publish the following messages to the `home/livingroom/amp/commands` topic:

```json
{
    "state": "on"
}
```

```json
{
    "state": "off"
}
```

To turn the amp on and off. Both commands are idempotent, sending `on` to an amp that's already on doesn't do anything.

```json
{
    "volume": "up"
}
```

```json
{
    "volume": "down"
}
```

To change the volume.

```json
{
    "source": "dock"
}
```

```json
{
    "source": "cd"
}
```

To change the source. The actual sources that are targeted with these generic names can be chosen by switches at the back of the amp. CD is either line 1 or digital 1. Dock is either line 2 or digital 2. More information is available [on page 8 of the user manual](http://www.intl.onkyo.com/downloads/manuals/pdf/a-9010_manual_en.pdf). Both of the source commands are idempotent.

Commands that change some state of the amplifier will also result in a message being published to either `home/livingroom/amp/state` or `home/livingroom/amp/source` topics. This is required to create a switch in Home Assistant. However, since there's no way to read the current state over RI, it is possible that what's on the topic is out of sync with the actual state of the amp (e.g. using the remote to change sources).

Finally there's a bit of logging information being written to the `home/livingroom/amp/logs` topic to help debugging.

## Home Assistant

To make your amp available in Home assistant, put the following in `configuration.yaml`:

```yaml
switch:
  - platform: mqtt
    name: "Amp"
    state_topic: "home/livingroom/amp/state"
    command_topic: "home/livingroom/amp/commands"
    payload_on: '{"state":"on"}'
    payload_off: '{"state":"off"}'
    icon: 'mdi:speaker'
```

## How does it work

More information on how the RI protocol actually works can be found [on the library's repository](https://github.com/docbender/Onkyo-RI). However, the commands that are listed there are not valid for the A-9010. It seems like the RI implementation varies wildly between devices.

I slightly modified the testing tool that's in the library's repository to scan through the entire command space to use MQTT. This modified version can be found in `onkyo-mqtt-test.ino`. It will loop over all commands, publishing the current one that's been sent to `home/livingroom/amp/live`. Controlling the process can be done by sending messages to the `home/livingroom/amp` topic:

- *p* to pause/resume the scan
- *Anything else* will be parsed as a hex string and sent to the amp (e.g. sending `2` will turn up the volume).

Going through all the commands, I've found the following for the A-9010:

| Command | Action |
| ---- | ---- |
| 0x002 | Volume up |
| 0x003 | Volume down |
| 0x004 | On/off toggle |
| 0x005 | Muting toggle |
| 0x020 | Change source to CD |
| 0x02f | Turn on |
| 0x0d5 | Next input |
| 0x0d6 | Previous input |
| 0x0da | Turn off |
| 0x0e3 | Line in |
| 0x170 | Change source to Dock |
| 0x503 | Muting toggle |