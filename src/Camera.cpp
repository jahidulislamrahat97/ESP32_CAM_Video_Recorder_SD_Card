#include "Camera.h"

void configCamera()
{
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
    config.pin_sccb_sda = SIOD_GPIO_NUM; // updated
    config.pin_sccb_scl = SIOC_GPIO_NUM; // updated
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000; // 10000000 or 20000000 -- 100 is faster with v1.04  // 200 is faster with v1.06 // 16500000 is an option
    config.pixel_format = PIXFORMAT_JPEG;
    // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
    // config.frame_size = (framesize_t)camera_frame_size;
    config.frame_size = FRAMESIZE_UXGA;
    // config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    // config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_count = CAMERA_FRAME_BUFFER_COUNT;
    // config.grab_mode      = CAMERA_GRAB_LATEST; // https://github.com/espressif/esp32-camera/issues/357#issuecomment-1047086477

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

    if (cam_err != ESP_OK)
    {
        Serial.printf("Camera init failed with error 0x%x", cam_err);
    }

    sensor_t *ss = esp_camera_sensor_get();
    // ss->set_hmirror(ss, 1);        // 0 = disable , 1 = enable
    // ss->set_vflip(ss, 1);          // 0 = disable , 1 = enable

    Serial.printf("\nCamera started correctly, Type is %x (hex) of 9650, 7725, 2640, 3660, 5640\n\n", ss->id.PID);

    if (ss->id.PID == OV5640_PID)
    {
        ss->set_hmirror(ss, 1); // 0 = disable , 1 = enable
    }
    else
    {
        ss->set_hmirror(ss, 0); // 0 = disable , 1 = enable
    }
    ss->set_quality(ss, CAMERA_QUALITY);
    ss->set_framesize(ss, (framesize_t)CAMERA_FRAME_SIZE);
    ss->set_brightness(ss, 1);  // up the blightness just a bit
    ss->set_saturation(ss, -2); // lower the saturation

    delay(500);
    for (int j = 0; j < 10; j++)
    {
        camera_fb_t *fb = esp_camera_fb_get();
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
}

camera_fb_t *capGoodJpeg(unsigned long *total_pic_cap_time, uint16_t *frame_count, int *camera_quality, int *normal_jpg, int *extend_jpg, int *bad_jpg)
{

    camera_fb_t *fb;
    int failures = 0;

    do
    {
        int fblen = 0;
        int foundffd9 = 0;
        long bp = millis();

        fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera Capture Failed");
            failures++;
        }
        else
        {

            total_pic_cap_time = total_pic_cap_time + millis() - bp;
            fblen = fb->len;

            for (int j = 1; j <= 1025; j++)
            {
                if (fb->buf[fblen - j] != 0xD9)
                {
                    // no d9, try next for
                }
                else
                { // Serial.println("Found a D9");
                    if (fb->buf[fblen - j - 1] == 0xFF)
                    { // Serial.print("Found the FFD9, junk is "); Serial.println(j);
                        if (j == 1)
                        {
                            normal_jpg++;
                        }
                        else
                        {
                            extend_jpg++;
                        }
                        foundffd9 = 1;
                        break;
                    }
                }
            }

            if (!foundffd9)
            {
                bad_jpg++;
                Serial.printf("Bad jpeg, Frame %d, Len = %d \n", frame_count, fblen);

                esp_camera_fb_return(fb);
                failures++;
            }
            else
            {
                break;
                // count up the useless bytes
            }
        }

    } while (failures < 10); // normally leave the loop with a break()

    // if we get 10 bad frames in a row, then quality parameters are too high - set them lower (+5), and start new movie
    if (failures == 10)
    {
        Serial.printf("10 failures");

        sensor_t *ss = esp_camera_sensor_get();
        int qual = ss->status.quality;
        ss->set_quality(ss, qual + 5);
        camera_quality = &qual + 5;
        Serial.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
        delay(1000);
    }
    return fb;
}
