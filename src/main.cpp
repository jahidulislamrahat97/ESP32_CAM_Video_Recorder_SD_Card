#include <Arduino.h>


#include "cameraTest.h"
#include"aviTest.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "soc/soc.h"
#include "soc/cpu.h"
#include "soc/rtc_cntl_reg.h"


#include <EEPROM.h>


static const char vernum[] = "v60.4.7";
char devname[30];
String devstr = "desklens";

#define Lots_of_Stats 1 //may be debug (-_-)


int avi_length = 1800;    // how long a movie in seconds -- 1800 sec = 30 min
int frame_interval = 0;   // record at full speed
int speed_up_factor = 1;  // play at realtime
int MagicNumber = 12; //EEPORM purpose    // change this number to reset the eprom in your esp32 for file numbers

bool reboot_now = false; // need modification then can delete
bool restart_now = false; // need modification then can delete



TaskHandle_t the_camera_loop_task;
TaskHandle_t the_sd_loop_task;

static SemaphoreHandle_t wait_for_sd;
static SemaphoreHandle_t sd_go;
SemaphoreHandle_t baton; // need edit

long current_frame_time;
long last_frame_time;


// https://github.com/espressif/esp32-camera/issues/182
#define fbs 8  // was 64 -- how many kb of static ram for psram -> sram buffer for sd write
uint8_t framebuffer_static[fbs * 1024 + 20];

typedef struct Camera_Frame_t
{
  uint8_t *framebuffer;
  int framebuffer_len;
  unsigned long framebuffer_time;
} Camera_Frame_t;

typedef struct AVI_Frame_t
{

} AVI_Frame_t;

Camera_Frame_t cam_frm;
AVI_Frame_t avi_frm;

camera_fb_t *fb_curr = NULL;
camera_fb_t *fb_next = NULL;

static esp_err_t cam_err;
float most_recent_fps = 0;
int most_recent_avg_framesize = 0;


long total_frame_data = 0;
long last_frame_length = 0;
long avi_start_time = 0;
long avi_end_time = 0;
int start_record = 0;


long bytes_before_last_100_frames = 0;
long time_before_last_100_frames = 0;

long time_in_loop = 0;
long time_in_camera = 0;
long time_in_sd = 0;
long time_in_good = 0;
long time_total = 0;
long delay_wait_for_sd = 0;
long wait_for_cam = 0;

int do_it_now = 0;
int gframe_cnt;
int gfblen;
int gj;
int gmdelay;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Avi Writer Stuff here


// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "FS.h"
#include <SD_MMC.h>

File logfile;
File avifile;
File idxfile;

char avi_file_name[100];

static int i = 0;
uint16_t frame_cnt = 0;
uint16_t remnant = 0;
uint32_t length = 0;
uint32_t startms;
uint32_t elapsedms;
uint32_t uVideoLen = 0;

int bad_jpg = 0;
int extend_jpg = 0;
int normal_jpg = 0;

int file_number = 0;
int file_group = 0;

long totalp; //may be total picture
long totalw; //may be total wait time

#define BUFFSIZE 512

uint8_t buf[BUFFSIZE];




unsigned long movi_size = 0;
unsigned long jpeg_size = 0;
unsigned long idx_offset = 0;

uint8_t zero_buf[4] = { 0x00, 0x00, 0x00, 0x00 };
uint8_t dc_buf[4] = { 0x30, 0x30, 0x64, 0x63 };  // "00dc"
// uint8_t dc_and_zero_buf[8] = { 0x30, 0x30, 0x64, 0x63, 0x00, 0x00, 0x00, 0x00 };

// uint8_t avi1_buf[4] = { 0x41, 0x56, 0x49, 0x31 };  // "AVI1"
uint8_t idx1_buf[4] = { 0x69, 0x64, 0x78, 0x31 };  // "idx1"





struct eprom_data {
  int eprom_good;
  int file_group;
};

void do_eprom_write() {

  eprom_data ed;
  ed.eprom_good = MagicNumber;
  ed.file_group = file_group;

  Serial.println("Writing to EPROM ...");

  EEPROM.begin(200);
  EEPROM.put(0, ed);
  EEPROM.commit();
  EEPROM.end();
}


