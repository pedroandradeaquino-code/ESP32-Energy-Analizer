import network
import socket
import uasyncio as asyncio
import ujson as json
import gc
import time

from machine import Pin, UART, reset

# /=========================================================
# CONFIGURAÇÕES
# =========================================================

SSID = "SU33"
PASSWORD = "12345678"

RELAY_PIN = 5
LED_PIN = 2

UART_TX = 17
UART_RX = 16

# =========================================================
# LIMITES
# =========================================================

MAX_VOLTAGE = 245.0
MIN_VOLTAGE = 190.0

MAX_CURRENT = 20.0
MIN_CURRENT = 0.01

TRIP_DELAY_MS = 3000
REARM_DELAY_MS = 5000

# =========================================================
# SISTEMA
# =========================================================

relay = Pin(RELAY_PIN, Pin.OUT)
relay.off()

led = Pin(LED_PIN, Pin.OUT)
led.off()

uart = UART(
    2,
    baudrate=9600,
    tx=Pin(UART_TX),
    rx=Pin(UART_RX),
    bits=8,
    parity=None,
    stop=1,
    timeout=1000
)

# =========================================================
# ESTADOS
# =========================================================

measurements = {
    "voltage": 0.0,
    "current": 0.0,
    "power": 0.0,
    "energy": 0.0,
    "frequency": 0.0,
    "pf": 0.0,
    "valid": False,
    "timestamp": 0
}

relay_state = False
protection_trip = False
trip_reason = "NONE"

last_valid_read = time.ticks_ms()
last_trip_time = 0

logs = []

# =========================================================
# FILTRO
# =========================================================

VOLTAGE_BUFFER = []
CURRENT_BUFFER = []
POWER_BUFFER = []

FILTER_SIZE = 5

# =========================================================
# HTML
# =========================================================

HTML = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Power Protection</title>

<style>
body{
background:#0f172a;
color:#e2e8f0;
font-family:Arial;
margin:0;
padding:0;
}

.header{
padding:20px;
text-align:center;
background:#111827;
font-size:24px;
font-weight:bold;
}

.container{
padding:15px;
display:grid;
grid-template-columns:repeat(auto-fit,minmax(150px,1fr));
gap:15px;
}

.card{
background:#1e293b;
padding:15px;
border-radius:12px;
box-shadow:0 0 10px rgba(0,0,0,0.3);
}

.value{
font-size:28px;
margin-top:10px;
font-weight:bold;
}

.status{
padding:10px;
margin:15px;
border-radius:10px;
text-align:center;
font-size:18px;
}

.ok{
background:#065f46;
}

.trip{
background:#7f1d1d;
}

button{
width:100%;
padding:14px;
margin-top:10px;
border:none;
border-radius:10px;
background:#2563eb;
color:white;
font-size:16px;
}

.logs{
margin:15px;
background:#111827;
padding:15px;
border-radius:10px;
max-height:300px;
overflow:auto;
font-size:13px;
}

.logline{
padding:4px;
border-bottom:1px solid #1e293b;
}
</style>
</head>

<body>

<div class="header">
ESP32 Power Protection
</div>

<div id="status" class="status ok">
SYSTEM OK
</div>

<div class="container">

<div class="card">
<div>Voltage</div>
<div class="value" id="voltage">0</div>
</div>

<div class="card">
<div>Current</div>
<div class="value" id="current">0</div>
</div>

<div class="card">
<div>Power</div>
<div class="value" id="power">0</div>
</div>

<div class="card">
<div>Energy</div>
<div class="value" id="energy">0</div>
</div>

<div class="card">
<div>Frequency</div>
<div class="value" id="frequency">0</div>
</div>

<div class="card">
<div>PF</div>
<div class="value" id="pf">0</div>
</div>

</div>

<div style="padding:15px">
<button onclick="rearm()">REARM</button>
<button onclick="relayToggle()">TOGGLE RELAY</button>
</div>

<div class="logs" id="logs"></div>

<script>

async function update(){

try{

let r = await fetch('/api/status');
let d = await r.json();

document.getElementById('voltage').innerText =
d.voltage.toFixed(1)+'V';

document.getElementById('current').innerText =
d.current.toFixed(2)+'A';

document.getElementById('power').innerText =
d.power.toFixed(1)+'W';

document.getElementById('energy').innerText =
d.energy.toFixed(3)+'kWh';

document.getElementById('frequency').innerText =
d.frequency.toFixed(1)+'Hz';

document.getElementById('pf').innerText =
d.pf.toFixed(2);

let s = document.getElementById('status');

if(d.trip){
s.innerText = 'TRIPPED: '+d.reason;
s.className = 'status trip';
}else{
s.innerText = 'SYSTEM OK';
s.className = 'status ok';
}

}catch(e){}

}

