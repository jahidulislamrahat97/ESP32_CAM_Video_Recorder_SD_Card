#include <Arduino.h>

#include "main.h"

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(4, OUTPUT);   // Blinding Disk-Avtive Light
  digitalWrite(4, LOW); // turn off

  cam_frm.current_frame_time = 0;
  cam_frm.last_frame_time = 0;
  cam_frm.frame_interval = 0;
  cam_frm.most_recent_fps = 0.0;
  cam_frm.most_recent_avg_framesize = 0;
  cam_frm.on_recording = false;

  avi_frm.avi_length = MAX_VIDEO_LENGTH_SEC;
  avi_frm.speed_up_factor = 1;
  avi_frm.avi_start_time = 0;
  avi_frm.avi_end_time = 0;

  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.printf("ESP32-CAM-Video-Recorder-junior \n");
  Serial.println("-------------------------------------");

  do_eprom_read();

  // SD Card init
  Serial.println("Mounting the SD card ...");
  if (!initSD())
  {
    Serial.printf("SD Card init failed");
    return;
  }

  // Camera init
  Serial.println("Setting up the camera ...");
  configCamera();

  cam_frm.framebuffer = (uint8_t *)ps_malloc(512 * 1024); // buffer to store a jpg in motion // needs to be larger for big frames from ov5640

  take_new_frame = xSemaphoreCreateBinary();
  save_current_frame = xSemaphoreCreateBinary();

  xTaskCreatePinnedToCore(cameraTask, "camera_task", 3000, NULL, 6, &the_camera_loop_task, 0);
  delay(100);
  xTaskCreatePinnedToCore(aviTask, "avi_task", 2000, NULL, 4, &the_sd_loop_task, 1);
  delay(200);

  Serial.println("End of setup() \n\n");
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
/************************    AVI Task   ******************************************/
/*********************************************************************************/

void aviTask(void *pvParameter)
{

  Serial.print("the_sd_loop, core ");
  Serial.print(xPortGetCoreID());
  Serial.print(", priority = ");
  Serial.println(uxTaskPriorityGet(NULL));

  while (1)
  {
    xSemaphoreTake(save_current_frame, portMAX_DELAY); // we wait for camera loop to tell us to go
    // another_save_avi(cam_frm.current_frame_buffer);                         // do the actual sd wrte
    another_save_avi(cam_frm.current_frame_buffer, cam_frm, avi_frm, log_frm);
    xSemaphoreGive(take_new_frame); // tell camera loop we are done
  }
}

