#ifndef __SDCARD_H__
#define __SDCARD_H__

#include <Arduino.h>
#include "FS.h"
#include <SD_MMC.h>


bool initSD();
void listDir(const char *dirname, uint8_t levels);
void deleteFolderOrFile(const char *val);
void delete_old_stuff();

#endif