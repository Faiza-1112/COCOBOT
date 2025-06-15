#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <driver/rtc_io.h>
#include <esp_log.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_camera.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WebSocketsServer.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>
#include <FS.h>
#include <SD_MMC.h>
#include <ESP32_Servo.h>

#ifndef camerapinconfig
#define camerapinconfig
#define pwdn_gpio_num     32
#define reset_gpio_num    -1
#define xclk_gpio_num     0
#define siod_gpio_num     26
#define sioc_gpio_num     27
#define y9_gpio_num       35
#define y8_gpio_num       34
#define y7_gpio_num       39
#define y6_gpio_num       36
#define y5_gpio_num       21
#define y4_gpio_num       19
#define y3_gpio_num       18
#define y2_gpio_num       5
#define vsync_gpio_num    25
#define href_gpio_num     23
#define pclk_gpio_num     22
#endif 

#ifndef onboardledpinconfig
#define onboardled        33
#endif

#ifndef flashpinconfig
#define flashpinconfig
#define flasher           4
#endif 

#ifndef servopinconfig
#define servopinconfig
#define servopan          13
#define servotilt         14
#endif 

#ifndef cutterpinconfig
#define cutterpinconfig
#define servocutter       12
#endif 

#ifndef motorpinconfig
#define motorpinconfig
#define motor1a           2
#define motor1b           15
#endif 

