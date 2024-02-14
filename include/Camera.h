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
camera_fb_t *capGoodJpeg(unsigned long *total_pic_cap_time, uint16_t *frame_count, int *camera_quality, int *normal_jpg, int* extend_jpg, int* bad_jpg);



//  capGoodJpeg()  - take a picture and make sure it has a good jpeg
// camera_fb_t *capGoodJpeg()
// {

//   camera_fb_t *fb;
//   int failures = 0;

//   do
//   {
//     int fblen = 0;
//     int foundffd9 = 0;
//     long bp = millis();

//     fb = esp_camera_fb_get();
//     if (!fb)
//     {
//       Serial.println("Camera Capture Failed");
//       failures++;
//     }
//     else
//     {

//       log_frm.total_pic_cap_time = log_frm.total_pic_cap_time + millis() - bp;
//       fblen = fb->len;

//       for (int j = 1; j <= 1025; j++)
//       {
//         if (fb->buf[fblen - j] != 0xD9)
//         {
//           // no d9, try next for
//         }
//         else
//         { // Serial.println("Found a D9");
//           if (fb->buf[fblen - j - 1] == 0xFF)
//           { // Serial.print("Found the FFD9, junk is "); Serial.println(j);
//             if (j == 1)
//             {
//               cam_frm.normal_jpg++;
//             }
//             else
//             {
//               cam_frm.extend_jpg++;
//             }
//             foundffd9 = 1;
//             break;
//           }
//         }
//       }

//       if (!foundffd9)
//       {
//         cam_frm.bad_jpg++;
//         Serial.printf("Bad jpeg, Frame %d, Len = %d \n", cam_frm.frame_count, fblen);

//         esp_camera_fb_return(fb);
//         failures++;
//       }
//       else
//       {
//         break;
//         // count up the useless bytes
//       }
//     }

//   } while (failures < 10); // normally leave the loop with a break()

//   // if we get 10 bad frames in a row, then quality parameters are too high - set them lower (+5), and start new movie
//   if (failures == 10)
//   {
//     Serial.printf("10 failures");

//     sensor_t *ss = esp_camera_sensor_get();
//     int qual = ss->status.quality;
//     ss->set_quality(ss, qual + 5);
//     camera_quality = qual + 5;
//     Serial.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
//     delay(1000);
//   }
//   return fb;
// }


#endif