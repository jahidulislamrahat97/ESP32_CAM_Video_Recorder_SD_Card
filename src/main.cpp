#include <Arduino.h>

#include "cameraTest.h"
#include "aviTest.h"
#include "sdTest.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "soc/soc.h"
#include "soc/cpu.h"
#include "soc/rtc_cntl_reg.h"

#include <EEPROM.h>

TaskHandle_t the_camera_loop_task;
TaskHandle_t the_sd_loop_task;

static SemaphoreHandle_t take_new_frame;
static SemaphoreHandle_t save_current_frame;

static const char vernum[] = "v60.4.7";
char devname[30];
String devstr = "desklens";

bool reboot_now = false;  // need modification then can delete
bool restart_now = false; // need modification then can delete

// https://github.com/espressif/esp32-camera/issues/182
#define fbs 8 // was 64 -- how many kb of static ram for psram -> sram buffer for sd write
uint8_t framebuffer_static[fbs * 1024 + 20];

Camera_Frame_t cam_frm;
AVI_Frame_t avi_frm;

camera_fb_t *fb_curr = NULL;
camera_fb_t *fb_next = NULL;

// int avi_frm.avi_length = 1800;    // how long a movie in seconds -- 1800 sec = 30 min
// int cam_frm.frame_interval = 0;   // record at full speed
// int avi_frm.speed_up_factor = 1;  // play at realtime
int MagicNumber = 12; // EEPORM purpose    // change this number to reset the eprom in your esp32 for file numbers

long bytes_before_last_100_frames = 0;
long time_before_last_100_frames = 0;

File avifile;
File idxfile;

char avi_file_name[100];

static int i = 0;
uint16_t frame_cnt = 0;
uint32_t length = 0;
uint32_t startms;
uint32_t elapsedms;
uint32_t uVideoLen = 0;

int bad_jpg = 0;
int extend_jpg = 0;
int normal_jpg = 0;

int file_number = 0;
int file_group = 0;

long totalp; // may be total picture
long totalw; // may be total wait time

#define BUFFSIZE 512

uint8_t buf[BUFFSIZE];

unsigned long movi_size = 0;
//  = 0;
unsigned long idx_offset = 0;

uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t dc_buf[4] = {0x30, 0x30, 0x64, 0x63}; // "00dc"
// uint8_t dc_and_zero_buf[8] = { 0x30, 0x30, 0x64, 0x63, 0x00, 0x00, 0x00, 0x00 };

// uint8_t avi1_buf[4] = { 0x41, 0x56, 0x49, 0x31 };  // "AVI1"
uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31}; // "idx1"

void do_eprom_read();
void do_eprom_write();

void the_sd_loop(void *pvParameter);
static esp_err_t init_sdcard();
void listDir(const char *dirname, uint8_t levels);
void deleteFolderOrFile(const char *val);
void delete_old_stuff();

static void start_avi();
static void another_save_avi(camera_fb_t *fb);
static void end_avi();
static void inline print_quartet(unsigned long i, File fd);
static void inline print_2quartet(unsigned long i, unsigned long j, File fd);

void the_camera_loop(void *pvParameter);
camera_fb_t *get_good_jpeg();

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(4, OUTPUT);   // Blinding Disk-Avtive Light
  digitalWrite(4, LOW); // turn off

  // cam_frm.framebuffer_time = 0;
  cam_frm.current_frame_time = 0;
  cam_frm.last_frame_time = 0;
  cam_frm.frame_interval = 0;
  cam_frm.most_recent_fps = 0.0;
  cam_frm.most_recent_avg_framesize = 0;

  // cam_frm.start_record = false;
  // cam_frm.end_record = false;
  cam_frm.on_recording = false;

  avi_frm.avi_length = MAX_VIDEO_LENGTH_SEC;
  avi_frm.speed_up_factor = 1;

  avi_frm.avi_start_time = 0;
  avi_frm.avi_end_time = 0;
  

  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.printf("ESP32-CAM-Video-Recorder-junior %s\n", vernum);
  Serial.println("-------------------------------------");

  do_eprom_read();

  // SD camera init
  Serial.println("Mounting the SD card ...");
  esp_err_t card_err = init_sdcard();
  if (card_err != ESP_OK)
  {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    return;
  }

  Serial.println("Try to get parameters from config.txt ...");

  Serial.println("Setting up the camera ...");
  config_camera();

  Serial.println("Checking SD for available space ...");
  delete_old_stuff();

  cam_frm.framebuffer = (uint8_t *)ps_malloc(512 * 1024); // buffer to store a jpg in motion // needs to be larger for big frames from ov5640

  take_new_frame = xSemaphoreCreateBinary();
  save_current_frame = xSemaphoreCreateBinary();

  xTaskCreatePinnedToCore(the_camera_loop, "the_camera_loop", 3000, NULL, 6, &the_camera_loop_task, 0); // prio 3, core 0 //v56 core 1 as http dominating 0 ... back to 0, raise prio
  delay(100);
  xTaskCreatePinnedToCore(the_sd_loop, "the_sd_loop", 2000, NULL, 4, &the_sd_loop_task, 1); // prio 4, core 1
  delay(200);

  Serial.println("  End of setup()\n\n");
}