String index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

    <head>
        <meta charset="UTF-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title> PIPE INSPECTION ROBOT </title>
    </head>
    
    <body>

        <img id = "live" src = ' ' class = stream>

        <button class = "capture" id = "cap"><b> CAPTURE </b></button>

        <div class = "slidecontainer">

            <input type = "range" id = "flash" min = "1" max = "100" value = "0" setp = "1" class = "button">
            <p><b> FLASH: </b><span id = "cflash"></span></p>

            <input type = "range" id = "night" min = "1" max = "100" value = "0"  step = "1" class = "button">
            <p><b> NIGHT: </b><span id = "cnight" span></p>

            <input type = "range" id = "xaxis" min = "1" max = "180" value = "90" step = "1" class = "button">
            <p><b> XAXIS: </b><span id= "cxaxis"></span></p>

            <input type = "range" id = "yaxis" min = "1" max = "180" value = "90" step = "1" class = "button">
            <p><b> YAXIS: </b><span id = "cyaxis"></span></p>

        </div>

        <div class = "joystick">

            <button class = "stick forward" id = "for"></button>
            <button class = "stick reverse" id = "rev"></button>
            <button class = "stick right"   id = "rig"></button>
            <button class = "stick left"    id = "lef"></button>

        </div>
    
    </body>

    <style>

        .stream
        {
            position: absolute;
            border: 1px solid #ddd;
            border-radius: 4px;
            border-color: blueviolet;
            padding: 5px;
            width: 640px;
            height: 480px;
        }

        .capture
        {
            position: absolute;
            color: red;
            width: 130px;
            height: 40px;
            top: 525px;
        }

        .slidecontainer
        {
            position: absolute;
            color: black;
            width: 130px;
            height: 40px;
            top: 600px;
        }

        .joystick
        {
            position: absolute;
            top: 600px;
            left: 175px;
            width: 15rem; 
            height: 15rem; 
            display: grid;
            grid-gap: 0.5rem;
            grid-template-columns: 45fr 60fr 45fr;
            grid-template-rows: 45fr 60fr 45fr;
            grid-template-areas: "... for ..."
                                 "lef ... rig"
                                 "... rev ...";
        }

        .stick
        {
            color: rgb(55, 50, 50);
            background-color: gray;
            border: none;
            position: relative;
        }

        .stick::before
        {
            display: block;
            content: "";
            position: absolute;
            width: 4.25rem;
            height: 4.25rem;
            transform: rotate(45deg);
            background: gray;
        }

        .forward {grid-area: for; border-radius: 0.5rem 0.5rem 0, 0;}
        .forward::before {left: calc(50% - 2.125rem); bottom: -2.125rem;}
        .reverse {grid-area: rev; border-radius: 0 0 0.5rem 0.5rem;}
        .reverse::before {left: calc(50% - 2.125rem); top: -2.125rem;}
        .right {grid-area: rig; border-radius: 0 0.5rem 0.5rem 0;}
        .right::before {left: -2.125rem; top: calc(50% - 2.125rem);}
        .left {grid-area: lef; border-radius: 0.5rem 0 0 0.5rem;}
        .left::before {right: -2.125rem; top: calc(50% - 2.125rem);}

    </style>

    <script language = "javascript" type = "text/javascript">

        const text = '{"flash":0, "night":0, "xaxis":0, "yaxis":0}';
        var host   = "ws://server_ip:81";
        var socket = null;

        window.addEventListener("load", begin, false);
        setInterval(caption_update, 100);
        document.getElementById("flash").onchange = websocket_phrase;
        document.getElementById("night").onchange = websocket_phrase;
        document.getElementById("xaxis").onchange = websocket_phrase;
        document.getElementById("yaxis").onchange = websocket_phrase;

        document.getElementById("for").onmousedown = forward;
        document.getElementById("for").ontouchstart = forward;
        document.getElementById("for").onmouseup = stop;
        document.getElementById("for").ontouchend = stop;

        document.getElementById("rev").onmousedown = reverse;
        document.getElementById("rev").ontouchstart = reverse;
        document.getElementById("rev").onmouseup = stop;
        document.getElementById("rev").ontouchend = stop;

        document.getElementById("rig").onmousedown = right;
        document.getElementById("rig").ontouchstart = right;
        document.getElementById("rig").onmouseup = stop;
        document.getElementById("rig").ontouchend = stop;

        document.getElementById("lef").onmousedown = left;
        document.getElementById("lef").ontouchstart = left;
        document.getElementById("lef").onmouseup = stop;
        document.getElementById("lef").ontouchend = stop;

        function caption_update()
        {
            document.getElementById("cflash").innerHTML = document.getElementsByClassName("button").flash.value + '%';
            document.getElementById("cnight").innerHTML = document.getElementsByClassName("button").night.value + '%';
            document.getElementById("cxaxis").innerHTML = document.getElementsByClassName("button").xaxis.value + '\u00B0';
            document.getElementById("cyaxis").innerHTML = document.getElementsByClassName("button").yaxis.value + '\u00B0';
        }
    
        function begin()
        {
            if(('WebSocket') in window)
            {
                socket = new WebSocket(host);
                socket.binaryType = 'arraybuffer';
                socket.onopen = websocket_open;
                socket.onclose = websocket_close;
                socket.onmessage = websocket_event;
            }
            else alert("Your browser doesn't supported");
        }

        function websocket_open() { console.log("WebSocket established"); }
        function websocket_close() { console.log("WebSocket terminated"); }

        function websocket_event(msg)
        {
            var bytes  = new Uint8Array(msg.data);
            var len    = bytes.byteLength;
            var binary = '';

            for(var i = 0; i < len; i++)
            binary += String.fromCharCode(bytes[i]);

            if(len <= 1024)
            {
                console.log(binary);
                var obj = JSON.parse(binary);

                if(obj.flash) document.getElementById("flash").value = obj.flash;
                if(obj.night) document.getElementById("night").value = obj.night;
                if(obj.xaxis) document.getElementById("xaxis").value = obj.xaxis;
                if(obj.yaxis) document.getElementById("yaxis").value = obj.yaxis;
            }
            else document.getElementById("live").src = "data:image/jpg;base64," + window.btoa(binary);
        }

        function websocket_phrase()
        {
            const obj = JSON.parse(text);
            obj.flash = parseInt(flash.value);
            obj.night = parseInt(night.value);
            obj.xaxis = parseInt(xaxis.value);
            obj.yaxis = parseInt(yaxis.value);

            console.log(obj);
            socket.send(JSON.stringify(obj));
        }

        document.getElementById("cap").onclick = function() {socket.send("capture");}
        function forward() { socket.send("forward"); }
        function reverse() { socket.send("reverse"); }
        function right() { socket.send("right"); }
        function left() { socket.send("left"); }
        function stop() { socket.send("stop");}

    </script>

</html>
)rawliteral";

static const char *TAG = "ESP32";
const char *wifi_ssid = "ESP8266";
const char *wifi_pass  = "PIC16f877a";

const uint16_t jsonsize = 1024;
uint8_t websocketdata[jsonsize];
size_t websocketcount;

uint32_t picturecount;
uint8_t serialarray[jsonsize];
size_t serialcount;

WebSocketsServer websocket = WebSocketsServer(81);
WiFiServer server(80);

Servo pan;
Servo tilt;
Servo cutter;

esp_err_t issdinitialized;
esp_err_t iscaminitialize;
esp_err_t iswifiinitialized;

uint8_t flashvalue;
uint8_t nightvalue;
uint8_t activeclients;

uint32_t cutterinterval;
uint32_t routineinterval;
bool iscutteractivated = false;

static esp_err_t esp_camera_begin(void);
static esp_err_t esp_wifi_begin();
static esp_err_t esp_sd_begin(void);

