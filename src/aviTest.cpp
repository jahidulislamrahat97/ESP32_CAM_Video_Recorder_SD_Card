#include<aviTest.h>

const int avi_header[AVIOFFSET] PROGMEM = {
  0x52, 0x49, 0x46, 0x46, 0xD8, 0x01, 0x0E, 0x00, 0x41, 0x56, 0x49, 0x20, 0x4C, 0x49, 0x53, 0x54,
  0xD0, 0x00, 0x00, 0x00, 0x68, 0x64, 0x72, 0x6C, 0x61, 0x76, 0x69, 0x68, 0x38, 0x00, 0x00, 0x00,
  0xA0, 0x86, 0x01, 0x00, 0x80, 0x66, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54, 0x84, 0x00, 0x00, 0x00,
  0x73, 0x74, 0x72, 0x6C, 0x73, 0x74, 0x72, 0x68, 0x30, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x73,
  0x4D, 0x4A, 0x50, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x66,
  0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00,
  0x01, 0x00, 0x18, 0x00, 0x4D, 0x4A, 0x50, 0x47, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x46, 0x4F,
  0x10, 0x00, 0x00, 0x00, 0x6A, 0x61, 0x6D, 0x65, 0x73, 0x7A, 0x61, 0x68, 0x61, 0x72, 0x79, 0x20,
  0x76, 0x39, 0x39, 0x20, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
};
uint8_t buf[BUFFSIZE];
uint8_t framebuffer_static[FBS * 1024 + 20];