void loop()
{
}

/*********************************************************************************/
/************************    EEPROM    *******************************************/
/*********************************************************************************/

struct eprom_data
{
  int eprom_good;
  int file_group;
};

void do_eprom_write()
{

  eprom_data ed;
  ed.eprom_good = MagicNumber;
  ed.file_group = file_group;

  Serial.println("Writing to EPROM ...");

  EEPROM.begin(200);
  EEPROM.put(0, ed);
  EEPROM.commit();
  EEPROM.end();
}

//  eprom functions  - increment the file_group, so files are always unique
void do_eprom_read()
{

  eprom_data ed;

  EEPROM.begin(200);
  EEPROM.get(0, ed);

  if (ed.eprom_good == MagicNumber)
  {
    Serial.println("Good settings in the EPROM ");
    file_group = ed.file_group;
    file_group++;
    Serial.print("New File Group ");
    Serial.println(file_group);
  }
  else
  {
    Serial.println("No settings in EPROM - Starting with File Group 1 ");
    file_group = 1;
  }
  do_eprom_write();
  file_number = 1;
}

/*********************************************************************************/
/************************    SD Card    *******************************************/
/*********************************************************************************/

void the_sd_loop(void *pvParameter)
{

  Serial.print("the_sd_loop, core ");
  Serial.print(xPortGetCoreID());
  Serial.print(", priority = ");
  Serial.println(uxTaskPriorityGet(NULL));

  while (1)
  {
    xSemaphoreTake(save_current_frame, portMAX_DELAY); // we wait for camera loop to tell us to go
    another_save_avi(fb_curr);                         // do the actual sd wrte
    xSemaphoreGive(take_new_frame);                    // tell camera loop we are done
  }
}

/*********************************************************************************/
/*********************** AVI Conversion  *****************************************/
/*********************************************************************************/

// start_avi - open the files and write in headers
static void start_avi()
{
  Serial.println("Starting an avi ");

  sprintf(avi_file_name, "/%s%d.%03d.avi", devname, file_group, file_number);

  file_number++;

  avifile = SD_MMC.open(avi_file_name, "w");
  idxfile = SD_MMC.open("/idx.tmp", "w");

  if (avifile)
  {
    Serial.printf("File open: %s\n", avi_file_name);
  }
  else
  {
    Serial.println("Could not open file");
  }

  if (idxfile)
  {
    // Serial.printf("File open: %s\n", "//idx.tmp");
  }
  else
  {
    Serial.println("Could not open file /idx.tmp");
  }

  for (i = 0; i < AVIOFFSET; i++)
  {
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }

  memcpy(buf + 0x40, frameSizeData[camera_framesize].frameWidth, 2);
  memcpy(buf + 0xA8, frameSizeData[camera_framesize].frameWidth, 2);
  memcpy(buf + 0x44, frameSizeData[camera_framesize].frameHeight, 2);
  memcpy(buf + 0xAC, frameSizeData[camera_framesize].frameHeight, 2);

  size_t err = avifile.write(buf, AVIOFFSET);

  uint8_t ex_fps = 1;
  if (cam_frm.frame_interval == 0)
  {
    if (camera_framesize >= 11)
    {
      ex_fps = 12.5 * avi_frm.speed_up_factor;
      ;
    }
    else
    {
      ex_fps = 25.0 * avi_frm.speed_up_factor;
    }
  }
  else
  {
    ex_fps = round(1000.0 / cam_frm.frame_interval * avi_frm.speed_up_factor);
  }

  avifile.seek(0x84, SeekSet);
  print_quartet((int)ex_fps, avifile);

  avifile.seek(AVIOFFSET, SeekSet);

  Serial.print(F("\nRecording "));
  Serial.print(avi_frm.avi_length);
  Serial.println(" seconds.");

  startms = millis();

  totalp = 0;
  totalw = 0;

  movi_size = 0;
  uVideoLen = 0;
  idx_offset = 4;

  bad_jpg = 0;
  extend_jpg = 0;
  normal_jpg = 0;

  avifile.flush();
}

