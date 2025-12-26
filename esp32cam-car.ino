/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
//compilato con core esp32 by espressif 2.0.17

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include <ESP32Servo.h>

#include "index-html.h"
// Replace with your network credentials
const char* ssid = "myTPLINK";
const char* password = "MultiscanE220";

Servo servo;

#define PART_BOUNDARY "123456789000000000000987654321"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define DEBUG            true
#define LED_PIN          4
// MOTORE SINISTRO
#define MOTOR_1_PIN_1    13
#define MOTOR_1_PIN_2    12

// PWM
#define ENA_PIN          15   // PWM sinistra
// LEDC
#define PWM_LEFT_CH      3
#define PWM_FREQ         5000     // 15 kHz
#define PWM_RES          10        // 0..1023
//float kLeft  = 1.00;
int basePWM  = 800;               // 0..1023

// === CONFIGURAZIONE SERVO ===
#define SERVO_PIN 14
//#define SERVO_MIN_US 500    // SG90
//#define SERVO_MAX_US 2400
#define SERVO_MIN_US  900    // ≈ 45°
#define SERVO_MAX_US  2100   // ≈ 135°
#define SERVO_FREQ 50       // Hz
#define STEP_DELAY 10       // ms tra uno step e l'altro
int currentPos = 90;
unsigned long lastMove = 0;
const int moveInterval = 15; // ms

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;


static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      if(DEBUG){Serial.println("Camera capture failed");}
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            if(DEBUG){Serial.println("JPEG compression failed");}
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char buf[128];
  size_t buf_len;
  char variable[32] = {0,};
  char bpwm_str[16] = {0,}; // Buffer per bpwm

  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  // Usa il buffer locale, assicurandoti che la dimensione richiesta non superi la dimensione del buffer
  if (buf_len > 1 && buf_len <= sizeof(buf)) { // >>> Aggiunta del controllo <<<
    // Non c'è bisogno di malloc/free, si usa il buffer locale
    
    // NOTA: il terzo parametro è la dimensione massima del buffer, non buf_len
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) { 
      // Estrae il comando 'go' (direzione)
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
      // Estrae il valore di 'bpwm' (coefficiente per il motore destro)
      if (httpd_query_key_value(buf, "bpwm", bpwm_str, sizeof(bpwm_str)) == ESP_OK) {
         basePWM = atof(bpwm_str); 
      }
    } else {
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    // Rimosso free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  sensor_t * s = esp_camera_sensor_get();
  int res = 0;

  // Usa i nuovi valori kLeft e kRight per il calcolo del PWM
  //int pwmLeft  = constrain(int(basePWM * kLeft),  0, 1023);
  
  // Stampa a scopo di debug per verificare i valori ricevuti
  if(DEBUG){Serial.printf("Comando: %s, basePWM: %d\n", variable, basePWM);}
  
  if(!strcmp(variable, "forward")) {
    // ... (Logica per Forward)
    if(DEBUG){Serial.println("Forward");}
    //servo.write(90);
    //updateServo(90);
    moveServo(90, STEP_DELAY);
    digitalWrite(MOTOR_1_PIN_1, 1);
    digitalWrite(MOTOR_1_PIN_2, 0);
    ledcWrite(PWM_LEFT_CH,  basePWM);  
  }
  else if(!strcmp(variable, "left")) {
    // ... (Logica per Left)
    if(DEBUG){Serial.println("Left");}
    //servo.write(135);
    //updateServo(135);
    moveServo(135, STEP_DELAY);
    digitalWrite(MOTOR_1_PIN_1, 1);
    digitalWrite(MOTOR_1_PIN_2, 0);
    ledcWrite(PWM_LEFT_CH,  basePWM);   
  }
  else if(!strcmp(variable, "right")) {
    // ... (Logica per Right)
    if(DEBUG){Serial.println("Right");}
    //servo.write(45);
    //updateServo(45);
    moveServo(45, STEP_DELAY);
    digitalWrite(MOTOR_1_PIN_1, 1);
    digitalWrite(MOTOR_1_PIN_2, 0);
    ledcWrite(PWM_LEFT_CH,  basePWM);
  }
  else if(!strcmp(variable, "backward")) {
    // ... (Logica per Backward)
    if(DEBUG){Serial.println("Backward");}
    //servo.write(90);
    //updateServo(90);
    moveServo(90, STEP_DELAY);
    digitalWrite(MOTOR_1_PIN_1, 0);
    digitalWrite(MOTOR_1_PIN_2, 1);
    ledcWrite(PWM_LEFT_CH,  basePWM);   
  }
  else if(!strcmp(variable, "stop")) {
    // ... (Logica per Stop)
    if(DEBUG){Serial.println("Stop");}
    //servo.write(90);
    //updateServo(90);
    moveServo(90, 0);
    digitalWrite(MOTOR_1_PIN_1, 0);
    digitalWrite(MOTOR_1_PIN_2, 0);
    ledcWrite(PWM_LEFT_CH,  0);
  }
  else {
    res = -1;
  }

  if(res){
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

//STEP_DELAY=20
void moveServo(int targetPos, int stepDelayMs) {
  if(stepDelayMs<1){
    currentPos=targetPos;
    servo.write(targetPos);
    return;
  }

  if (currentPos < targetPos) {
    for (int pos = currentPos; pos <= targetPos; pos++) {
      servo.write(pos);
      delay(stepDelayMs);
    }
  } else {
    for (int pos = currentPos; pos >= targetPos; pos--) {
      servo.write(pos);
      delay(stepDelayMs);
    }
  }
  currentPos=targetPos;
}

void updateServo(int targetPos) {
  if(DEBUG){Serial.print("updateServo - currentPos: ");Serial.println(currentPos);}
  if (currentPos == targetPos) return;

  if (millis() - lastMove >= moveInterval) {
    lastMove = millis();
    currentPos += (currentPos < targetPos) ? 1 : -1;
    servo.write(currentPos);
  }
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  pinMode(MOTOR_1_PIN_1, OUTPUT);
  pinMode(MOTOR_1_PIN_2, OUTPUT);
  digitalWrite(MOTOR_1_PIN_1, 0);
  digitalWrite(MOTOR_1_PIN_2, 0);

  // Alloca esplicitamente un timer LEDC
  ESP32PWM::allocateTimer(0);
  //ESP32PWM::allocateTimer(1);

  // Configura il servo
  servo.setPeriodHertz(SERVO_FREQ);
  servo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo.write(90); // Porta subito il servo a 90°

  ledcSetup(PWM_LEFT_CH,  PWM_FREQ, PWM_RES);

  ledcAttachPin(ENA_PIN, PWM_LEFT_CH);
  // LEDC MODERNO
  //pwmLeftCh  = ledcAttach(ENA_PIN, PWM_FREQ, PWM_RES);
  //pwmRightCh = ledcAttach(ENB_PIN, PWM_FREQ, PWM_RES);


  if(DEBUG){
    Serial.begin(115200);
    Serial.setDebugOutput(false);
  }
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if(DEBUG){Serial.printf("Camera init failed with error 0x%x", err);}
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(DEBUG){Serial.print(".");}
  }
  if(DEBUG){
    Serial.println("");
    Serial.println("WiFi connected");
    
    Serial.print("Camera Stream Ready! Go to: http://");
    Serial.println(WiFi.localIP());
  }
  // Start streaming web server
  startCameraServer();
}

void loop() {
  
}