static void esp_http_responce(void);
static void esp_websocket_event(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

static void esp_handle_capture(void);
static void esp_handle_stream(uint8_t *buf, size_t length);
static void writeservo(uint8_t pin, uint8_t pos);

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200); while(!Serial);
  
  #ifndef debug
  esp_log_level_set("*", ESP_LOG_NONE);
  #else
  esp_log_level_set(TAG, ESP_LOG_INFO);
  #endif 

  pinMode(flasher, OUTPUT);
  digitalWrite(flasher, LOW);

  pinMode(motor1a, OUTPUT);
  pinMode(motor1b, OUTPUT);
  digitalWrite(motor1a, HIGH);
  digitalWrite(motor1b, HIGH);

  iscaminitialize    = esp_camera_begin();
  iswifiinitialized  = esp_wifi_begin();
  //issdinitialized    = esp_sd_begin();

  if(iscaminitialize   == ESP_FAIL) ESP.restart();
  if(iswifiinitialized == ESP_FAIL) ESP.restart();

  JSONVar object;
  object["xaxis"]       = 90;
  object["yaxis"]       = 90;
  object["night"]       = (uint8_t)nightvalue;
  object["flash"]       = (uint8_t)flashvalue;
  String jsonstring     = JSON.stringify(object);
  websocketcount        = strlen(jsonstring.c_str());
  memcpy(websocketdata, jsonstring.c_str(), websocketcount);

  pan.attach(servopan, 544, 2400); pan.write(90); delay(20);
  tilt.attach(servotilt, 544, 2400); tilt.write(90); delay(20);
  cutter.attach(servocutter, 544, 2400); cutter.write(0); delay(20);

  server.begin();
  websocket.begin();
  websocket.onEvent(esp_websocket_event);

  for(uint8_t k = 0; k < 3; k++)
  {
    digitalWrite(flasher, HIGH); delay(250);
    digitalWrite(flasher, LOW); delay(250);
  }
}