static void another_save_avi(camera_fb_t *fb)
{

  long start = millis();

  int fblen = fb->len;
  int fb_block_length;

  unsigned long jpeg_size = fblen;

  uint16_t remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

  long bw = millis();
  long frame_write_start = millis();

  framebuffer_static[0] = 0x30; // "00dc"
  framebuffer_static[1] = 0x30;
  framebuffer_static[2] = 0x64;
  framebuffer_static[3] = 0x63;

  int jpeg_size_rem = jpeg_size + remnant;

  framebuffer_static[4] = jpeg_size_rem % 0x100;
  framebuffer_static[5] = (jpeg_size_rem >> 8) % 0x100;
  framebuffer_static[6] = (jpeg_size_rem >> 16) % 0x100;
  framebuffer_static[7] = (jpeg_size_rem >> 24) % 0x100;

  uint8_t *fb_block_start = fb->buf;

  if (fblen > fbs * 1024 - 8)
  { // fbs is the size of frame buffer static
    fb_block_length = fbs * 1024;
    fblen = fblen - (fbs * 1024 - 8);
    memcpy(framebuffer_static + 8, fb_block_start, fb_block_length - 8);
    fb_block_start = fb_block_start + fb_block_length - 8;
  }
  else
  {
    fb_block_length = fblen + 8 + remnant;
    memcpy(framebuffer_static + 8, fb_block_start, fblen);
    fblen = 0;
  }

  size_t err = avifile.write(framebuffer_static, fb_block_length);

  if (err != fb_block_length)
  {
    Serial.print("Error on avi write: err = ");
    Serial.print(err);
    Serial.print(" len = ");
    Serial.println(fb_block_length);
  }

  while (fblen > 0)
  {

    if (fblen > fbs * 1024)
    {
      fb_block_length = fbs * 1024;
      fblen = fblen - fb_block_length;
    }
    else
    {
      fb_block_length = fblen + remnant;
      fblen = 0;
    }

    memcpy(framebuffer_static, fb_block_start, fb_block_length);

    size_t err = avifile.write(framebuffer_static, fb_block_length);

    if (err != fb_block_length)
    {
      Serial.print("Error on avi write: err = ");
      Serial.print(err);
      Serial.print(" len = ");
      Serial.println(fb_block_length);
    }

    fb_block_start = fb_block_start + fb_block_length;
    delay(0);
  }

  movi_size += jpeg_size;
  uVideoLen += jpeg_size;
  long frame_write_end = millis();

  print_2quartet(idx_offset, jpeg_size, idxfile);

  idx_offset = idx_offset + jpeg_size + remnant + 8;

  movi_size = movi_size + remnant;

  // if (do_it_now == 1 && frame_cnt < 1011)
  // {
  //   do_it_now = 0;
  //   Serial.printf("Frame %6d, len %6d, extra  %4d, cam time %7d,  sd time %4d -- \n", gframe_cnt, gfblen, gj - 1, gmdelay / 1000, millis() - bw);
  // }

  totalw = totalw + millis() - bw;

  avifile.flush();
}

