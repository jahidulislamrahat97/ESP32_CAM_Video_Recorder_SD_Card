#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <Arduino.h>
#include "esp_camera.h"
#include "sensor.h"
#include "esp_log.h"
#include "esp_system.h"

// #include "config.h"

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



// int camera_framesize = CAMERA_FRAME_SIZE;
// int camera_quality = CAMERA_QUALITY;
// int camera_jpeg_quality = CAMERA_JPEG_QUALITY;
// int camera_frame_buffer_count = CAMERA_FRAME_BUFFER_COUNT;
#define CAMERA_FRAME_SIZE FRAMESIZE_HD
#define CAMERA_QUALITY 12
#define CAMERA_JPEG_QUALITY 5
#define CAMERA_FRAME_BUFFER_COUNT 3



typedef struct Camera_Frame_t
{
    camera_fb_t *current_frame_buffer = NULL;
    camera_fb_t *next_frame_buffer = NULL;
    uint8_t *framebuffer;
    int framebuffer_len;
    uint16_t frame_count;
    unsigned long current_frame_time;
    unsigned long last_frame_time;
    int frame_interval;
    float most_recent_fps;
    int most_recent_avg_framesize;

    bool on_recording;

    int bad_jpg;
    int extend_jpg;
    int normal_jpg;

} Camera_Frame_t;


void configCamera();

#endif