async function logs(){

try{

let r = await fetch('/api/logs');
let d = await r.json();

let html='';

for(let i=d.length-1;i>=0;i--){
html += '<div class="logline">'+d[i]+'</div>';
}

document.getElementById('logs').innerHTML = html;

}catch(e){}

}

async function rearm(){
await fetch('/api/reset');
}

async function relayToggle(){
await fetch('/api/rele');
}

setInterval(update,1000);
setInterval(logs,3000);

update();
logs();

</script>

</body>
</html>
"""

# =========================================================
# LOGS
# =========================================================

def add_log(msg):
    global logs

    timestamp = time.time()

    entry = "{} - {}".format(timestamp, msg)

    logs.append(entry)

    if len(logs) > 100:
        logs.pop(0)

# =========================================================
# RELÉ
# =========================================================

def relay_on():
    global relay_state

    relay.on()
    relay_state = True

def relay_off():
    global relay_state

    relay.off()
    relay_state = False

# =========================================================
# WIFI
# =========================================================

async def wifi_task():

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    while True:

        try:

            if not wlan.isconnected():

                add_log("WiFi reconnecting")

                wlan.connect(SSID, PASSWORD)

                timeout = 0

                while not wlan.isconnected():

                    await asyncio.sleep(1)

                    timeout += 1

                    if timeout > 20:
                        break

                if wlan.isconnected():
                    add_log("WiFi connected")

        except Exception as e:
            add_log("WiFi error")

        await asyncio.sleep(10)

# =========================================================
# PZEM
# =========================================================

def crc16(data):

    crc = 0xFFFF

    for pos in data:

        crc ^= pos

        for i in range(8):

            if (crc & 1):
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1

    return crc

def build_cmd():

    cmd = bytearray([
        0xF8,
        0x04,
        0x00,
        0x00,
        0x00,
        0x0A
    ])

    crc = crc16(cmd)

    cmd.append(crc & 0xFF)
    cmd.append((crc >> 8) & 0xFF)

    return cmd

def valid_value(v, mn, mx):
    return mn <= v <= mx

def moving_average(buffer, value):

    buffer.append(value)

    if len(buffer) > FILTER_SIZE:
        buffer.pop(0)

    return sum(buffer) / len(buffer)

async def pzem_task():

    global measurements
    global last_valid_read

    cmd = build_cmd()

    while True:

        try:

            uart.read()

            uart.write(cmd)

            await asyncio.sleep_ms(300)

            data = uart.read()

            if data and len(data) >= 25:

                voltage = ((data[3] << 8) | data[4]) / 10.0

                current = (
                    (
                        data[5] |
                        (data[6] << 8) |
                        (data[7] << 16) |
                        (data[8] << 24)
                    ) / 1000.0
                )

                power = (
                    (
                        data[9] |
                        (data[10] << 8) |
                        (data[11] << 16) |
                        (data[12] << 24)
                    ) / 10.0
                )

                energy = (
                    (
                        data[13] |
                        (data[14] << 8) |
                        (data[15] << 16) |
                        (data[16] << 24)
                    ) / 1000.0
                )

                frequency = (
                    ((data[17] << 8) | data[18]) / 10.0
                )

                pf = (
                    ((data[19] << 8) | data[20]) / 100.0
                )

                if (
                    valid_value(voltage, 0, 300) and
                    valid_value(current, 0, 100) and
                    valid_value(power, 0, 25000)
                ):

                    voltage = moving_average(
                        VOLTAGE_BUFFER,
                        voltage
                    )

                    current = moving_average(
                        CURRENT_BUFFER,
                        current
                    )

                    power = moving_average(
                        POWER_BUFFER,
                        power
                    )

                    measurements = {
                        "voltage": voltage,
                        "current": current,
                        "power": power,
                        "energy": energy,
                        "frequency": frequency,
                        "pf": pf,
                        "valid": True,
                        "timestamp": time.ticks_ms()
                    }

                    last_valid_read = time.ticks_ms()

                    led.value(not led.value())

                else:
                    add_log("Invalid reading")

            else:
                add_log("PZEM timeout")

        except Exception as e:
            add_log("PZEM error")

        gc.collect()

        await asyncio.sleep_ms(1000)

# =========================================================
# PROTEÇÃO
# =========================================================

async def protection_task():

    global protection_trip
    global trip_reason
    global last_trip_time

    while True:

        try:

            now = time.ticks_ms()

            if (
                time.ticks_diff(now, last_valid_read)
                > 5000
            ):

                if not protection_trip:

                    protection_trip = True
                    trip_reason = "PZEM LOST"

                    relay_off()

                    add_log("TRIP PZEM LOST")

            if measurements["valid"]:

                voltage = measurements["voltage"]
                current = measurements["current"]

                if voltage > MAX_VOLTAGE:

                    if not protection_trip:

                        protection_trip = True
                        trip_reason = "OVER VOLTAGE"

                        relay_off()

                        add_log("TRIP OVER VOLTAGE")

                elif voltage < MIN_VOLTAGE:

                    if not protection_trip:

                        protection_trip = True
                        trip_reason = "UNDER VOLTAGE"

                        relay_off()

                        add_log("TRIP UNDER VOLTAGE")

                elif current > MAX_CURRENT:

                    if not protection_trip:

                        protection_trip = True
                        trip_reason = "OVER CURRENT"

                        relay_off()

                        add_log("TRIP OVER CURRENT")

                elif current < MIN_CURRENT:

                    pass

        except Exception as e:
            add_log("Protection error")

        await asyncio.sleep_ms(500)

# =========================================================
# WATCHDOG
# =========================================================

async def watchdog_task():

    while True:

        try:

            gc.collect()

            if gc.mem_free() < 15000:
                add_log("LOW MEMORY")

        except:
            pass

        await asyncio.sleep(15)

# =========================================================
# HTTP
# =========================================================

async def http_response(writer, content, ctype):

    writer.write(
        "HTTP/1.1 200 OK\r\n"
    )

    writer.write(
        "Content-Type: {}\r\n".format(ctype)
    )

    writer.write(
        "Connection: close\r\n\r\n"
    )

    writer.write(content)

    await writer.drain()

async def handle_client(reader, writer):

    global protection_trip

    try:

        req = await reader.readline()

        if not req:
            await writer.aclose()
            return

        req = req.decode()

        path = req.split(' ')[1]

        while True:

            line = await reader.readline()

            if line == b"\r\n":
                break

        if path == "/":

            await http_response(
                writer,
                HTML,
                "text/html"
            )

        elif path == "/api/status":

            payload = json.dumps({
                "voltage": measurements["voltage"],
                "current": measurements["current"],
                "power": measurements["power"],
                "energy": measurements["energy"],
                "frequency": measurements["frequency"],
                "pf": measurements["pf"],
                "relay": relay_state,
                "trip": protection_trip,
                "reason": trip_reason
            })

            await http_response(
                writer,
                payload,
                "application/json"
            )

        elif path == "/api/logs":

            payload = json.dumps(logs)

            await http_response(
                writer,
                payload,
                "application/json"
            )

        elif path == "/api/reset":

            protection_trip = False

            add_log("MANUAL RESET")

            await http_response(
                writer,
                '{"ok":true}',
                "application/json"
            )

        elif path == "/api/rele":

            if not protection_trip:

                if relay_state:
                    relay_off()
                else:
                    relay_on()

            await http_response(
                writer,
                '{"ok":true}',
                "application/json"
            )

        else:

            writer.write(
                "HTTP/1.1 404 NOT FOUND\r\n\r\n"
            )

        await writer.drain()

    except Exception as e:
        pass

    try:
        await writer.aclose()
    except:
        pass

# =========================================================
# WEB SERVER
# =========================================================

async def webserver_task():

    server = await asyncio.start_server(
        handle_client,
        "0.0.0.0",
        80
    )

    add_log("Webserver started")

    while True:
        await asyncio.sleep(3600)

# =========================================================
# MAIN
# =========================================================

async def main():

    relay_off()

    add_log("System boot")

    asyncio.create_task(wifi_task())

    asyncio.create_task(pzem_task())

    asyncio.create_task(protection_task())

    asyncio.create_task(watchdog_task())

    asyncio.create_task(webserver_task())

    while True:

        await asyncio.sleep(1)

try:

    asyncio.run(main())

except Exception as e:

    add_log("FATAL ERROR")

    relay_off()

    time.sleep(5)

    reset()