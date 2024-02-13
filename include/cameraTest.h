#ifndef __CAMERA_TEST_H__
#define __CAMERA_TEST_H__

#include <Arduino.h>
#include "esp_camera.h"
#include "sensor.h"
#include "esp_log.h"
#include "esp_system.h"

#include "config.h"

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

int camera_frame_size = FRAMESIZE_UXGA;
int camera_jpeg_quality = 5;
int camera_frame_buffer_count = 3;


typedef struct Camera_Frame_t
{
  uint8_t *framebuffer;
  int framebuffer_len;
  unsigned long framebuffer_time;
  unsigned long current_frame_time;
  unsigned long last_frame_time;
  int frame_interval;
  float most_recent_fps;
  int most_recent_avg_framesize;

  bool on_recording;

} Camera_Frame_t;


static void config_camera()
{

    camera_config_t config;

    Serial.println("config camera");

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
    config.pin_sccb_sda = SIOD_GPIO_NUM; //updated
    config.pin_sccb_scl = SIOC_GPIO_NUM; //updated
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000; // 10000000 or 20000000 -- 100 is faster with v1.04  // 200 is faster with v1.06 // 16500000 is an option

    config.pixel_format = PIXFORMAT_JPEG;
    // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition

    //   Serial.printf("Frame config %d, quality config %d, buffers config %d\n", camera_frame_size, camera_jpeg_quality, camera_frame_buffer_count);
    // config.frame_size = (framesize_t)camera_frame_size;
    config.frame_size = FRAMESIZE_UXGA;
    //   config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    //   config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = camera_jpeg_quality;
    config.fb_count = camera_frame_buffer_count;

    // https://github.com/espressif/esp32-camera/issues/357#issuecomment-1047086477
    // config.grab_mode      = CAMERA_GRAB_LATEST;

    // if (Lots_of_Stats)
    // {
    //     Serial.printf("Before camera config ...");
    //     Serial.printf("Internal Total heap %d, internal Free Heap %d, ", ESP.getHeapSize(), ESP.getFreeHeap());
    //     Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    // }

    esp_err_t cam_err = ESP_FAIL;
    int attempt = 5;
    while (attempt && cam_err != ESP_OK)
    {
        cam_err = esp_camera_init(&config);
        if (cam_err != ESP_OK)
        {
            Serial.printf("Camera init failed with error 0x%x", cam_err);
            digitalWrite(PWDN_GPIO_NUM, 1);
            delay(500);
            digitalWrite(PWDN_GPIO_NUM, 0); // power cycle the camera (OV2640)
            attempt--;
        }
    }

    // if (Lots_of_Stats)
    // {
    //     Serial.printf("After  camera config ...");
    //     Serial.printf("Internal Total heap %d, internal Free Heap %d, ", ESP.getHeapSize(), ESP.getFreeHeap());
    //     Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    // }

    if (cam_err != ESP_OK)
    {
        // major_fail();
        Serial.printf("Camera init failed with error 0x%x", cam_err);
    }

    sensor_t *ss = esp_camera_sensor_get();

    /// ss->set_hmirror(ss, 1);        // 0 = disable , 1 = enable
    /// ss->set_vflip(ss, 1);          // 0 = disable , 1 = enable

    Serial.printf("\nCamera started correctly, Type is %x (hex) of 9650, 7725, 2640, 3660, 5640\n\n", ss->id.PID);

    //check main code of ESP32 cam
    if (ss->id.PID == OV5640_PID)
    {
        // Serial.println("56 - going mirror");
        ss->set_hmirror(ss, 1); // 0 = disable , 1 = enable
    }
    else
    {
        ss->set_hmirror(ss, 0); // 0 = disable , 1 = enable
    }

    ss->set_quality(ss, camera_quality);
    ss->set_framesize(ss, (framesize_t)camera_framesize);

    ss->set_brightness(ss, 1);  // up the blightness just a bit
    ss->set_saturation(ss, -2); // lower the saturation

    delay(500);
    for (int j = 0; j < 10; j++)
    {
        camera_fb_t *fb = esp_camera_fb_get(); // get_good_jpeg();
        if (!fb)
        {
            Serial.println("Camera Capture Failed");
        }
        else
        {
            Serial.print("Pic, len=");
            Serial.print(fb->len);
            Serial.printf(", new fb %X\n", (long)fb->buf);
            esp_camera_fb_return(fb);
            delay(10);
        }
    }
    Serial.printf("End of setup ...");
    Serial.printf("Internal Total heap %d, internal Free Heap %d, ", ESP.getHeapSize(), ESP.getFreeHeap());
    Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());
}

#endif