void deleteFolderOrFile(const char *val) {
  // Function provided by user @gemi254
  Serial.printf("Deleting : %s\n", val);
  File f = SD_MMC.open("/" + String(val));
  if (!f) {
    Serial.printf("Failed to open %s\n", val);
    return;
  }

  if (f.isDirectory()) {
    File file = f.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        Serial.print("  DIR : ");
        Serial.println(file.name());
      } else {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.print(file.size());
        if (SD_MMC.remove(file.name())) {
          Serial.println(" deleted.");
        } else {
          Serial.println(" FAILED.");
        }
      }
      file = f.openNextFile();
    }
    f.close();
    //Remove the dir
    if (SD_MMC.rmdir("/" + String(val))) {
      Serial.printf("Dir %s removed\n", val);
    } else {
      Serial.println("Remove dir failed");
    }

  } else {
    //Remove the file
    if (SD_MMC.remove("/" + String(val))) {
      Serial.printf("File %s deleted\n", val);
    } else {
      Serial.println("Delete failed");
    }
  }
}


//  data structure from here https://github.com/s60sc/ESP32-CAM_MJPEG2SD/blob/master/avi.cpp, extended for ov5640



//
// Writes an uint32_t in Big Endian at current file position
//
static void inline print_quartet(unsigned long i, File fd) {

  uint8_t y[4];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  size_t i1_err = fd.write(y, 4);
}

