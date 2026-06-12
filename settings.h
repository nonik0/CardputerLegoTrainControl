#pragma once

#include <Lpf2Hub.h> // for Color
#include <SD.h>

#include "common.h"

const char *SettingsFilename = "/LegoTrainControl.conf";

// extern in the settings we care about
extern RemoteDevice activeRemoteLeft;
extern RemoteDevice activeRemoteRight;

extern Color lpf2SensorSpdUpColor;
extern Color lpf2SensorSpdUpAltColor;
extern Color lpf2SensorStopColor;
extern Color lpf2SensorStopAltColor;
extern Color lpf2SensorSpdDnColor;
extern Color lpf2SensorSpdDnAltColor;
extern int8_t lpf2SensorSpdUpFunction;
extern int8_t lpf2SensorSpdUpAltFunction;
extern int8_t lpf2SensorStopFunction;
extern int8_t lpf2SensorStopAltFunction;
extern int8_t lpf2SensorSpdDnFunction;
extern int8_t lpf2SensorSpdDnAltFunction;

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
    Lpf2SensorSpdUpAltColor = 3,
    Lpf2SensorStopColor = 4,
    Lpf2SensorStopAltColor = 5,
    Lpf2SensorSpdDnColor = 6,
    Lpf2SensorSpdDnAltColor = 7,
    Lpf2SensorSpdUpFunction = 8,
    Lpf2SensorSpdUpAltFunction = 9,
    Lpf2SensorStopFunction = 10,
    Lpf2SensorStopAltFunction = 11,
    Lpf2SensorSpdDnFunction = 12,
    Lpf2SensorSpdDnAltFunction = 13,
    IrModeSetting =14,
    IrChannelSetting = 15,
    IrPortFunction = 16,
    SbrickLeftPort = 17,
    SbrickRightPort = 18,
    CircuitCubesLeftPort = 19,
    CircuitCubesRightPort = 20,
    SettingsCount = 21
};

const char *SettingsNames[] = {
    "activeRemoteLeft",
    "activeRemoteRight",
    "lpf2SensorSpdUpColor",
    "lpf2SensorSpdUpAltColor",
    "lpf2SensorStopColor",
    "lpf2SensorStopAltColor",
    "lpf2SensorSpdDnColor",
    "lpf2SensorSpdDnAltColor",
    "lpf2SensorSpdUpFunction",
    "lpf2SensorSpdUpAltFunction",
    "lpf2SensorStopFunction",
    "lpf2SensorStopAltFunction",
    "lpf2SensorSpdDnFunction",
    "lpf2SensorSpdDnAltFunction",
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
    "lpf2sensorspdupaltcolor",
    "lpf2sensorstopcolor",
    "lpf2sensorstopaltcolor",
    "lpf2sensorspddncolor",
    "lpf2sensorspddnaltcolor",
    "lpf2sensorspdupfunction",
    "lpf2sensorspdupaltfunction",
    "lpf2sensorstopfunction",
    "lpf2sensorstopaltfunction",
    "lpf2sensorspddnfunction",
    "lpf2sensorspddnaltfunction",
    "irmode",
    "irchannel",
    "irportfunction",
    "sbrickleftport",
    "sbrickrightport",
    "circuitcubesleftport",
    "circuitcubesrightport",
};

extern bool sdCardInit();
extern PowerFunctionsIrBroadcast irTrainCtl;
extern void powerFunctionsRecvCallback(PowerFunctionsIrMessage receivedMessage);

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
        else if (name == SettingNamesLower[Lpf2SensorSpdUpAltColor])
        {
            lpf2SensorSpdUpAltColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorStopColor])
        {
            lpf2SensorStopColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorStopAltColor])
        {
            lpf2SensorStopAltColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdDnColor])
        {
            lpf2SensorSpdDnColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdDnAltColor])
        {
            lpf2SensorSpdDnAltColor = (Color)value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdUpFunction])
        {
            lpf2SensorSpdUpFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdUpAltFunction])
        {
            lpf2SensorSpdUpAltFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorStopFunction])
        {
            lpf2SensorStopFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorStopAltFunction])
        {
            lpf2SensorStopAltFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdDnFunction])
        {
            lpf2SensorSpdDnFunction = value.toInt();
        }
        else if (name == SettingNamesLower[Lpf2SensorSpdDnAltFunction])
        {
            lpf2SensorSpdDnAltFunction = value.toInt();
        }
        else if (name == SettingNamesLower[IrModeSetting])
        {
            irMode = value.toInt();
            if (irMode == 2 || irMode == 3)
            {
                irTrainCtl.enableBroadcast();
                irTrainCtl.registerRecvCallback(powerFunctionsRecvCallback);
            }
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
        case Lpf2SensorSpdUpAltColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdUpAltColor], lpf2SensorSpdUpAltColor);
            break;
        case Lpf2SensorStopColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorStopColor], lpf2SensorStopColor);
            break;
        case Lpf2SensorStopAltColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorStopAltColor], lpf2SensorStopAltColor);
            break;
        case Lpf2SensorSpdDnColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdDnColor], lpf2SensorSpdDnColor);
            break;
        case Lpf2SensorSpdDnAltColor:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdDnAltColor], lpf2SensorSpdDnAltColor);
            break;
        case Lpf2SensorSpdUpFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdUpFunction], lpf2SensorSpdUpFunction);
            break;
        case Lpf2SensorSpdUpAltFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdUpAltFunction], lpf2SensorSpdUpAltFunction);
            break;
        case Lpf2SensorStopFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorStopFunction], lpf2SensorStopFunction);
            break;
        case Lpf2SensorStopAltFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorStopAltFunction], lpf2SensorStopAltFunction);
            break;
        case Lpf2SensorSpdDnFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdDnFunction], lpf2SensorSpdDnFunction);
            break;
        case Lpf2SensorSpdDnAltFunction:
            configFile.printf("%s=%d\n", SettingsNames[Lpf2SensorSpdDnAltFunction], lpf2SensorSpdDnAltFunction);
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