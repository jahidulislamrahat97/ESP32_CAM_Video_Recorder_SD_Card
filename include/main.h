#ifndef __MAIN_H__
#define __MAIN_H__


#include "config.h"
#include "Camera.h"
#include "SDCard.h"
#include "aviTest.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <EEPROM.h>

TaskHandle_t the_camera_loop_task;
TaskHandle_t the_sd_loop_task;

static SemaphoreHandle_t take_new_frame;
static SemaphoreHandle_t save_current_frame;

File avifile;
File idxfile;

Camera_Frame_t cam_frm;
AVI_Frame_t avi_frm;
LOG_AVI_Frame_t log_frm;



bool Take_New_Shot = true;


/***********************************************************************************************************************/

// int avi_frm.avi_length = 1800;    // how long a movie in seconds -- 1800 sec = 30 min
// int cam_frm.frame_interval = 0;   // record at full speed
// int avi_frm.speed_up_factor = 1;  // play at realtime

int MagicNumber = 12; // EEPORM purpose    // change this number to reset the eprom in your esp32 for file numbers


int file_number = 0;
int file_group = 0;

/**************************************************/
int camera_framesize = CAMERA_FRAME_SIZE;
int camera_quality = CAMERA_QUALITY;
int camera_jpeg_quality = CAMERA_JPEG_QUALITY;
int camera_frame_buffer_count = CAMERA_FRAME_BUFFER_COUNT;
/**************************************************/



void do_eprom_read();
void do_eprom_write();

void aviTask(void *pvParameter);


static void start_avi();
static void another_save_avi(camera_fb_t *fb);
static void end_avi();
static void inline print_quartet(unsigned long i, File fd);
static void inline print_2quartet(unsigned long i, unsigned long j, File fd);

void cameraTask(void *pvParameter);
camera_fb_t *capGoodJpeg();


#endif