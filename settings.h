#pragma once

#include <SD.h>

#include "common.h"

const char *SettingsFilename = "/LegoTrainControl.conf";


// extern in the settings we care about
extern RemoteDevice activeRemoteLeft;
extern RemoteDevice activeRemoteRight;

enum Settings
{
    ActiveRemoteLeft = 0,
    ActiveRemoteRight = 1,
    SettingsCount = 2
};

const char *SettingsNames[] = {
    "activeRemoteLeft",
    "activeRemoteRight",
};
const char *SettingNamesLower[] = {
    "activeremoteleft",
    "activeremoteright",
};

extern bool sdCardInit();

File loadConfigFile(const char *mode)
{
    return sdCardInit() ? SD.open(SettingsFilename, mode) : File();
}

void loadSettings()
{
    log_d("Loading settings");

    File configFile = loadConfigFile(FILE_READ);
    if (!configFile)
    {
        log_e("Failed to open settings file for reading");
        return;
    }

    while (configFile.available())
    {
        String line = configFile.readStringUntil('\n');
        String name = line.substring(0, line.indexOf('='));
        String value = line.substring(line.indexOf('=') + 1);

        name.trim();
        name.toLowerCase();
        value.trim();
        value.toLowerCase();

        if (name == SettingNamesLower[ActiveRemoteLeft])
        {
            activeRemoteLeft = (RemoteDevice)value.toInt();
        }
        else if (name == SettingNamesLower[ActiveRemoteRight])
        {
            activeRemoteRight = (RemoteDevice)value.toInt();
        }
        else
        {
            log_e("Unknown setting: %s", name.c_str());
        }
    }

    configFile.close();
    log_i("Loaded settings");
}

void saveSettings()
{
    log_d("Saving settings");

    File configFile = loadConfigFile(FILE_WRITE);
    if (!configFile)
    {
        log_e("Failed to open settings file for writing");
        return;
    }

    for (int i = 0; i < SettingsCount; i++)
    {
        switch (i)
        {
        case ActiveRemoteLeft:
            configFile.printf("%s=%d\n", SettingsNames[ActiveRemoteLeft], activeRemoteLeft);
            break;
        case ActiveRemoteRight:
            configFile.printf("%s=%d\n", SettingsNames[ActiveRemoteRight], activeRemoteRight);
            break;
        default:
            log_e("Unknown setting: %d", i);
            break;
        }
    }

    configFile.flush();
    configFile.close();

    log_i("Saved settings");
}