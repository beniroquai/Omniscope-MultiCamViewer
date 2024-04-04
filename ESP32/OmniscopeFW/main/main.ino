#include <Arduino.h>
#include <Base64.h>
#include <base64.h>
#ifdef CAMERA_MODEL_XIAO
#include "esp_camera.h"
#endif
#include "camera_pins.h"
#include <esp_task_wdt.h>
#include "soc/soc.h"          // disable brownout detector
#include "soc/rtc_cntl_reg.h" // disable brownout detector

#include "driver/i2c.h"
#include "esp_camera.h"

#define I2C_SLAVE_SCL_IO D1      // Example SCL pin
#define I2C_SLAVE_SDA_IO D0      // Example SDA pin
#define I2C_SLAVE_NUM I2C_NUM_0  // I2C port number for the slave device
#define I2C_SLAVE_TX_BUF_LEN 256 //(2 * DATA_LENGTH)
#define I2C_SLAVE_RX_BUF_LEN 256 //(2 * DATA_LENGTH)
#define ESP_SLAVE_ADDR 0x28      // Slave address

long startTime = 0;
int lastBlink = 0;
int blinkState = 0;
int blinkPeriod = 500;


#define BAUDRATE 500000
#ifdef CAMERA_MODEL_XIAO
void initCamera()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector (Fehler bei WiFi Anmeldung beheben)

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  /*
     FRAMESIZE_UXGA (1600 x 1200)
    FRAMESIZE_QVGA (320 x 240)
    FRAMESIZE_CIF (352 x 288)
    FRAMESIZE_VGA (640 x 480)
    FRAMESIZE_SVGA (800 x 600)
    FRAMESIZE_XGA (1024 x 768)
    FRAMESIZE_SXGA (1280 x 1024)
  */
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (!psramFound())
  {
    Serial.println("PSRAM NOT FOUND");
    ESP.restart();
  }
  // Serial.println("PSRAM FOUND");
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.frame_size = FRAMESIZE_SVGA; // FRAMESIZE_VGA; // ; // iFRAMESIZE_UXGA;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    log_e("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  // Serial.println("Camera init success!");
}
#endif

// Initialize I2C as master
static esp_err_t i2c_slave_init()
{
  i2c_port_t i2c_slave_port = I2C_SLAVE_NUM;
  i2c_config_t conf_slave;
  conf_slave.sda_io_num = I2C_SLAVE_SDA_IO;
  conf_slave.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf_slave.scl_io_num = I2C_SLAVE_SCL_IO;
  conf_slave.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf_slave.mode = I2C_MODE_SLAVE;
  conf_slave.slave.addr_10bit_en = 0;
  conf_slave.slave.slave_addr = ESP_SLAVE_ADDR;
  i2c_param_config(i2c_slave_port, &conf_slave);
  return i2c_driver_install(i2c_slave_port, conf_slave.mode,
                            I2C_SLAVE_RX_BUF_LEN, I2C_SLAVE_TX_BUF_LEN, 0);
}

/**
 * Simulate capturing a frame and sending data based on camera ID.
 * In a real application, this function would capture and chunk camera data.
 */
void simulate_frame_capture_and_send(uint8_t camera_id)
{
  // Simulate different responses based on camera ID
  const char *response = (camera_id == 1) ? "Frame from Camera 1" : "Frame from Camera 2";

  // Log the response for demonstration purposes
  Serial.print("Sending: ");
  Serial.println(response);

  // Send the response back over I2C (simplified, real implementation would chunk data)
  i2c_slave_write_buffer(I2C_SLAVE_NUM, (uint8_t *)response, strlen(response), 1000 / portTICK_RATE_MS);
}

 

void setup()
{

  Serial.begin(BAUDRATE);
  Serial.println("Initializing I2C");
  i2c_slave_init();

  startTime = millis();
  Serial.println("Booting");
  pinMode(LED_BUILTIN, OUTPUT);
  // blink LED 10x
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
  }
  initCamera();
}

uint8_t rx_buffer[128]; // Buffer for received data
uint8_t inBuff[256];
void loop()
{

  // blink the LED
  if (millis() - lastBlink > blinkPeriod)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    lastBlink = millis();
  }


  int size = i2c_slave_read_buffer(I2C_SLAVE_NUM, rx_buffer, sizeof(rx_buffer), 1000 / portTICK_RATE_MS);
  if (size > 0)
  {
    // Give feedback
    uint8_t camera_id = rx_buffer[0];
    Serial.println("Received request for camera ID" + String(camera_id));
    
    // Send out frame 
    camera_fb_t *fb = esp_camera_fb_get();
    uint32_t frame_size = fb->len;
    Serial.println("Frame size: " + String(frame_size));
    // first send the size of the frame
    i2c_slave_write_buffer(I2C_SLAVE_NUM, (uint8_t*)&frame_size, sizeof(frame_size), 1000 / portTICK_RATE_MS);
    
    
    // then send the actual frame
    //i2c_slave_write_buffer(I2C_SLAVE_NUM, fb->buf, fb->len, 1000 / portTICK_RATE_MS);
    esp_camera_fb_return(fb);
    
  }
}
