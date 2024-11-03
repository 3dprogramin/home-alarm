const express = require("express");
const logger = require("morgan");
const axios = require("axios");
const dotenv = require("dotenv");
dotenv.config();
const app = express();
const port = parseInt(process.env.PORT);

// do not change here, use .env file
const TOKEN = process.env.TOKEN;
const TRIGGER_DELAY = parseInt(process.env.TRIGGER_DELAY);
const DISCORD_WEBHOOK_ACTIVATED = process.env.DISCORD_WEBHOOK_ACTIVATED;
const DISCORD_WEBHOOK_TRIGGER = process.env.DISCORD_WEBHOOK_TRIGGER;

let discordTimeout = null;

// logging of requests
app.use(logger("dev"));

// middleware to check if the token is valid
app.use((req, res, next) => {
  if (req.query.token !== TOKEN) {
    return res.status(401).send("Unauthorized");
  }
  next();
});

// send a notification to Discord
async function sendDiscordNotification(webhook, message) {
  try {
    await axios.post(webhook, {
      content: message,
    });
    console.log("[+] Notification sent to Discord");
  } catch (error) {
    console.error(`[-] Error sending Discord notification: ${error.message}`);
  }
  discordTimeout = null;
}

// called when alarm is activated
app.get("/alarm/activate", (req, res) => {
  try {
    sendDiscordNotification(DISCORD_WEBHOOK_ACTIVATED, "Activated 🔑");
    console.log("[+] Alarm activated");
    res.send("Alarm activated");
  } catch (err) {
    console.error(`[-] Error activating alarm: ${err.message}`);
  }
});

// called when motion is detected
app.get("/alarm/trigger", (req, res) => {
  try {
    // check if it's already triggered
    if (discordTimeout) {
      clearTimeout(discordTimeout);
      discordTimeout = null;
      console.log("[+] Alarm re-triggered while it was already triggered");
    }
    // set the timeout for sending the Discord notification
    discordTimeout = setTimeout(() => {
      sendDiscordNotification(DISCORD_WEBHOOK_TRIGGER, "🚨 Intruder detected! 🚨");
    }, TRIGGER_DELAY);
    console.log("[+] Alarm triggered");
    res.send("Alarm triggered");
  } catch (err) {
    console.error(`[-] Error triggering alarm: ${err.message}`);
  }
});

// called when PIN is entered correctly
app.get("/alarm/stop", (req, res) => {
  try {
    // if we have a notification in progress, stop it
    // the user has typed the PIN correctly
    if (discordTimeout) {
      console.log("[+] Alarm stopped");
      res.send("Alarm stopped");
      clearTimeout(discordTimeout);
      discordTimeout = null;
      return;
    }
    console.log("[+] Alarm was not triggered");
    return res.send("Alarm was not triggered");
  } catch (err) {
    console.error(`[-] Error stopping alarm: ${err.message}`);
  }
});

// start the server
app.listen(port, () => {
  console.log(`[+] Home alarm server listening on port ${port}`);
});