File avifile;
File idxfile;


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



 void start_avi( Camera_Frame_t &cam_frm, AVI_Frame_t &avi_frm, LOG_AVI_Frame_t &log_frm, int camera_framesize)
{
  Serial.println("Starting an avi ");

  sprintf(avi_frm.avi_file_name, "/test.avi");
//   file_number++;

  avifile = SD_MMC.open(avi_frm.avi_file_name, "w");
  idxfile = SD_MMC.open("/idx.tmp", "w");

  if (avifile)
  {
    Serial.printf("File open: %s\n", avi_frm.avi_file_name);
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

  for (int i = 0; i < AVIOFFSET; i++)
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

  log_frm.total_clip_process_time = millis();
  log_frm.total_pic_cap_time = 0;
  log_frm.total_pic_write_time = 0;
  log_frm.total_frame_len = 0;

  avi_frm.movi_size = 0;
  avi_frm.idx_offset = 4;

  cam_frm.bad_jpg = 0;
  cam_frm.extend_jpg = 0;
  cam_frm.normal_jpg = 0;

  avifile.flush();
}

 void another_save_avi(camera_fb_t *fb, Camera_Frame_t &cam_frm, AVI_Frame_t &avi_frm, LOG_AVI_Frame_t &log_frm)
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

  if (fblen > FBS * 1024 - 8)
  { // fbs is the size of frame buffer static
    fb_block_length = FBS * 1024;
    fblen = fblen - (FBS * 1024 - 8);
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

    if (fblen > FBS * 1024)
    {
      fb_block_length = FBS * 1024;
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

  avi_frm.movi_size += jpeg_size;
  log_frm.total_frame_len += jpeg_size;
  long frame_write_end = millis();

  print_2quartet(avi_frm.idx_offset, jpeg_size, idxfile);

  avi_frm.idx_offset = avi_frm.idx_offset + jpeg_size + remnant + 8;

  avi_frm.movi_size = avi_frm.movi_size + remnant;

  // if (do_it_now == 1 && cam_frm.frame_count < 1011)
  // {
  //   do_it_now = 0;
  //   Serial.printf("Frame %6d, len %6d, extra  %4d, cam time %7d,  sd time %4d -- \n", gframe_cnt, gfblen, gj - 1, gmdelay / 1000, millis() - bw);
  // }

  log_frm.total_pic_write_time = log_frm.total_pic_write_time + millis() - bw;

  avifile.flush();
}


 void end_avi(Camera_Frame_t &cam_frm, AVI_Frame_t &avi_frm, LOG_AVI_Frame_t &log_frm)
{
  uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
  uint8_t dc_buf[4] = {0x30, 0x30, 0x64, 0x63};   // "00dc"
  uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31}; // "idx1"

  unsigned long current_end = avifile.position();

  Serial.println("End of avi - closing the files");

  if (cam_frm.frame_count < 5)
  {
    Serial.println("Recording screwed up, less than 5 frames, forget index\n");
    idxfile.close();
    avifile.close();
    int xx = remove("/idx.tmp");
    int yy = remove(avi_frm.avi_file_name);
  }
  else
  {

    log_frm.elapsedms = millis() - log_frm.total_clip_process_time;

    float fRealFPS = (1000.0f * (float)cam_frm.frame_count) / ((float)log_frm.elapsedms) * avi_frm.speed_up_factor;

    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS);
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    // Modify the MJPEG header from the beginning of the file, overwriting various placeholders

    avifile.seek(4, SeekSet);
    print_quartet(avi_frm.movi_size + 240 + 16 * cam_frm.frame_count + 8 * cam_frm.frame_count, avifile);

    avifile.seek(0x20, SeekSet);
    print_quartet(us_per_frame, avifile);

    unsigned long max_bytes_per_sec = (1.0f * avi_frm.movi_size * iAttainedFPS) / cam_frm.frame_count;

    avifile.seek(0x24, SeekSet);
    print_quartet(max_bytes_per_sec, avifile);

    avifile.seek(0x30, SeekSet);
    print_quartet(cam_frm.frame_count, avifile);

    avifile.seek(0x8c, SeekSet);
    print_quartet(cam_frm.frame_count, avifile);

    avifile.seek(0x84, SeekSet);
    print_quartet((int)iAttainedFPS, avifile);

    avifile.seek(0xe8, SeekSet);
    print_quartet(avi_frm.movi_size + cam_frm.frame_count * 8 + 4, avifile);

    Serial.println(F("\n*** Video recorded and saved ***\n"));

    Serial.printf("Recorded %5d frames in %5d seconds\n", cam_frm.frame_count, log_frm.elapsedms / 1000);
    Serial.printf("File size is %u bytes\n", avi_frm.movi_size + 12 * cam_frm.frame_count + 4);
    Serial.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    Serial.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    Serial.printf("Frame duration is %d us\n", us_per_frame);
    Serial.printf("Average frame length is %d bytes\n", log_frm.total_frame_len / cam_frm.frame_count);
    Serial.print("Average picture time (ms) ");
    Serial.println(1.0 * log_frm.total_pic_cap_time / cam_frm.frame_count);
    Serial.print("Average write time (ms)   ");
    Serial.println(1.0 * log_frm.total_pic_write_time / cam_frm.frame_count);
    Serial.print("Normal jpg % ");
    Serial.println(100.0 * cam_frm.normal_jpg / cam_frm.frame_count, 1);
    Serial.print("Extend jpg % ");
    Serial.println(100.0 * cam_frm.extend_jpg / cam_frm.frame_count, 1);
    Serial.print("Bad    jpg % ");
    Serial.println(100.0 * cam_frm.bad_jpg / cam_frm.frame_count, 5);

    Serial.printf("Writng the index, %d frames\n", cam_frm.frame_count);

    avifile.seek(current_end, SeekSet);
    idxfile.close();

    size_t i1_err = avifile.write(idx1_buf, 4);

    print_quartet(cam_frm.frame_count * 16, avifile);

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

    for (int i = 0; i < cam_frm.frame_count; i++)
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
  // time_total = millis() - log_frm.total_clip_process_time;
  // Serial.printf("waiting for cam %10dms, %4.1f%%\n", wait_for_cam, 100.0 * wait_for_cam / time_total);
  // Serial.printf("Time in camera  %10dms, %4.1f%%\n", time_in_camera, 100.0 * time_in_camera / time_total);
  // Serial.printf("waiting for sd  %10dms, %4.1f%%\n", delay_wait_for_sd, 100.0 * delay_wait_for_sd / time_total);
  // Serial.printf("Time in sd      %10dms, %4.1f%%\n", time_in_sd, 100.0 * time_in_sd / time_total);
  // Serial.printf("web (core 1)    %10dms, %4.1f%%\n", time_in_web1, 100.0 * time_in_web1 / time_total);
  // Serial.printf("web (core 0)    %10dms, %4.1f%%\n", time_in_web2, 100.0 * time_in_web2 / time_total);
  // Serial.printf("time total      %10dms, %4.1f%%\n", time_total, 100.0 * time_total / time_total);
}