void loop()
{
  esp_http_responce();
  websocket.loop();
  
  camera_fb_t *fb = esp_camera_fb_get();
  websocket.broadcastBIN(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if(iscutteractivated && (millis() > cutterinterval))
  {
    uint8_t lpos = cutter.read();
    if(lpos == 0) cutter.write(180);
    else cutter.write(0);
    cutterinterval = millis() + 1000;
  }
}

static esp_err_t esp_camera_begin(void)
{
  camera_config_t config;

  config.ledc_channel   = LEDC_CHANNEL_0; 
  config.ledc_timer     = LEDC_TIMER_0;
  config.pin_pwdn       = pwdn_gpio_num;
  config.pin_reset      = reset_gpio_num;
  config.xclk_freq_hz   = 20000000;
  config.pixel_format   = PIXFORMAT_JPEG;

  config.pin_d0         = y2_gpio_num;
  config.pin_d1         = y3_gpio_num;
  config.pin_d2         = y4_gpio_num;
  config.pin_d3         = y5_gpio_num;
  config.pin_d4         = y6_gpio_num;
  config.pin_d5         = y7_gpio_num;
  config.pin_d6         = y8_gpio_num;
  config.pin_d7         = y9_gpio_num;

  config.pin_xclk       = xclk_gpio_num;
  config.pin_pclk       = pclk_gpio_num;
  config.pin_vsync      = vsync_gpio_num;
  config.pin_href       = href_gpio_num;
  config.pin_sscb_sda   = siod_gpio_num;
  config.pin_sscb_scl   = sioc_gpio_num;

  config.frame_size     = FRAMESIZE_VGA;
  config.jpeg_quality   = 12; //20
  config.fb_count       = 2;  //1

  esp_err_t err = esp_camera_init(&config);
  if(err != ESP_OK) ESP_LOGI(TAG, "camera initialize failed...");
  return err;
}

static esp_err_t esp_wifi_begin()
{
    WiFi.begin(wifi_ssid, wifi_pass);

    long int wifitimer = millis();
    while(WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        if((wifitimer + 10000) < millis()) break;
    }
    
    if(WiFi.status() == WL_CONNECTED)
    {
        index_html.replace("server_ip", WiFi.localIP().toString());
        ESP_LOGI(TAG, "IP: %d.%d.%d.%d", WiFi.localIP().operator[](0), 
        WiFi.localIP().operator[](1), WiFi.localIP().operator[](2), WiFi.localIP().operator[](3));  
        WiFi.softAP(WiFi.localIP().toString(), wifi_pass);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t esp_sd_begin(void)
{
  if(SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT))
  {
    ESP_LOGI(TAG, "SD card mounted @ 1bit mode");
    pinMode(4, OUTPUT);  digitalWrite(4, LOW);
    pinMode(12, OUTPUT); digitalWrite(12, LOW);
    pinMode(13, OUTPUT); digitalWrite(13, LOW);

    if(SD_MMC.cardType() != CARD_NONE)
    {
      ESP_LOGI(TAG, "card type valid");
      return ESP_OK;
    }
    else
    {
      ESP_LOGI(TAG, "card type invalid");
      return ESP_FAIL;
    }
  }
  ESP_LOGI(TAG, "No SD card detected");
  return ESP_FAIL;
}

static void esp_http_responce(void)
{
  WiFiClient client = server.available();
  if(!client.connected() || !client.available()) return ;
  client.flush(); client.println(index_html); client.stop();
}

static void esp_websocket_event(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch(type)
  {
    case WStype_DISCONNECTED:
    ESP_LOGI(TAG, "[%u] Disconnected..!", num);
    break;

    case WStype_CONNECTED:
    ESP_LOGI(TAG, "[%u] Connected..!", num);
    websocket.broadcastBIN(websocketdata, websocketcount);
    break;

    case WStype_TEXT:
    digitalWrite(flasher, HIGH); delay(100); 
    ESP_LOGI(TAG, "message from client[%u]\r\n%s", num, (char*)payload);
    digitalWrite(flasher, LOW); delay(100);

    if(strstr((char*)payload, "forward") != NULL)
    {
      digitalWrite(motor1a, LOW);
      digitalWrite(motor1b, HIGH);
    }
    else if(strstr((char*)payload, "reverse") != NULL)
    {
      digitalWrite(motor1a, HIGH);
      digitalWrite(motor1b, LOW);
    }
    else if(strstr((char*)payload, "stop") != NULL)
    {
      digitalWrite(motor1a, HIGH);
      digitalWrite(motor1b, HIGH);
    }
    else if(strstr((char*)payload, "right") != NULL)
    {
      iscutteractivated = true;
    }
    else if(strstr((char*)payload, "left") != NULL)
    {
      iscutteractivated = false;
    }
    else 
    {
      Serial.println((char*)payload); 
      esp_handle_stream(payload, length);
    }
    break;

    default: break;
  }
}

static void esp_handle_capture(void)
{
  picturecount = EEPROM.readUInt(0x00);
  String path = "/picture" + String(picturecount) + ".jpg";

  ledcWrite(1, 255); delay(500);
  camera_fb_t *fb = esp_camera_fb_get();
  ledcWrite(1, map(flashvalue, 0, 100, 0, 255));

  if(!fb)
  {
    ESP_LOGI(TAG, "can't acquire buffer");
    esp_camera_fb_return(fb);
    return ;
  }

  if(issdinitialized == ESP_OK)
  {
    ESP_LOGI(TAG, "picture file: %s", path.c_str());
    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);

    if(!file)
    {
      ESP_LOGI(TAG, "file handling error...");
      return ;
    }
    file.write(fb->buf, fb->len);
    ESP_LOGI(TAG, "Image captured & stored");
    picturecount = picturecount + 1;
    EEPROM.writeUInt(0, picturecount);
    EEPROM.commit(); file.close();
  }

  esp_camera_fb_return(fb);
}

static void esp_handle_stream(uint8_t *buf, size_t length)
{
  uint8_t servopanpos;
  uint8_t servotiltpos;

  memset(websocketdata, '\0', sizeof(websocketdata));
  websocketcount = length; memcpy(websocketdata, buf, websocketcount);
  websocket.broadcastBIN(websocketdata, websocketcount);

  JSONVar object = JSON.parse((char*)websocketdata);
  if(object.hasOwnProperty("flash")) flashvalue = (uint8_t)object["flash"];
  if(object.hasOwnProperty("night")) nightvalue = (uint8_t)object["night"];
  if(object.hasOwnProperty("xaxis")) servopanpos = (uint8_t)object["xaxis"];
  if(object.hasOwnProperty("yaxis")) servotiltpos = (uint8_t)object["yaxis"];

  pan.write(servopanpos);
  tilt.write(servotiltpos);
}

static void writeservo(uint8_t pin, uint8_t pos)
{
  uint32_t requiredelay = map(pos, 0, 180, 544, 2400);
  pinMode(pin, OUTPUT); digitalWrite(pin, HIGH);
  delayMicroseconds(requiredelay); 
  digitalWrite(pin, LOW);
  delayMicroseconds(20000 - requiredelay);
}