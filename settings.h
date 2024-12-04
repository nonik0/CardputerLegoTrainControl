#pragma once

#include <Lpf2Hub.h> // for Color
#include <SD.h>

#include "common.h"

const char *SettingsFilename = "/LegoTrainControl.conf";

// extern in the settings we care about
extern RemoteDevice activeRemoteLeft;
extern RemoteDevice activeRemoteRight;

extern Color lpf2SensorSpdUpColor;
extern Color lpf2SensorStopColor;
extern Color lpf2SensorSpdDnColor;
extern int8_t lpf2SensorSpdUpFunction;
extern int8_t lpf2SensorStopFunction;
extern int8_t lpf2SensorSpdDnFunction;

extern uint8_t irMode;
extern byte irChannel;
extern bool irPortFunction[4][2];

extern byte sbrickLeftPort;
extern byte sbrickRightPort;
extern byte circuitCubesLeftPort;
extern byte circuitCubesRightPort;

enum Settings
{
    ActiveRemoteLeft = 0,
    ActiveRemoteRight = 1,
    Lpf2SensorSpdUpColor = 2,
    Lpf2SensorStopColor = 3,
    Lpf2SensorSpdDnColor = 4,
    Lpf2SensorSpdUpFunction = 5,
    Lpf2SensorStopFunction = 6,
    Lpf2SensorSpdDnFunction = 7,
    IrModeSetting = 8,
    IrChannelSetting = 9,
    IrPortFunction = 10,
    SbrickLeftPort = 11,
    SbrickRightPort = 12,
    CircuitCubesLeftPort = 13,
    CircuitCubesRightPort = 14,
    SettingsCount = 15
};

const char *SettingsNames[] = {
    "activeRemoteLeft",
    "activeRemoteRight",
    "lpf2SensorSpdUpColor",
    "lpf2SensorStopColor",
    "lpf2SensorSpdDnColor",
    "lpf2SensorSpdUpFunction",
    "lpf2SensorStopFunction",
    "lpf2SensorSpdDnFunction",
    "irMode",
    "irChannel",
    "irPortFunction",
    "sbrickLeftPort",
    "sbrickRightPort",
    "circuitCubesLeftPort",
    "circuitCubesRightPort",
};
const char *SettingNamesLower[] = {
    "activeremoteleft",
    "activeremoteright",
    "lpf2sensorspdupcolor",
    "lpf2sensorstopcolor",
    "lpf2sensorspddncolor",
    "lpf2sensorspdupfunction",
    "lpf2sensorstopfunction",
    "lpf2sensorspddnfunction",
    "irmode",
    "irchannel",
    "irportfunction",
    "sbrickleftport",
    "sbrickrightport",
    "circuitcubesleftport",
    "circuitcubesrightport",
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
        else if (name == SettingNamesLower[Lpf2SensorSpdUpColor])
        {
            lpf2SensorSpdUpColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorStopColor])
        {
            lpf2SensorStopColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdDnColor])
        {
            lpf2SensorSpdDnColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdUpFunction])
        {
            lpf2SensorSpdUpFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorStopFunction])
        {
            lpf2SensorStopFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdDnFunction])
        {
            lpf2SensorSpdDnFunction = value.toInt();
        }
        else if (name == SettingNamesLower[IrModeSetting])
        {
            irMode = value.toInt();
        }
        else if (name == SettingNamesLower[IrChannelSetting])
        {
            irChannel = value.toInt();
        }
        else if (name == SettingNamesLower[IrPortFunction])
        {
            for (int i = 0; i < 8; i++)
            {
                irPortFunction[i / 2][i % 2] = value[i] == '1';
            }
        }
        else if (name == SettingNamesLower[SbrickLeftPort])
        {
            sbrickLeftPort = value.toInt();
        }
        else if (name == SettingNamesLower[SbrickRightPort])
        {
            sbrickRightPort = value.toInt();
        }
        else if (name == SettingNamesLower[CircuitCubesLeftPort])
        {
            circuitCubesLeftPort = value.toInt();
        }
        else if (name == SettingNamesLower[CircuitCubesRightPort])
        {
            circuitCubesRightPort = value.toInt();
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
        case Lpf2SensorSpdUpColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdUpColor], lpf2SensorSpdUpColor);
            break;
        case Lpf2SensorStopColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorStopColor], lpf2SensorStopColor);
            break;
        case Lpf2SensorSpdDnColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdDnColor], lpf2SensorSpdDnColor);
            break;
        case Lpf2SensorSpdUpFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdUpFunction], lpf2SensorSpdUpFunction);
            break;
        case Lpf2SensorStopFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorStopFunction], lpf2SensorStopFunction);
            break;
        case Lpf2SensorSpdDnFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdDnFunction], lpf2SensorSpdDnFunction);
            break;
        case IrModeSetting:
            configFile.printf("%s=%d\n", SettingsNames[IrModeSetting], irMode);
            break;
        case IrChannelSetting:
            configFile.printf("%s=%d\n", SettingsNames[IrChannelSetting], irChannel);
            break;
        case IrPortFunction:
            configFile.printf("%s=", SettingsNames[IrPortFunction]);
            for (int i = 0; i < 8; i++)
            {
                configFile.print(irPortFunction[i / 2][i % 2] ? '1' : '0');
            }
            configFile.println();
            break;
        case SbrickLeftPort:
            configFile.printf("%s=%d\n", SettingsNames[SbrickLeftPort], sbrickLeftPort);
            break;
        case SbrickRightPort:
            configFile.printf("%s=%d\n", SettingsNames[SbrickRightPort], sbrickRightPort);
            break;
        case CircuitCubesLeftPort:
            configFile.printf("%s=%d\n", SettingsNames[CircuitCubesLeftPort], circuitCubesLeftPort);
            break;
        case CircuitCubesRightPort:
            configFile.printf("%s=%d\n", SettingsNames[CircuitCubesRightPort], circuitCubesRightPort);
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