/*********************************************************************************/
/*********************** Camera Task  ********************************************/
/*********************************************************************************/
void cameraTask(void *pvParameter)
{
  Serial.print("the camera loop, core ");
  Serial.print(xPortGetCoreID());
  Serial.print(", priority = ");
  Serial.println(uxTaskPriorityGet(NULL));

  unsigned long bytes_before_last_100_frames = 0;
  unsigned long time_before_last_100_frames = 0;

  cam_frm.frame_count = 0;
  cam_frm.on_recording = false;

  delay(1000);

  while (1)
  {
    if (Take_New_Shot)
    {
      if (cam_frm.frame_count == 0 && !cam_frm.on_recording)
      {
        /************************** 1st frame of Clip ***********************/

        digitalWrite(4, HIGH);

        avi_frm.avi_start_time = millis();
        Serial.printf("\n******** Start the 1st frame of the clip -> at %d *********\n", avi_frm.avi_start_time);
        Serial.printf("Framesize %d, camera_quality %d, length %d seconds\n\n", camera_framesize, camera_quality, avi_frm.avi_length);

        cam_frm.frame_count++;

        cam_frm.current_frame_buffer = capGoodJpeg(cam_frm, &log_frm.total_pic_cap_time, &camera_quality);
        start_avi(cam_frm, avi_frm, log_frm, camera_framesize);

        cam_frm.next_frame_buffer = capGoodJpeg(cam_frm, &log_frm.total_pic_cap_time, &camera_quality);
        cam_frm.framebuffer_len = cam_frm.next_frame_buffer->len;                                    // v59.5
        memcpy(cam_frm.framebuffer, cam_frm.next_frame_buffer->buf, cam_frm.next_frame_buffer->len); // v59.5
        
        cam_frm.on_recording = true;
        xSemaphoreGive(save_current_frame); // trigger sd write to write first frame
      }
      else if ((cam_frm.frame_count > 0 && cam_frm.on_recording) && (millis() > (avi_frm.avi_start_time + avi_frm.avi_length * 1000)))
      {
        /************************** Last frame of Clip ***********************/

        xSemaphoreTake(take_new_frame, portMAX_DELAY);
        esp_camera_fb_return(cam_frm.current_frame_buffer);

        cam_frm.frame_count++;
        cam_frm.current_frame_buffer = cam_frm.next_frame_buffer;
        cam_frm.next_frame_buffer = NULL;

        xSemaphoreGive(save_current_frame);            // save final frame of movie
        xSemaphoreTake(take_new_frame, portMAX_DELAY); // wait for final frame of movie to be written

        esp_camera_fb_return(cam_frm.current_frame_buffer);
        cam_frm.current_frame_buffer = NULL;
        Serial.printf("\n******** End the last frame of the clip -> at %d *********\n", millis());

        // end_avi(); // end the movie
        end_avi(cam_frm, avi_frm, log_frm);
        delay(50);

        avi_frm.avi_end_time = millis();

        float fps = 1.0 * cam_frm.frame_count / ((avi_frm.avi_end_time - avi_frm.avi_start_time) / 1000);

        Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), cam_frm.frame_count, avi_frm.avi_end_time, avi_frm.avi_end_time - avi_frm.avi_start_time, fps);

        cam_frm.on_recording = false;
        Take_New_Shot = false;
        cam_frm.frame_count = 0;

        digitalWrite(4, LOW);
      }
      else if (cam_frm.frame_count > 0 && cam_frm.on_recording)
      {
        /************************** Contineous frame of Clip ***********************/

        cam_frm.current_frame_time = millis();
        if (cam_frm.current_frame_time - cam_frm.last_frame_time < cam_frm.frame_interval)
        {
          delay(cam_frm.frame_interval - (cam_frm.current_frame_time - cam_frm.last_frame_time)); // delay for timelapse
        }
        cam_frm.last_frame_time = millis();
        cam_frm.frame_count++;

        long delay_wait_for_sd_start = millis();
        xSemaphoreTake(take_new_frame, portMAX_DELAY); // make sure sd writer is done

        esp_camera_fb_return(cam_frm.current_frame_buffer);
        cam_frm.current_frame_buffer = cam_frm.next_frame_buffer; // we will write a frame, and get the camera preparing a new one

        xSemaphoreGive(save_current_frame); // write the frame in cam_frm.current_frame_buffer

        cam_frm.next_frame_buffer =capGoodJpeg(cam_frm, &log_frm.total_pic_cap_time, &camera_quality);
        cam_frm.framebuffer_len = cam_frm.next_frame_buffer->len;                                    // v59.5
        memcpy(cam_frm.framebuffer, cam_frm.next_frame_buffer->buf, cam_frm.next_frame_buffer->len); // v59.5

        if (cam_frm.frame_count % 100 == 10)
        {
          // print some status every 100 frames
          if (cam_frm.frame_count == 10)
          {
            bytes_before_last_100_frames = avi_frm.movi_size;
            time_before_last_100_frames = millis();
            cam_frm.most_recent_fps = 0;
            cam_frm.most_recent_avg_framesize = 0;
          }
          else
          {
            cam_frm.most_recent_fps = 100.0 / ((millis() - time_before_last_100_frames) / 1000.0);
            cam_frm.most_recent_avg_framesize = (avi_frm.movi_size - bytes_before_last_100_frames) / 100;

            bytes_before_last_100_frames = avi_frm.movi_size;
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
