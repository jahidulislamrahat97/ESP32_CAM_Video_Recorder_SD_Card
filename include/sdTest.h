#ifndef __SDTEST_H__
#define __SDTEST_H__

// MicroSD
#include <Arduino.h>
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "FS.h"
#include <SD_MMC.h>

static esp_err_t init_sdcard()
{

    int succ = SD_MMC.begin("/sdcard", true);
    if (succ)
    {
        Serial.printf("SD_MMC Begin: %d\n", succ);
        uint8_t cardType = SD_MMC.cardType();
        Serial.print("SD_MMC Card Type: ");
        if (cardType == CARD_MMC)
        {
            Serial.println("MMC");
        }
        else if (cardType == CARD_SD)
        {
            Serial.println("SDSC");
        }
        else if (cardType == CARD_SDHC)
        {
            Serial.println("SDHC");
        }
        else
        {
            Serial.println("UNKNOWN");
        }

        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
    }
    else
    {
        Serial.printf("Failed to mount SD card VFAT filesystem. \n");
        Serial.println("Do you have an SD Card installed?");
    }
    return ESP_OK;
}

void listDir(const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", "/");

    File root = SD_MMC.open("/");
    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File filex = root.openNextFile();
    while (filex)
    {
        if (filex.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(filex.name());
            if (levels)
            {
                listDir(filex.name(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(filex.name());
            Serial.print("  SIZE: ");
            Serial.println(filex.size());
        }
        filex = root.openNextFile();
    }
}

void deleteFolderOrFile(const char *val)
{
    Serial.printf("Deleting : %s\n", val);
    File f = SD_MMC.open("/" + String(val));
    if (!f)
    {
        Serial.printf("Failed to open %s\n", val);
        return;
    }

    if (f.isDirectory())
    {
        File file = f.openNextFile();
        while (file)
        {
            if (file.isDirectory())
            {
                Serial.print("  DIR : ");
                Serial.println(file.name());
            }
            else
            {
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("  SIZE: ");
                Serial.print(file.size());
                if (SD_MMC.remove(file.name()))
                {
                    Serial.println(" deleted.");
                }
                else
                {
                    Serial.println(" FAILED.");
                }
            }
            file = f.openNextFile();
        }
        f.close();
        // Remove the dir
        if (SD_MMC.rmdir("/" + String(val)))
        {
            Serial.printf("Dir %s removed\n", val);
        }
        else
        {
            Serial.println("Remove dir failed");
        }
    }
    else
    {
        // Remove the file
        if (SD_MMC.remove("/" + String(val)))
        {
            Serial.printf("File %s deleted\n", val);
        }
        else
        {
            Serial.println("Delete failed");
        }
    }
}

void delete_old_stuff()
{
    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

    float full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();
    ;
    if (full < 0.8)
    {
        Serial.printf("Nothing deleted, %.1f%% disk full\n", 100.0 * full);
    }
    else
    {
        Serial.printf("Disk is %.1f%% full ... deleting oldest file\n", 100.0 * full);
        while (full > 0.8)
        {

            double del_number = 999999999;
            char del_numbername[50];

            File f = SD_MMC.open("/");

            File file = f.openNextFile();

            while (file)
            {
                // Serial.println(file.name());
                if (!file.isDirectory())
                {

                    char foldname[50];
                    strcpy(foldname, file.name());
                    for (int x = 0; x < 50; x++)
                    {
                        if ((foldname[x] >= 0x30 && foldname[x] <= 0x39) || foldname[x] == 0x2E)
                        {
                        }
                        else
                        {
                            if (foldname[x] != 0)
                                foldname[x] = 0x20;
                        }
                    }

                    double i = atof(foldname);
                    if (i > 0 && i < del_number)
                    {
                        strcpy(del_numbername, file.name());
                        del_number = i;
                    }
                    // Serial.printf("Name is %s, number is %f\n", foldname, i);
                }
                file = f.openNextFile();
            }
            Serial.printf("lowest is Name is %s, number is %f\n", del_numbername, del_number);
            if (del_number < 999999999)
            {
                deleteFolderOrFile(del_numbername);
            }
            full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();
            Serial.printf("Disk is %.1f%% full ... \n", 100.0 * full);
            f.close();
        }
    }
}

#endif