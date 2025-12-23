# Mochi State API Specification

The MiBuddy app can query an external HTTP server to determine the mochi avatar's state.
This enables AI-driven or cloud-based decision making for the avatar's emotions.

## Overview

```
┌─────────────────┐     POST /mochi/state      ┌─────────────────┐
│   ESP32-C6      │ ──────────────────────────→│   Your Server   │
│   MiBuddy App   │     {inputs as JSON}       │   (AI/Logic)    │
│                 │ ←──────────────────────────│                 │
└─────────────────┘   {state, activity}        └─────────────────┘
```

## Endpoint

```
POST /mochi/state
Content-Type: application/json
```

## Request Body

```json
{
  "battery": 85.0,
  "charging": false,
  "hour": 14,
  "minute": 30,
  "moving": false,
  "shaking": false,
  "night": false,
  "low_battery": false,
  "wifi": true,
  "touch": false
}
```

### Request Fields

| Field | Type | Description |
|-------|------|-------------|
| `battery` | float | Battery percentage (0-100) |
| `charging` | bool | USB power connected |
| `hour` | int | Hour of day (0-23) |
| `minute` | int | Minute (0-59) |
| `moving` | bool | Device in motion (accel > 0.3g deviation from rest) |
| `shaking` | bool | Device being shaken vigorously (accel > 2g) |
| `night` | bool | Nighttime hours (22:00-06:00) |
| `low_battery` | bool | Battery below 20% |
| `wifi` | bool | WiFi currently connected |
| `touch` | bool | Touch screen currently pressed |

## Response Body

```json
{
  "state": "EXCITED",
  "activity": "BOUNCE"
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `state` | string | Emotional state (see values below) |
| `activity` | string | Animation activity (see values below) |

### Valid State Values

| State | Description | Typical Use |
|-------|-------------|-------------|
| `HAPPY` | Default positive | Normal/idle |
| `EXCITED` | High energy | Good news, activity |
| `WORRIED` | Concerned | Low battery, warnings |
| `COOL` | Confident/chill | Relaxed state |
| `DIZZY` | Confused | Error states |
| `PANIC` | Alarmed | Urgent alerts |
| `SLEEPY` | Tired | Night time, idle |
| `SHOCKED` | Surprised | Unexpected events |

### Valid Activity Values

| Activity | Description | Animation |
|----------|-------------|-----------|
| `IDLE` | Gentle breathing | Subtle face squish |
| `SHAKE` | Rapid left-right | 10Hz horizontal |
| `BOUNCE` | Up-down bouncing | 3Hz vertical |
| `SPIN` | Slow rotation | 0.5Hz spin |
| `WIGGLE` | Side-to-side wobble | 4Hz rotation |
| `NOD` | Up-down nod | 2Hz vertical |
| `BLINK` | Periodic eye blinks | 3s interval |
| `SNORE` | Breathing + zzz | For sleepy state |
| `VIBRATE` | Fast micro-shake | 30Hz for panic |

## Response Codes

| Code | Meaning | Client Behavior |
|------|---------|-----------------|
| 200 | Success | Use returned state/activity |
| 4xx | Client error | Fall back to HAPPY/IDLE |
| 5xx | Server error | Fall back to HAPPY/IDLE |
| Timeout | No response (5s) | Fall back to HAPPY/IDLE |

## Example Implementations

### Python (Flask)

```python
from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/mochi/state', methods=['POST'])
def mochi_state():
    data = request.json

    # Example logic
    if data.get('shaking'):
        return jsonify({"state": "DIZZY", "activity": "SHAKE"})

    if data.get('low_battery'):
        return jsonify({"state": "WORRIED", "activity": "IDLE"})

    hour = data.get('hour', 12)
    if 9 <= hour < 17:  # Work hours
        return jsonify({"state": "COOL", "activity": "IDLE"})

    return jsonify({"state": "HAPPY", "activity": "BOUNCE"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

### Node.js (Express)

```javascript
const express = require('express');
const app = express();
app.use(express.json());

app.post('/mochi/state', (req, res) => {
    const { battery, hour, moving, shaking } = req.body;

    if (shaking) {
        return res.json({ state: 'PANIC', activity: 'VIBRATE' });
    }

    if (battery < 10) {
        return res.json({ state: 'WORRIED', activity: 'IDLE' });
    }

    if (moving) {
        return res.json({ state: 'EXCITED', activity: 'BOUNCE' });
    }

    res.json({ state: 'HAPPY', activity: 'IDLE' });
});

app.listen(8080, '0.0.0.0');
```

### Python with OpenAI

```python
from flask import Flask, request, jsonify
import openai

app = Flask(__name__)
openai.api_key = "your-api-key"

STATES = ["HAPPY", "EXCITED", "WORRIED", "COOL", "DIZZY", "PANIC", "SLEEPY", "SHOCKED"]
ACTIVITIES = ["IDLE", "SHAKE", "BOUNCE", "SPIN", "WIGGLE", "NOD", "BLINK", "SNORE", "VIBRATE"]

@app.route('/mochi/state', methods=['POST'])
def mochi_state():
    data = request.json

    prompt = f"""You are controlling a cute mochi avatar's emotions.
Current sensor data: {data}
Choose the most appropriate emotional state and activity.
States: {STATES}
Activities: {ACTIVITIES}
Respond with JSON only: {{"state": "...", "activity": "..."}}"""

    response = openai.ChatCompletion.create(
        model="gpt-3.5-turbo",
        messages=[{"role": "user", "content": prompt}],
        max_tokens=50
    )

    # Parse AI response (add error handling in production)
    import json
    result = json.loads(response.choices[0].message.content)
    return jsonify(result)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

## Configuration

Set the API URL in `lvgl_app_mibuddy.cpp`:

```c
mochi_input_set_api_url("http://YOUR_SERVER_IP:8080/mochi/state");
```

The API is called in the default mapper when no other conditions match and WiFi is connected.
If the API fails or times out, the avatar falls back to HAPPY/IDLE.

## Timing

- API is called every 100ms (10Hz) when in default state
- HTTP timeout: 5 seconds
- State only changes when the returned state differs from current
