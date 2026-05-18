# MQTT Cloud Bridge

This bridge runs on the PC.

```text
HiveMQ Cloud <-> PC bridge <-> local Mosquitto <-> STM32
```

It forwards:

```text
cloud stm32/h750/led/set     -> local stm32/h750/led/set
local stm32/h750/status      -> cloud stm32/h750/status
local stm32/h750/led/state   -> cloud stm32/h750/led/state
```

## Run

Open PowerShell:

```powershell
cd C:\Users\DJ\Desktop\ThreadX_MQTT_H750_Template_clean
node .\tools\mqtt-cloud-bridge.mjs
```

When prompted, enter the HiveMQ password for:

```text
stm32_h750_web
```

Keep the PowerShell window open while testing.

## Test

1. Make sure STM32 still connects to local Mosquitto at `192.168.50.1:1883`.
2. Run the bridge script.
3. Open `web/led-control.html`.
4. Connect the web page to HiveMQ Cloud.
5. Press `ON`, `OFF`, or `TOGGLE`.

Expected:

- STM32 PB1 LED changes.
- Web page receives `stm32/h750/led/state`.

## Override Parameters

Example:

```powershell
$env:CLOUD_MQTT_URL="wss://43bd1840f54641e0a562882f103a010e.s1.eu.hivemq.cloud:8884/mqtt"
$env:CLOUD_MQTT_USERNAME="stm32_h750_web"
$env:MQTT_TOPIC_PREFIX="stm32/h750"
$env:LOCAL_MQTT_URL="mqtt://192.168.50.1:1883"
node .\tools\mqtt-cloud-bridge.mjs
```
