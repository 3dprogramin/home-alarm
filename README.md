# Home Alarm

**Home Alarm** is an Arduino-based project designed for home security. This project utilizes an ESP32 microcontroller, a PIR sensor for motion detection, an RGB LED, and a keypad for user interactions. The system communicates with an Express.js server to ensure robust notifications and intruder alerts.

<p align="center">
<img src="/images/home-alarm.JPG">
</p>

## Project Structure
```perl
home-alarm/
├── src/               # Arduino code (PlatformIO)
├── server/            # Express.js server code
├── schematics/        # Wiring schematics (KiCad and PDF)
└── design/            # 3D design files (FreeCAD and .obj)
```

## Overview

The **Home Alarm** system operates as follows:
1. **Startup & Connection**: The ESP32 connects to a predefined WiFi network upon startup.
2. **Arming**: Press the * key on the keypad to activate the alarm. The system provides a 15-second grace period to exit the room without triggering the alarm.
3. **Detection & Notification**: 
   - If the PIR sensor detects motion while armed, the system initiates a 30-second window for PIN entry.
   - If the correct PIN is entered, the system disarms itself and sends a notification to the server to stop any further alerts.
   - Failure to enter the correct PIN within 30 seconds triggers the Express.js server to send an alert via Discord.
4. **Self-Arming**: If the system remains inactive for 12 hours and no motion is detected, it arms itself automatically.
5. **LED Indicators**:
   - **Green**: Powered on but not armed.
   - **Red**: Armed and ready.
   - **Blue**: Motion detected and awaiting PIN entry.
  
## Requirements

### Hardware
- **ESP32 Microcontroller**
- **PIR sensor**
- **RGB LED** (and 3 x 220 ohm resistors)
- **3x4 Matrix Keypad**
- **3D printer** for printing the case
  
### Software
- **PlatformIO** for Arduino code development
- **Node.js** for running the Express.js server
- **KiCad** for viewing/modifying schematics (*optional*)
- **FreeCAD** for viewing/modifying 3D design files (*optional*)

## Configuration

Before uploading the Arduino code, modify the following variables in `src/main.cpp`:

```cpp
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
const char *serverURL = "YOUR_API_SERVER_URL";
const char *serverToken = "YOUR_API_SERVER_TOKEN";  // something unique, make sure the same token is used in server
const char *alarmPin = "YOUR_ALARM_PIN";            // e.g 1234#
```

There are other configurations that can be modified to your own needs, most of them being defined with `#define`

## Server Configuration

1. Navigate to the `server/` directory
2. Copy the `.env.sample` file and rename it to `.env`
3. Modify the variables in the `.env` file as needed:

```bash
PORT=3000
TOKEN=your_server_token
TRIGGER_DELAY=30000
DISCORD_WEBHOOK_ACTIVATED=https://discord.com/api/webhooks/your_activated_webhook
DISCORD_WEBHOOK_TRIGGER=https://discord.com/api/webhooks/your_trigger_webhook
```

The `TOKEN` inside your arduino code and the token inside the server need to match. It is used to authenticate the API calls made from the device to the server.

## Installation

### Arduino Code

1. Ensure **PlatformIO** is installed in your IDE (e.g., VSCode).
2. Navigate to `src/` folder
3. Upload the code to your ESP32

### Server Code

1. Navigate to the `server/` folder
2. Run the following commands:

```bash
npm install
npm start
```

## Operation

1. Power on the device.
2. Once connected to WiFi, arm the system by pressing the `*` key.
3. Leave the room within 15 seconds.
4. If an intruder is detected, enter the correct PIN on the keypad within 30 seconds to disarm.
5. If not disarmed, the Express.js server sends a Discord notification.


## Schematics and Design

- **Schematics**: The wiring schematics are created in **KiCad** and rendered to PDF for easy viewing. The original KiCad project files are included for any custom modifications.
- **3D Design**: The design is created in **FreeCAD** and exported to .obj files for use in 3D printing or rendering. The original FreeCAD project files are also included for custom modifications.

## License

This project is open-source and available under the MIT License.

## Contributing

Feel free to fork the repository, submit pull requests, or open issues for improvements or bug fixes.