//
// Writes 2 uint32_t in Big Endian at current file position
//
static void inline print_2quartet(unsigned long i, unsigned long j, File fd) {

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

//
// if we have no camera, or sd card, then flash rear led on and off to warn the human SOS - SOS
//
// void major_fail() {

//   Serial.println(" ");
//   logfile.close();

//   for (int i = 0; i < 10; i++) {  // 10 loops or about 100 seconds then reboot
//     for (int j = 0; j < 3; j++) {
//       digitalWrite(33, LOW);
//       delay(150);
//       digitalWrite(33, HIGH);
//       delay(150);
//     }
//     delay(1000);

//     for (int j = 0; j < 3; j++) {
//       digitalWrite(33, LOW);
//       delay(500);
//       digitalWrite(33, HIGH);
//       delay(500);
//     }
//     delay(1000);
//     Serial.print("Major Fail  ");
//     Serial.print(i);
//     Serial.print(" / ");
//     Serial.println(10);
//   }

//   ESP.restart();
// }



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//


static esp_err_t init_sdcard() {

  int succ = SD_MMC.begin("/sdcard", true);
  if (succ) {
    Serial.printf("SD_MMC Begin: %d\n", succ);
    uint8_t cardType = SD_MMC.cardType();
    Serial.print("SD_MMC Card Type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  } else {
    Serial.printf("Failed to mount SD card VFAT filesystem. \n");
    Serial.println("Do you have an SD Card installed?");
    Serial.println("Check pin 12 and 13, not grounded, or grounded with 10k resistors!\n\n");
    // major_fail();
  }

  return ESP_OK;
}

// #include "config.h"

// void read_config_file() {

//   // if there is a config.txt, use it plus defaults
//   // else use defaults, and create a config.txt

//   // put a file "config.txt" onto SD card, to set parameters different from your hardcoded parameters
//   // it should look like this - one paramter per line, in the correct order, followed by 2 spaces, and any comments you choose
//   /*
//     ~~~ old config.txt file ~~~
//     desklens  // camera name for files, mdns, etc
//     11  // camera_framesize 9=svga, 10=xga, 11=hd, 12=sxga, 13=uxga, 14=fhd, 17=qxga, 18=qhd, 21=qsxga
//     8  // quality 0-63, lower the better, 10 good start, must be higher than "quality config"
//     11  // camera_framesize config - must be equal or higher than camera_framesize
//     5  / quality config - high q 0..5, med q 6..10, low q 11+
//     3  // buffers - 1 is half speed of 3, but you might run out od memory with 3 and camera_framesize > uxga
//     900  // length of video in seconds
//     0  // interval - ms between frames - 0 for fastest, or 500 for 2fps, 10000 for 10 sec/frame
//     1  // speedup - multiply framerate - 1 for realtime, 24 for record at 1fps, play at 24fps or24x
//     0  // streamdelay - ms between streaming frames - 0 for fast as possible, 500 for 2fps
//     4  // 0 no internet, 1 get time then shutoff, 2 streaming using wifiman, 3 for use ssid names below default off, 4 names below default on
//     MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00  // timezone - this is mountain time, find timezone here https://sites.google.com/a/usapiens.com/opnode/time-zones
//     ssid1234  // ssid
//     mrpeanut  // ssid password

//     ~~~ new config.txt file ~~~
//     desklens  // camera name
//     11  // camera_framesize  11=hd
//     1800  // length of video in seconds
//     0  // interval - ms between recording frames
//     1  // speedup - multiply framerate
//     0  // streamdelay - ms between streaming frames
//     GMT // timezone
//     ssid1234  // ssid wifi name
//     mrpeanut  // ssid password
//     ~~~

//     Lines above are rigid - do not delete lines, must have 2 spaces after the number or string
//   */

//   String junk;

//   String cname = "desklens";
//   int cframesize = 11;
//   int cquality = 12;
//   int cframesizeconfig = 13;
//   int cqualityconfig = 5;
//   int cbuffersconfig = 4;  //58.9
//   int clength = 1800;
//   int cinterval = 0;
//   int cspeedup = 1;
//   int cstreamdelay = 0;
//   int cinternet = 0;
//   String czone = "GMT";



//   Serial.printf("=========   Data fram config.txt and defaults  =========\n");
//   Serial.printf("Name %s\n", cname);
//   logfile.printf("Name %s\n", cname);
//   Serial.printf("Framesize %d\n", cframesize);
//   logfile.printf("Framesize %d\n", cframesize);
//   Serial.printf("Quality %d\n", cquality);
//   logfile.printf("Quality %d\n", cquality);
//   Serial.printf("Framesize config %d\n", cframesizeconfig);
//   logfile.printf("Framesize config%d\n", cframesizeconfig);
//   Serial.printf("Quality config %d\n", cqualityconfig);
//   logfile.printf("Quality config%d\n", cqualityconfig);
//   Serial.printf("Buffers config %d\n", cbuffersconfig);
//   logfile.printf("Buffers config %d\n", cbuffersconfig);
//   Serial.printf("Length %d\n", clength);
//   logfile.printf("Length %d\n", clength);
//   Serial.printf("Interval %d\n", cinterval);
//   logfile.printf("Interval %d\n", cinterval);
//   Serial.printf("Speedup %d\n", cspeedup);
//   logfile.printf("Speedup %d\n", cspeedup);
//   Serial.printf("Streamdelay %d\n", cstreamdelay);
//   logfile.printf("Streamdelay %d\n", cstreamdelay);
//   Serial.printf("Internet %d\n", cinternet);
//   logfile.printf("Internet %d\n", cinternet);
//   Serial.printf("Zone len %d, %s\n", czone.length(), czone.c_str());  //logfile.printf("Zone len %d, %s\n", czone.length(), czone);


//   framesize = cframesize;
//   quality = cquality;
//   framesizeconfig = cframesizeconfig;
//   camera_jpeg_quality = cqualityconfig;
//   buffersconfig = cbuffersconfig;
//   avi_length = clength;
//   frame_interval = cinterval;
//   speed_up_factor = cspeedup;
//   stream_delay = cstreamdelay;
//   configfile = true;
//   TIMEZONE = czone;

//   cname.toCharArray(devname, cname.length() + 1);
// }


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  delete_old_stuff() - delete oldest files to free diskspace
//

void listDir(const char *dirname, uint8_t levels) {

  Serial.printf("Listing directory: %s\n", "/");

  File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File filex = root.openNextFile();
  while (filex) {
    if (filex.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(filex.name());
      if (levels) {
        listDir(filex.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(filex.name());
      Serial.print("  SIZE: ");
      Serial.println(filex.size());
    }
    filex = root.openNextFile();
  }
}

void delete_old_stuff() {

  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

  //listDir( "/", 0);

  float full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();
  ;
  if (full < 0.8) {
    Serial.printf("Nothing deleted, %.1f%% disk full\n", 100.0 * full);
  } else {
    Serial.printf("Disk is %.1f%% full ... deleting oldest file\n", 100.0 * full);
    while (full > 0.8) {

      double del_number = 999999999;
      char del_numbername[50];

      File f = SD_MMC.open("/");

      File file = f.openNextFile();

      while (file) {
        //Serial.println(file.name());
        if (!file.isDirectory()) {

          char foldname[50];
          strcpy(foldname, file.name());
          for (int x = 0; x < 50; x++) {
            if ((foldname[x] >= 0x30 && foldname[x] <= 0x39) || foldname[x] == 0x2E) {
            } else {
              if (foldname[x] != 0) foldname[x] = 0x20;
            }
          }

          double i = atof(foldname);
          if (i > 0 && i < del_number) {
            strcpy(del_numbername, file.name());
            del_number = i;
          }
          //Serial.printf("Name is %s, number is %f\n", foldname, i);
        }
        file = f.openNextFile();
      }
      Serial.printf("lowest is Name is %s, number is %f\n", del_numbername, del_number);
      if (del_number < 999999999) {
        deleteFolderOrFile(del_numbername);
      }
      full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();
      Serial.printf("Disk is %.1f%% full ... \n", 100.0 * full);
      f.close();
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  get_good_jpeg()  - take a picture and make sure it has a good jpeg
//
camera_fb_t *get_good_jpeg() {

  camera_fb_t *fb;

  long start;
  int failures = 0;

  do {
    int fblen = 0;
    int foundffd9 = 0;
    long bp = millis();
    long mstart = micros();

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera Capture Failed");
      failures++;
    } else {
      long mdelay = micros() - mstart;

      int get_fail = 0;

      totalp = totalp + millis() - bp;
      time_in_camera = totalp;

      fblen = fb->len;

      for (int j = 1; j <= 1025; j++) {
        if (fb->buf[fblen - j] != 0xD9) {
          // no d9, try next for
        } else {                                 //Serial.println("Found a D9");
          if (fb->buf[fblen - j - 1] == 0xFF) {  //Serial.print("Found the FFD9, junk is "); Serial.println(j);
            if (j == 1) {
              normal_jpg++;
            } else {
              extend_jpg++;
            }
            foundffd9 = 1;
            if (Lots_of_Stats) {
              if (j > 900) {  //  rarely happens - sometimes on 2640
                Serial.print("Frame ");
                Serial.print(frame_cnt);
                logfile.print("Frame ");
                logfile.print(frame_cnt);
                Serial.print(", Len = ");
                Serial.print(fblen);
                logfile.print(", Len = ");
                logfile.print(fblen);
                //Serial.print(", Correct Len = "); Serial.print(fblen - j + 1);
                Serial.print(", Extra Bytes = ");
                Serial.println(j - 1);
                logfile.print(", Extra Bytes = ");
                logfile.println(j - 1);
                logfile.flush();
              }

              if (frame_cnt % 100 == 50) {
                gframe_cnt = frame_cnt;
                gfblen = fblen;
                gj = j;
                gmdelay = mdelay;
                //Serial.printf("Frame %6d, len %6d, extra  %4d, cam time %7d ", frame_cnt, fblen, j - 1, mdelay / 1000);
                //logfile.printf("Frame %6d, len %6d, extra  %4d, cam time %7d ", frame_cnt, fblen, j - 1, mdelay / 1000);
                do_it_now = 1;
              }
            }
            break;
          }
        }
      }

      if (!foundffd9) {
        bad_jpg++;
        Serial.printf("Bad jpeg, Frame %d, Len = %d \n", frame_cnt, fblen);
        logfile.printf("Bad jpeg, Frame %d, Len = %d\n", frame_cnt, fblen);

        esp_camera_fb_return(fb);
        failures++;

      } else {
        break;
        // count up the useless bytes
      }
    }

  } while (failures < 10);  // normally leave the loop with a break()

  // if we get 10 bad frames in a row, then quality parameters are too high - set them lower (+5), and start new movie
  if (failures == 10) {
    Serial.printf("10 failures");
    logfile.printf("10 failures");
    logfile.flush();

    sensor_t *ss = esp_camera_sensor_get();
    int qual = ss->status.quality;
    ss->set_quality(ss, qual + 5);
    camera_quality = qual + 5;
    Serial.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
    logfile.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
    delay(1000);

    start_record = 0;
    //reboot_now = true;
  }
  return fb;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  eprom functions  - increment the file_group, so files are always unique
//





void do_eprom_read() {

  eprom_data ed;

  EEPROM.begin(200);
  EEPROM.get(0, ed);

  if (ed.eprom_good == MagicNumber) {
    Serial.println("Good settings in the EPROM ");
    file_group = ed.file_group;
    file_group++;
    Serial.print("New File Group ");
    Serial.println(file_group);
  } else {
    Serial.println("No settings in EPROM - Starting with File Group 1 ");
    file_group = 1;
  }
  do_eprom_write();
  file_number = 1;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Make the avi functions
//
//   start_avi() - open the file and write headers
//   another_pic_avi() - write one more frame of movie
//   end_avi() - write the final parameters and close the file


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// start_avi - open the files and write in headers
//

static void start_avi() {

  long start = millis();

  Serial.println("Starting an avi ");

  sprintf(avi_file_name, "/%s%d.%03d.avi", devname, file_group, file_number);

  file_number++;

  avifile = SD_MMC.open(avi_file_name, "w");
  idxfile = SD_MMC.open("/idx.tmp", "w");

  if (avifile) {
    Serial.printf("File open: %s\n", avi_file_name);
    logfile.printf("File open: %s\n", avi_file_name);
  } else {
    Serial.println("Could not open file");
    // major_fail();
  }

  if (idxfile) {
    //Serial.printf("File open: %s\n", "//idx.tmp");
  } else {
    Serial.println("Could not open file /idx.tmp");
    // major_fail();
  }

  for (i = 0; i < AVIOFFSET; i++) {
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }

  memcpy(buf + 0x40, frameSizeData[camera_framesize].frameWidth, 2);
  memcpy(buf + 0xA8, frameSizeData[camera_framesize].frameWidth, 2);
  memcpy(buf + 0x44, frameSizeData[camera_framesize].frameHeight, 2);
  memcpy(buf + 0xAC, frameSizeData[camera_framesize].frameHeight, 2);

  size_t err = avifile.write(buf, AVIOFFSET);

  uint8_t ex_fps = 1;
  if (frame_interval == 0) {
    if (camera_framesize >= 11) {
      ex_fps = 12.5 * speed_up_factor;
      ;
    } else {
      ex_fps = 25.0 * speed_up_factor;
    }
  } else {
    ex_fps = round(1000.0 / frame_interval * speed_up_factor);
  }

  avifile.seek(0x84, SeekSet);
  print_quartet((int)ex_fps, avifile);

  avifile.seek(AVIOFFSET, SeekSet);

  Serial.print(F("\nRecording "));
  Serial.print(avi_length);
  Serial.println(" seconds.");

  startms = millis();

  totalp = 0;
  totalw = 0;

  jpeg_size = 0;
  movi_size = 0;
  uVideoLen = 0;
  idx_offset = 4;

  bad_jpg = 0;
  extend_jpg = 0;
  normal_jpg = 0;

  time_in_loop = 0;
  time_in_camera = 0;
  time_in_sd = 0;
  time_in_good = 0;
  time_total = 0;
  // time_in_web1 = 0;
  // time_in_web2 = 0;
  delay_wait_for_sd = 0;
  wait_for_cam = 0;

  time_in_sd += (millis() - start);

  logfile.flush();
  avifile.flush();

}  // end of start avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  another_save_avi saves another frame to the avi file, uodates index
//           -- pass in a fb pointer to the frame to add
//

static void another_save_avi(camera_fb_t *fb) {

  long start = millis();

  int fblen;
  fblen = fb->len;

  int fb_block_length;
  uint8_t *fb_block_start;

  jpeg_size = fblen;

  remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

  long bw = millis();
  long frame_write_start = millis();

  framebuffer_static[0] = 0x30;  // "00dc"
  framebuffer_static[1] = 0x30;
  framebuffer_static[2] = 0x64;
  framebuffer_static[3] = 0x63;

  int jpeg_size_rem = jpeg_size + remnant;

  framebuffer_static[4] = jpeg_size_rem % 0x100;
  framebuffer_static[5] = (jpeg_size_rem >> 8) % 0x100;
  framebuffer_static[6] = (jpeg_size_rem >> 16) % 0x100;
  framebuffer_static[7] = (jpeg_size_rem >> 24) % 0x100;

  fb_block_start = fb->buf;

  if (fblen > fbs * 1024 - 8) {  // fbs is the size of frame buffer static
    fb_block_length = fbs * 1024;
    fblen = fblen - (fbs * 1024 - 8);
    memcpy(framebuffer_static + 8, fb_block_start, fb_block_length - 8);
    fb_block_start = fb_block_start + fb_block_length - 8;

  } else {
    fb_block_length = fblen + 8 + remnant;
    memcpy(framebuffer_static + 8, fb_block_start, fblen);
    fblen = 0;
  }

  size_t err = avifile.write(framebuffer_static, fb_block_length);

  if (err != fb_block_length) {
    Serial.print("Error on avi write: err = ");
    Serial.print(err);
    Serial.print(" len = ");
    Serial.println(fb_block_length);
    logfile.print("Error on avi write: err = ");
    logfile.print(err);
    logfile.print(" len = ");
    logfile.println(fb_block_length);
  }

  while (fblen > 0) {

    if (fblen > fbs * 1024) {
      fb_block_length = fbs * 1024;
      fblen = fblen - fb_block_length;
    } else {
      fb_block_length = fblen + remnant;
      fblen = 0;
    }

    memcpy(framebuffer_static, fb_block_start, fb_block_length);

    size_t err = avifile.write(framebuffer_static, fb_block_length);

    if (err != fb_block_length) {
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

  if (do_it_now == 1 && frame_cnt < 1011) {
    do_it_now = 0;
    Serial.printf("Frame %6d, len %6d, extra  %4d, cam time %7d,  sd time %4d -- \n", gframe_cnt, gfblen, gj - 1, gmdelay / 1000, millis() - bw);
    logfile.printf("Frame % 6d, len % 6d, extra  % 4d, cam time % 7d,  sd time % 4d -- \n", gframe_cnt, gfblen, gj - 1, gmdelay / 1000, millis() - bw);
    //Serial.printf(" sd time %4d -- \n",  millis() - bw);
    //logfile.printf(" sd time %4d -- \n",  millis() - bw);
    logfile.flush();
  }

  totalw = totalw + millis() - bw;
  time_in_sd += (millis() - start);

  avifile.flush();


}  // end of another_pic_avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  end_avi writes the index, and closes the files
//

static void end_avi() {

  long start = millis();

  unsigned long current_end = avifile.position();

  Serial.println("End of avi - closing the files");
  logfile.println("End of avi - closing the files");

  if (frame_cnt < 5) {
    Serial.println("Recording screwed up, less than 5 frames, forget index\n");
    idxfile.close();
    avifile.close();
    int xx = remove("/idx.tmp");
    int yy = remove(avi_file_name);

  } else {

    elapsedms = millis() - startms;

    float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * speed_up_factor;

    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS);
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    //Modify the MJPEG header from the beginning of the file, overwriting various placeholders

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

    logfile.printf("Recorded %5d frames in %5d seconds\n", frame_cnt, elapsedms / 1000);
    logfile.printf("File size is %u bytes\n", movi_size + 12 * frame_cnt + 4);
    logfile.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    logfile.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    logfile.printf("Frame duration is %d us\n", us_per_frame);
    logfile.printf("Average frame length is %d bytes\n", uVideoLen / frame_cnt);
    logfile.print("Average picture time (ms) ");
    logfile.println(1.0 * totalp / frame_cnt);
    logfile.print("Average write time (ms)   ");
    logfile.println(1.0 * totalw / frame_cnt);
    logfile.print("Normal jpg % ");
    logfile.println(100.0 * normal_jpg / frame_cnt, 1);
    logfile.print("Extend jpg % ");
    logfile.println(100.0 * extend_jpg / frame_cnt, 1);
    logfile.print("Bad    jpg % ");
    logfile.println(100.0 * bad_jpg / frame_cnt, 5);

    logfile.printf("Writng the index, %d frames\n", frame_cnt);

    avifile.seek(current_end, SeekSet);

    idxfile.close();

    size_t i1_err = avifile.write(idx1_buf, 4);

    print_quartet(frame_cnt * 16, avifile);

    idxfile = SD_MMC.open("/idx.tmp", "r");

    if (idxfile) {
      //Serial.printf("File open: %s\n", "//idx.tmp");
      //logfile.printf("File open: %s\n", "/idx.tmp");
    } else {
      Serial.println("Could not open index file");
      logfile.println("Could not open index file");
      // major_fail();
    }

    char *AteBytes;
    AteBytes = (char *)malloc(8);

    for (int i = 0; i < frame_cnt; i++) {
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

  Serial.println("---");
  logfile.println("---");

  time_in_sd += (millis() - start);

  Serial.println("");
  time_total = millis() - startms;
  Serial.printf("waiting for cam %10dms, %4.1f%%\n", wait_for_cam, 100.0 * wait_for_cam / time_total);
  Serial.printf("Time in camera  %10dms, %4.1f%%\n", time_in_camera, 100.0 * time_in_camera / time_total);
  Serial.printf("waiting for sd  %10dms, %4.1f%%\n", delay_wait_for_sd, 100.0 * delay_wait_for_sd / time_total);
  Serial.printf("Time in sd      %10dms, %4.1f%%\n", time_in_sd, 100.0 * time_in_sd / time_total);
  // Serial.printf("web (core 1)    %10dms, %4.1f%%\n", time_in_web1, 100.0 * time_in_web1 / time_total);
  // Serial.printf("web (core 0)    %10dms, %4.1f%%\n", time_in_web2, 100.0 * time_in_web2 / time_total);
  Serial.printf("time total      %10dms, %4.1f%%\n", time_total, 100.0 * time_total / time_total);

  logfile.printf("waiting for cam %10dms, %4.1f%%\n", wait_for_cam, 100.0 * wait_for_cam / time_total);
  logfile.printf("Time in camera  %10dms, %4.1f%%\n", time_in_camera, 100.0 * time_in_camera / time_total);
  logfile.printf("waiting for sd  %10dms, %4.1f%%\n", delay_wait_for_sd, 100.0 * delay_wait_for_sd / time_total);
  logfile.printf("Time in sd      %10dms, %4.1f%%\n", time_in_sd, 100.0 * time_in_sd / time_total);
  // logfile.printf("web (core 1)    %10dms, %4.1f%%\n", time_in_web1, 100.0 * time_in_web1 / time_total);
  // logfile.printf("web (core 0)    %10dms, %4.1f%%\n", time_in_web2, 100.0 * time_in_web2 / time_total);
  logfile.printf("time total      %10dms, %4.1f%%\n", time_total, 100.0 * time_total / time_total);

  logfile.flush();
}


void the_camera_loop(void *pvParameter);
void the_sd_loop(void *pvParameter);
void delete_old_stuff();


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(4, OUTPUT);    // Blinding Disk-Avtive Light
  digitalWrite(4, LOW);  // turn off


  cam_frm.framebuffer_time = 0;

  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.printf("ESP32-CAM-Video-Recorder-junior %s\n", vernum);
  Serial.println("-------------------------------------");

  do_eprom_read();

  // SD camera init
  Serial.println("Mounting the SD card ...");
  esp_err_t card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    // major_fail();
    return;
  }

  Serial.println("Try to get parameters from config.txt ...");


  Serial.println("Setting up the camera ...");
  config_camera();

  Serial.println("Checking SD for available space ...");
  delete_old_stuff();

  cam_frm.framebuffer = (uint8_t *)ps_malloc(512 * 1024);   // buffer to store a jpg in motion // needs to be larger for big frames from ov5640
  // avi_frm.framebuffer2 = (uint8_t *)ps_malloc(512 * 1024);  // buffer to store a jpg in motion // needs to be larger for big frames from ov5640
  // framebuffer3 = (uint8_t *)ps_malloc(512 * 1024);  // buffer to store a jpg in motion // needs to be larger for big frames from ov5640

  Serial.println("Creating the_camera_loop_task");

  wait_for_sd = xSemaphoreCreateBinary();  //xSemaphoreCreateMutex();
  sd_go = xSemaphoreCreateBinary();        //xSemaphoreCreateMutex();
  baton = xSemaphoreCreateMutex();

  // prio 6 - higher than the camera loop(), and the streaming
  xTaskCreatePinnedToCore(the_camera_loop, "the_camera_loop", 3000, NULL, 6, &the_camera_loop_task, 0);  // prio 3, core 0 //v56 core 1 as http dominating 0 ... back to 0, raise prio
  delay(100);
  // prio 4 - higher than the cam_loop(), and the streaming
  xTaskCreatePinnedToCore(the_sd_loop, "the_sd_loop", 2000, NULL, 4, &the_sd_loop_task, 1);  // prio 4, core 1
  delay(200);


  // boot_time = millis();


  Serial.println("  End of setup()\n\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// the_sd_loop()
//

void the_sd_loop(void *pvParameter) {

  Serial.print("the_sd_loop, core ");
  Serial.print(xPortGetCoreID());
  Serial.print(", priority = ");
  Serial.println(uxTaskPriorityGet(NULL));

  while (1) {
    xSemaphoreTake(sd_go, portMAX_DELAY);  // we wait for camera loop to tell us to go
    another_save_avi(fb_curr);             // do the actual sd wrte
    xSemaphoreGive(wait_for_sd);           // tell camera loop we are done
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// the_camera_loop()
int delete_old_stuff_flag = 0;

void the_camera_loop(void *pvParameter) {

  Serial.print("the camera loop, core ");
  Serial.print(xPortGetCoreID());
  Serial.print(", priority = ");
  Serial.println(uxTaskPriorityGet(NULL));

  frame_cnt = 0;
  // start_record_2nd_opinion = digitalRead(12);
  // start_record_1st_opinion = digitalRead(12);
  start_record = 1;

  delay(1000);

  while (1) {

    // if (frame_cnt == 0 && start_record == 0)  // do nothing
    // if (frame_cnt == 0 && start_record == 1)  // start a movie
    // if (frame_cnt > 0 && start_record == 0)   // stop the movie
    // if (frame_cnt > 0 && start_record != 0)   // another frame

    ///////////////////  NOTHING TO DO //////////////////
    // if (frame_cnt == 0 && start_record == 0) {

    //   // Serial.println("Do nothing");
    //   if (we_are_already_stopped == 0) Serial.println("\n\nDisconnect Pin 12 from GND to start recording.\n\n");
    //   we_are_already_stopped = 1;
    //   delay(100);

    //   ///////////////////  START A MOVIE  //////////////////
    // } else
    if (frame_cnt == 0 && start_record == 1) {

      //Serial.println("Ready to start");

      // we_are_already_stopped = 0;

      //delete_old_stuff(); // move to loop

      avi_start_time = millis();
      Serial.printf("\nStart the avi ... at %d\n", avi_start_time);
      Serial.printf("Framesize %d, camera_quality %d, length %d seconds\n\n", camera_framesize, camera_quality, avi_length);
      logfile.printf("\nStart the avi ... at %d\n", avi_start_time);
      logfile.printf("Framesize %d, camera_quality %d, length %d seconds\n\n", camera_framesize, camera_quality, avi_length);
      logfile.flush();

      frame_cnt++;

      long wait_for_cam_start = millis();
      fb_curr = get_good_jpeg();  // should take zero time
      wait_for_cam += millis() - wait_for_cam_start;

      start_avi();

      wait_for_cam_start = millis();
      fb_next = get_good_jpeg();  // should take nearly zero time due to time spent writing header
      //if (cam_frm.framebuffer_time < (millis() - 10)){
      xSemaphoreTake(baton, portMAX_DELAY);
      cam_frm.framebuffer_len = fb_next->len;                   // v59.5
      memcpy(cam_frm.framebuffer, fb_next->buf, fb_next->len);  // v59.5
      cam_frm.framebuffer_time = millis();                      // v59.5
      xSemaphoreGive(baton);
      //}
      wait_for_cam += millis() - wait_for_cam_start;
      xSemaphoreGive(sd_go);  // trigger sd write to write first frame


      ///////////////////  END THE MOVIE //////////////////
    } else if (restart_now || reboot_now || (frame_cnt > 0 && start_record == 0) || millis() > (avi_start_time + avi_length * 1000)) {  // end the avi

      Serial.println("End the Avi");
      restart_now = false;

      xSemaphoreTake(wait_for_sd, portMAX_DELAY);
      esp_camera_fb_return(fb_curr);

      frame_cnt++;
      fb_curr = fb_next;
      fb_next = NULL;

      xSemaphoreGive(sd_go);  // save final frame of movie


      xSemaphoreTake(wait_for_sd, portMAX_DELAY);  // wait for final frame of movie to be written

      esp_camera_fb_return(fb_curr);
      fb_curr = NULL;

      end_avi();  // end the movie


      delete_old_stuff_flag = 1;
      delay(50);

      avi_end_time = millis();

      float fps = 1.0 * frame_cnt / ((avi_end_time - avi_start_time) / 1000);

      Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time, avi_end_time - avi_start_time, fps);
      logfile.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time, avi_end_time - avi_start_time, fps);

      if (!reboot_now) frame_cnt = 0;  // start recording again on the next loop

      ///////////////////  ANOTHER FRAME  //////////////////
    } else if (frame_cnt > 0 && start_record != 0) {  // another frame of the avi

      //Serial.println("Another frame");

      current_frame_time = millis();
      if (current_frame_time - last_frame_time < frame_interval) {
        delay(frame_interval - (current_frame_time - last_frame_time));  // delay for timelapse
      }
      last_frame_time = millis();

      frame_cnt++;

      long delay_wait_for_sd_start = millis();
      xSemaphoreTake(wait_for_sd, portMAX_DELAY);  // make sure sd writer is done
      delay_wait_for_sd += millis() - delay_wait_for_sd_start;

      esp_camera_fb_return(fb_curr);

      fb_curr = fb_next;  // we will write a frame, and get the camera preparing a new one

      xSemaphoreGive(sd_go);  // write the frame in fb_curr

      long wait_for_cam_start = millis();
      fb_next = get_good_jpeg();  // should take near zero, unless the sd is faster than the camera, when we will have to wait for the camera
      //if (cam_frm.framebuffer_time < (millis() - 10)){
      xSemaphoreTake(baton, portMAX_DELAY);
      cam_frm.framebuffer_len = fb_next->len;                   // v59.5
      memcpy(cam_frm.framebuffer, fb_next->buf, fb_next->len);  // v59.5
      cam_frm.framebuffer_time = millis();                      // v59.5
      xSemaphoreGive(baton);
      //}
      wait_for_cam += millis() - wait_for_cam_start;

      
      if (frame_cnt % 100 == 10) {  // print some status every 100 frames
        if (frame_cnt == 10) {
          bytes_before_last_100_frames = movi_size;
          time_before_last_100_frames = millis();
          most_recent_fps = 0;
          most_recent_avg_framesize = 0;
        } else {

          most_recent_fps = 100.0 / ((millis() - time_before_last_100_frames) / 1000.0);
          most_recent_avg_framesize = (movi_size - bytes_before_last_100_frames) / 100;

          if (Lots_of_Stats && frame_cnt < 1011) {
            Serial.printf("So far: %04d frames, in %6.1f seconds, for last 100 frames: avg frame size %6.1f kb, %.2f fps ...\n", frame_cnt, 0.001 * (millis() - avi_start_time), 1.0 / 1024 * most_recent_avg_framesize, most_recent_fps);
            logfile.printf("So far: %04d frames, in %6.1f seconds, for last 100 frames: avg frame size %6.1f kb, %.2f fps ...\n", frame_cnt, 0.001 * (millis() - avi_start_time), 1.0 / 1024 * most_recent_avg_framesize, most_recent_fps);
          }

          // total_delay = 0;

          bytes_before_last_100_frames = movi_size;
          time_before_last_100_frames = millis();
        }
      }
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// loop() - loop runs at low prio, so I had to move it to the task the_camera_loop at higher priority

long wakeup;
long last_wakeup = 0;

void loop() {
}
