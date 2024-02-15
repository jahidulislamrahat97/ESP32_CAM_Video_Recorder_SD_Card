#ifndef __AVI_H__
#define __AVI_H__

#include <Arduino.h>
#include <Camera.h>

#include <SD_MMC.h>
#include <FS.h>

#include <stdio.h>
#include <pgmspace.h>

#define AVIOFFSET 240 // AVI main header length
#define BUFFSIZE 512
// https://github.com/espressif/esp32-camera/issues/182
#define FBS 8 // was 64 -- how many kb of static ram for psram -> sram buffer for sd write

struct frameSizeStruct
{
  uint8_t frameWidth[2];
  uint8_t frameHeight[2];
};

static const frameSizeStruct frameSizeData[] = {
    {{0x60, 0x00}, {0x60, 0x00}}, // FRAMESIZE_96X96,    // 96x96
    {{0xA0, 0x00}, {0x78, 0x00}}, // FRAMESIZE_QQVGA,    // 160x120
    {{0xB0, 0x00}, {0x90, 0x00}}, // FRAMESIZE_QCIF,     // 176x144
    {{0xF0, 0x00}, {0xB0, 0x00}}, // FRAMESIZE_HQVGA,    // 240x176
    {{0xF0, 0x00}, {0xF0, 0x00}}, // FRAMESIZE_240X240,  // 240x240
    {{0x40, 0x01}, {0xF0, 0x00}}, // FRAMESIZE_QVGA,     // 320x240   framessize
    {{0x90, 0x01}, {0x28, 0x01}}, // FRAMESIZE_CIF,      // 400x296       bytes per buffer required in psram - quality must be higher number (lower quality) than config quality
    {{0xE0, 0x01}, {0x40, 0x01}}, // FRAMESIZE_HVGA,     // 480x320       low qual  med qual  high quality
    {{0x80, 0x02}, {0xE0, 0x01}}, // FRAMESIZE_VGA,      // 640x480   8   11+   ##  6-10  ##  0-5         indoor(56,COUNT=3)  (56,COUNT=2)          (56,count=1)
    //               38,400    61,440    153,600
    {{0x20, 0x03}, {0x58, 0x02}}, // FRAMESIZE_SVGA,     // 800x600   9                       240,000
    {{0x00, 0x04}, {0x00, 0x03}}, // FRAMESIZE_XGA,      // 1024x768  10
    {{0x00, 0x05}, {0xD0, 0x02}}, // FRAMESIZE_HD,       // 1280x720  11  115,200   184,320   460,800     (11)50.000  25.4fps   (11)50.000 12fps    (11)50,000  12.7fps
    {{0x00, 0x05}, {0x00, 0x04}}, // FRAMESIZE_SXGA,     // 1280x1024 12
    {{0x40, 0x06}, {0xB0, 0x04}}, // FRAMESIZE_UXGA,     // 1600x1200 13  240,000   384,000   960,000
    // 3MP Sensors
    {{0x80, 0x07}, {0x38, 0x04}}, // FRAMESIZE_FHD,      // 1920x1080 14  259,200   414,720   1,036,800   (11)210,000 5.91fps
    {{0xD0, 0x02}, {0x00, 0x05}}, // FRAMESIZE_P_HD,     //  720x1280 15
    {{0x60, 0x03}, {0x00, 0x06}}, // FRAMESIZE_P_3MP,    //  864x1536 16
    {{0x00, 0x08}, {0x00, 0x06}}, // FRAMESIZE_QXGA,     // 2048x1536 17  393,216   629,146   1,572,864
    // 5MP Sensors
    {{0x00, 0x0A}, {0xA0, 0x05}}, // FRAMESIZE_QHD,      // 2560x1440 18  460,800   737,280   1,843,200   (11)400,000 3.5fps    (11)330,000 1.95fps
    {{0x00, 0x0A}, {0x40, 0x06}}, // FRAMESIZE_WQXGA,    // 2560x1600 19
    {{0x38, 0x04}, {0x80, 0x07}}, // FRAMESIZE_P_FHD,    // 1080x1920 20
    {{0x00, 0x0A}, {0x80, 0x07}}  // FRAMESIZE_QSXGA,    // 2560x1920 21  614,400   983,040   2,457,600   (15)425,000 3.25fps   (15)382,000 1.7fps  (15)385,000 1.7fps
};

typedef struct AVI_Frame_t
{
  char avi_file_name[100];
  int avi_length;
  unsigned long avi_start_time;
  unsigned long avi_end_time;
  int speed_up_factor;
  unsigned long movi_size;
  unsigned long idx_offset;
} AVI_Frame_t;

typedef struct LOG_AVI_Frame_t
{
  uint32_t total_frame_len = 0;
  uint32_t total_clip_process_time;
  uint32_t elapsedms;
  unsigned long total_pic_cap_time;   
  unsigned long total_pic_write_time;
} LOG_AVI_Frame_t;

void end_avi(Camera_Frame_t &cam_frm, AVI_Frame_t &avi_frm, LOG_AVI_Frame_t &log_frm);
void another_save_avi(camera_fb_t *fb, Camera_Frame_t &cam_frm, AVI_Frame_t &avi_frm, LOG_AVI_Frame_t &log_frm);
void start_avi(Camera_Frame_t &cam_frm, AVI_Frame_t &avi_frm, LOG_AVI_Frame_t &log_frm, int camera_framesize);

#endif