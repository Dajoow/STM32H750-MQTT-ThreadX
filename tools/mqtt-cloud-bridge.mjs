import mqtt from "mqtt";
import readline from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";

const config = {
  cloudUrl: process.env.CLOUD_MQTT_URL || "wss://43bd1840f54641e0a562882f103a010e.s1.eu.hivemq.cloud:8884/mqtt",
  cloudUsername: process.env.CLOUD_MQTT_USERNAME || "stm32_h750_web",
  cloudPassword: process.env.CLOUD_MQTT_PASSWORD || "",
  localUrl: process.env.LOCAL_MQTT_URL || "mqtt://192.168.50.1:1883",
  topicPrefix: process.env.MQTT_TOPIC_PREFIX || "stm32/h750",
};

const topics = {
  set: `${config.topicPrefix}/led/set`,
  state: `${config.topicPrefix}/led/state`,
  status: `${config.topicPrefix}/status`,
};

if (!config.cloudPassword) {
  const rl = readline.createInterface({ input, output });
  config.cloudPassword = await rl.question(`HiveMQ password for ${config.cloudUsername}: `);
  rl.close();
}

function connectClient(name, url, options) {
  const client = mqtt.connect(url, {
    ...options,
    protocolVersion: 4,
    clean: true,
    connectTimeout: 8000,
    reconnectPeriod: 2000,
  });

  client.on("connect", () => {
    console.log(`[${name}] connected`);
  });

  client.on("reconnect", () => {
    console.log(`[${name}] reconnecting`);
  });

  client.on("error", (error) => {
    console.log(`[${name}] error: ${error.message}`);
  });

  client.on("close", () => {
    console.log(`[${name}] closed`);
  });

  return client;
}

const cloud = connectClient("cloud", config.cloudUrl, {
  username: config.cloudUsername,
  password: config.cloudPassword,
  clientId: `pc_bridge_cloud_${Math.random().toString(16).slice(2)}`,
});

const local = connectClient("local", config.localUrl, {
  clientId: `pc_bridge_local_${Math.random().toString(16).slice(2)}`,
});

cloud.on("connect", () => {
  cloud.subscribe(topics.set, { qos: 0 }, (error) => {
    if (error) {
      console.log(`[cloud] subscribe failed: ${error.message}`);
      return;
    }
    console.log(`[cloud] subscribed ${topics.set}`);
  });
});

local.on("connect", () => {
  local.subscribe([topics.state, topics.status], { qos: 0 }, (error) => {
    if (error) {
      console.log(`[local] subscribe failed: ${error.message}`);
      return;
    }
    console.log(`[local] subscribed ${topics.state}, ${topics.status}`);
  });
});

cloud.on("message", (topic, payload) => {
  if (topic !== topics.set) {
    return;
  }

  const text = payload.toString();
  local.publish(topic, payload, { qos: 0, retain: false });
  console.log(`[cloud -> local] ${topic} ${text}`);
});

local.on("message", (topic, payload) => {
  if (topic !== topics.state && topic !== topics.status) {
    return;
  }

  const text = payload.toString();
  cloud.publish(topic, payload, { qos: 0, retain: false });
  console.log(`[local -> cloud] ${topic} ${text}`);
});

console.log("");
console.log("MQTT bridge running.");
console.log(`Cloud: ${config.cloudUrl}`);
console.log(`Local: ${config.localUrl}`);
console.log(`Cloud -> Local: ${topics.set}`);
console.log(`Local -> Cloud: ${topics.state}, ${topics.status}`);
console.log("Press Ctrl+C to stop.");
console.log("");

process.on("SIGINT", () => {
  console.log("\nStopping bridge...");
  cloud.end(true);
  local.end(true);
  setTimeout(() => process.exit(0), 200);
});
