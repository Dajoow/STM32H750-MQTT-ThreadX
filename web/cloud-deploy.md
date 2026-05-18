# Cloud Web + Cloud MQTT Setup

This setup uses a static web page plus a managed MQTT broker.

## Recommended Topology

```text
Browser on PC or phone
  -> HTTPS static web page
  -> MQTT over secure WebSocket, wss://
  -> Cloud MQTT broker
  -> MQTT device connection
  -> STM32
```

## 1. Create MQTT Broker

Recommended managed brokers:

- HiveMQ Cloud Serverless
- EMQX Cloud Serverless

Create one MQTT cluster, then create two MQTT users.

### Device User

Example:

```text
username: stm32_h750_device
password: use-a-long-random-password
```

Permissions:

```text
publish:   stm32/h750/status
publish:   stm32/h750/led/state
subscribe: stm32/h750/led/set
```

### Web User

Example:

```text
username: stm32_h750_web
password: use-another-long-random-password
```

Permissions:

```text
publish:   stm32/h750/led/set
subscribe: stm32/h750/status
subscribe: stm32/h750/led/state
```

This is the important topic split:

- The web page can send commands but cannot publish fake STM32 status.
- The STM32 can publish status but only subscribes to LED commands.

## 2. WebSocket URL

For a browser, use the broker's secure WebSocket address.

Common forms:

```text
wss://your-cluster-host:8884/mqtt
wss://your-cluster-host/mqtt
```

Use the exact WebSocket URL shown in your cloud MQTT console.

## 3. Deploy the Web Page

Deploy this folder:

```text
web/
```

The required file is:

```text
led-control.html
```

Good free static hosts:

- Cloudflare Pages
- GitHub Pages
- Vercel

For Cloudflare Pages:

1. Create a Pages project.
2. Upload the `web` folder or connect a Git repository.
3. Set the output directory to `web` if using repository deployment.
4. Open the deployed URL.
5. Fill the MQTT WebSocket URL, username, password, and topic prefix.

## 4. Protect the Web Page

Use Cloudflare Zero Trust Access in front of the Pages site.

Policy example:

```text
Application: your Pages domain
Access type: One-time PIN or allowed email list
Allowed users: your email addresses
```

This protects the page itself. The MQTT broker username/password and topic permissions still protect the MQTT side.

## 5. STM32 Firmware Settings

In `NetXDuo/App/app_netxduo.c`, set:

```c
#define MQTT_USERNAME              "stm32_h750_device"
#define MQTT_PASSWORD              "use-a-long-random-password"
```

Current firmware still connects to the local broker by IPv4 address:

```c
#define MQTT_BROKER_ADDRESS        IP_ADDRESS(192, 168, 50, 1)
#define MQTT_BROKER_PORT           NXD_MQTT_PORT
```

For a real cloud broker, the STM32 side still needs DNS plus TLS configured before using port `8883`.

## 6. Local Compatibility

The web page still works with local Mosquitto:

```text
ws://192.168.50.1:9001
```

Leave username and password empty when using the current local anonymous Mosquitto config.