static void end_avi()
{

  long start = millis();

  unsigned long current_end = avifile.position();

  Serial.println("End of avi - closing the files");

  if (frame_cnt < 5)
  {
    Serial.println("Recording screwed up, less than 5 frames, forget index\n");
    idxfile.close();
    avifile.close();
    int xx = remove("/idx.tmp");
    int yy = remove(avi_file_name);
  }
  else
  {

    elapsedms = millis() - startms;

    float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * avi_frm.speed_up_factor;

    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS);
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    // Modify the MJPEG header from the beginning of the file, overwriting various placeholders

    avifile.seek(4, SeekSet);
    print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, avifile);

    avifile.seek(0x20, SeekSet);
    print_quartet(us_per_frame, avifile);

    unsigned long max_bytes_per_sec = (1.0f * movi_size * iAttainedFPS) / frame_cnt;

    avifile.seek(0x24, SeekSet);
    print_quartet(max_bytes_per_sec, avifile);

    avifile.seek(0x30, SeekSet);
    print_quartet(frame_cnt, avifile);

    avifile.seek(0x8c, SeekSet);
    print_quartet(frame_cnt, avifile);

    avifile.seek(0x84, SeekSet);
    print_quartet((int)iAttainedFPS, avifile);

    avifile.seek(0xe8, SeekSet);
    print_quartet(movi_size + frame_cnt * 8 + 4, avifile);

    Serial.println(F("\n*** Video recorded and saved ***\n"));

    Serial.printf("Recorded %5d frames in %5d seconds\n", frame_cnt, elapsedms / 1000);
    Serial.printf("File size is %u bytes\n", movi_size + 12 * frame_cnt + 4);
    Serial.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    Serial.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    Serial.printf("Frame duration is %d us\n", us_per_frame);
    Serial.printf("Average frame length is %d bytes\n", uVideoLen / frame_cnt);
    Serial.print("Average picture time (ms) ");
    Serial.println(1.0 * totalp / frame_cnt);
    Serial.print("Average write time (ms)   ");
    Serial.println(1.0 * totalw / frame_cnt);
    Serial.print("Normal jpg % ");
    Serial.println(100.0 * normal_jpg / frame_cnt, 1);
    Serial.print("Extend jpg % ");
    Serial.println(100.0 * extend_jpg / frame_cnt, 1);
    Serial.print("Bad    jpg % ");
    Serial.println(100.0 * bad_jpg / frame_cnt, 5);

    Serial.printf("Writng the index, %d frames\n", frame_cnt);

    avifile.seek(current_end, SeekSet);
    idxfile.close();

    size_t i1_err = avifile.write(idx1_buf, 4);

    print_quartet(frame_cnt * 16, avifile);

    idxfile = SD_MMC.open("/idx.tmp", "r");

    if (idxfile)
    {
      Serial.printf("File open: %s\n", "//idx.tmp");
    }
    else
    {
      Serial.println("Could not open index file");
    }

    char *AteBytes;
    AteBytes = (char *)malloc(8);

    for (int i = 0; i < frame_cnt; i++)
    {
      size_t res = idxfile.readBytes(AteBytes, 8);
      size_t i1_err = avifile.write(dc_buf, 4);
      size_t i2_err = avifile.write(zero_buf, 4);
      size_t i3_err = avifile.write((uint8_t *)AteBytes, 8);
    }

    free(AteBytes);

    idxfile.close();
    avifile.close();

    int xx = SD_MMC.remove("/idx.tmp");
  }

  Serial.println("");
  // time_total = millis() - startms;
  // Serial.printf("waiting for cam %10dms, %4.1f%%\n", wait_for_cam, 100.0 * wait_for_cam / time_total);
  // Serial.printf("Time in camera  %10dms, %4.1f%%\n", time_in_camera, 100.0 * time_in_camera / time_total);
  // Serial.printf("waiting for sd  %10dms, %4.1f%%\n", delay_wait_for_sd, 100.0 * delay_wait_for_sd / time_total);
  // Serial.printf("Time in sd      %10dms, %4.1f%%\n", time_in_sd, 100.0 * time_in_sd / time_total);
  // Serial.printf("web (core 1)    %10dms, %4.1f%%\n", time_in_web1, 100.0 * time_in_web1 / time_total);
  // Serial.printf("web (core 0)    %10dms, %4.1f%%\n", time_in_web2, 100.0 * time_in_web2 / time_total);
  // Serial.printf("time total      %10dms, %4.1f%%\n", time_total, 100.0 * time_total / time_total);
}

// Writes an uint32_t in Big Endian at current file position
static void inline print_quartet(unsigned long i, File fd)
{
  uint8_t y[4];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  size_t i1_err = fd.write(y, 4);
}

// Writes 2 uint32_t in Big Endian at current file position
static void inline print_2quartet(unsigned long i, unsigned long j, File fd)
{
  uint8_t y[8];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  y[4] = j % 0x100;
  y[5] = (j >> 8) % 0x100;
  y[6] = (j >> 16) % 0x100;
  y[7] = (j >> 24) % 0x100;
  size_t i1_err = fd.write(y, 8);
}

/*********************************************************************************/
/*********************** Cam Reading and Conversion  *****************************/
/*********************************************************************************/
void the_camera_loop(void *pvParameter)
{
  Serial.print("the camera loop, core ");
  Serial.print(xPortGetCoreID());
  Serial.print(", priority = ");
  Serial.println(uxTaskPriorityGet(NULL));

  frame_cnt = 0;
  cam_frm.on_recording = false;

  delay(1000);

  while (1)
  {
    if (Take_New_Shot)
    {
      if (frame_cnt == 0 && !cam_frm.on_recording)
      {
        /************************** 1st frame of Clip ***********************/

        avi_frm.avi_start_time = millis();
        Serial.printf("\n******** Start the 1st frame of the clip -> at %d *********\n", avi_frm.avi_start_time);
        Serial.printf("Framesize %d, camera_quality %d, length %d seconds\n\n", camera_framesize, camera_quality, avi_frm.avi_length);

        frame_cnt++;

        fb_curr = get_good_jpeg(); // should take zero time

        start_avi();

        fb_next = get_good_jpeg(); // should take nearly zero time due to time spent writing header

        cam_frm.framebuffer_len = fb_next->len;                  // v59.5
        memcpy(cam_frm.framebuffer, fb_next->buf, fb_next->len); // v59.5
        // cam_frm.framebuffer_time = millis();                     // v59.5

        cam_frm.on_recording = true;
        xSemaphoreGive(save_current_frame); // trigger sd write to write first frame
      }
      else if ((frame_cnt > 0 && cam_frm.on_recording) && (millis() > (avi_frm.avi_start_time + avi_frm.avi_length * 1000)))
      {
        /************************** Last frame of Clip ***********************/

        xSemaphoreTake(take_new_frame, portMAX_DELAY);
        esp_camera_fb_return(fb_curr);

        frame_cnt++;
        fb_curr = fb_next;
        fb_next = NULL;

        xSemaphoreGive(save_current_frame);            // save final frame of movie
        xSemaphoreTake(take_new_frame, portMAX_DELAY); // wait for final frame of movie to be written

        esp_camera_fb_return(fb_curr);
        fb_curr = NULL;
        Serial.printf("\n******** End the last frame of the clip -> at %d *********\n", millis());

        end_avi(); // end the movie
        delay(50);

        avi_frm.avi_end_time = millis();

        float fps = 1.0 * frame_cnt / ((avi_frm.avi_end_time - avi_frm.avi_start_time) / 1000);

        Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_frm.avi_end_time, avi_frm.avi_end_time - avi_frm.avi_start_time, fps);

        cam_frm.on_recording = false;
        Take_New_Shot = false;
        frame_cnt = 0;
      }
      else if (frame_cnt > 0 && cam_frm.on_recording)
      {
        /************************** Contineous frame of Clip ***********************/

        cam_frm.current_frame_time = millis();
        if (cam_frm.current_frame_time - cam_frm.last_frame_time < cam_frm.frame_interval)
        {
          delay(cam_frm.frame_interval - (cam_frm.current_frame_time - cam_frm.last_frame_time)); // delay for timelapse
        }
        cam_frm.last_frame_time = millis();

        frame_cnt++;

        long delay_wait_for_sd_start = millis();
        xSemaphoreTake(take_new_frame, portMAX_DELAY); // make sure sd writer is done

        esp_camera_fb_return(fb_curr);
        fb_curr = fb_next; // we will write a frame, and get the camera preparing a new one

        xSemaphoreGive(save_current_frame); // write the frame in fb_curr

        fb_next = get_good_jpeg(); // should take near zero, unless the sd is faster than the camera, when we will have to wait for the camera

        cam_frm.framebuffer_len = fb_next->len;                  // v59.5
        memcpy(cam_frm.framebuffer, fb_next->buf, fb_next->len); // v59.5
        // cam_frm.framebuffer_time = millis();                     // v59.5

        if (frame_cnt % 100 == 10)
        {
          // print some status every 100 frames
          if (frame_cnt == 10)
          {
            bytes_before_last_100_frames = movi_size;
            time_before_last_100_frames = millis();
            cam_frm.most_recent_fps = 0;
            cam_frm.most_recent_avg_framesize = 0;
          }
          else
          {
            cam_frm.most_recent_fps = 100.0 / ((millis() - time_before_last_100_frames) / 1000.0);
            cam_frm.most_recent_avg_framesize = (movi_size - bytes_before_last_100_frames) / 100;

            bytes_before_last_100_frames = movi_size;
            time_before_last_100_frames = millis();
          }
        }
      }
    }
    else
    {
      vTaskDelay(2000);
    }
  }
}

//  get_good_jpeg()  - take a picture and make sure it has a good jpeg
camera_fb_t *get_good_jpeg()
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

      totalp = totalp + millis() - bp;
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
        Serial.printf("Bad jpeg, Frame %d, Len = %d \n", frame_cnt, fblen);

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
    camera_quality = qual + 5;
    Serial.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
    delay(1000);
  }
  return fb;
}
