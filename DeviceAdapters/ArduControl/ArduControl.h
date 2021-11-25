
/**
* Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/
* Written by François Marelli <francois.marelli@idiap.ch>
* 
* This file is part of CBI-MMTools.
* 
* CBI-MMTools is free software: you can redistribute it and/or modify
* it under the terms of the 3-Clause BSD License.
* 
* CBI-MMTools is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* 3-Clause BSD License for more details.
* 
* You should have received a copy of the 3-Clause BSD License along
* with CBI-MMTools. If not, see https://opensource.org/licenses/BSD-3-Clause.
* 
* SPDX-License-Identifier: BSD-3-Clause 
*/

#ifndef _ArduControl_H_
#define _ArduControl_H_

#include "MMDevice.h"
#include "DeviceBase.h"
#include <string>
#include <map>

typedef uint8_t byte;

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
constexpr auto ERR_INVALID_VALUE = 101;
constexpr auto ERR_BOARD_NOT_FOUND = 102;
constexpr auto ERR_COMMUNICATION = 103;
constexpr auto ERR_NO_PORT_SET = 104;
constexpr auto ERR_VERSION_MISMATCH = 105;
constexpr auto ERR_MOD_PROTOCOL = 106;
constexpr auto ERR_MOD_LENGTH = 107;
constexpr auto ERR_NMOD = 108;
constexpr auto ERR_TRIGGER = 110;

class AnalogMod;

class ControlHub : public HubBase<ControlHub>
{
public:
    ControlHub();
    ~ControlHub();

    int Initialize();
    int Shutdown();
    void GetName(char *pszName) const;
    bool Busy() { return false; }

    bool SupportsDeviceDetection(void);
    MM::DeviceDetectionStatus DetectDevice(void);
    int DetectInstalledDevices();

    // property handlers
    int OnPort(MM::PropertyBase *pPropt, MM::ActionType eAct);

    int OnExposure(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnFrameT(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnDeltaT(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnWaitBefore(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnWaitAfter(MM::PropertyBase *pPropt, MM::ActionType eAct);

    int OnNSteps(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnNFrames(MM::PropertyBase *pPropt, MM::ActionType eAct);

    int OnDigMod(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnAnaMod(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnLoopFrame(MM::PropertyBase *pPropt, MM::ActionType eAct);
    int OnAcquire(MM::PropertyBase *pPropt, MM::ActionType eAct);

    // custom interface for child devices
    bool IsPortAvailable() { return portAvailable_; }

    int SendCommand(unsigned char header, unsigned long payload);
    int SendCommand(unsigned char header, int payload);
    int SendCommand(unsigned char header, byte payload);
    int SendCommand(unsigned char header, bool payload);
    int SendCommand(unsigned char header);

    int SendCommand(unsigned char header, const unsigned char *payload, unsigned payload_len);
    int AskAnswer(unsigned char header, std::string &answer);

    long GetSequenceLength();
    void RegisterModulator(AnalogMod *modulator);
    void SelectTrigger(long trigger);

private:
    int GetControllerVersion(int &);
    int SendTimeMs(unsigned char header, double timeMs);

    int UpdateDeltaT();

    void ResetModulations();
    std::vector<AnalogMod *> modulators_;
    long selectTrigger_;

    const long MAX_MOD_LENGTH = 250;

    bool initialized_;
    bool portAvailable_;
    int version_;
    static MMThreadLock lock_;

    std::string port_;
    double exposure_;
    double frameT_;
    double deltaT_;
    double waitBefore_;
    double waitAfter_;

    byte NSteps_;
    byte NFrames_;
    int acquire_;

    bool digMod_;
    bool anaMod_;
    bool loopFrame_;
};

class EnableShutter : public CShutterBase<EnableShutter>
{
public:
    EnableShutter();
    ~EnableShutter();

    // MMDevice API
    // ------------
    int Initialize();
    int Shutdown();
    void GetName(char *pszName) const;
    bool Busy() { return false; }

    // Shutter API
    int SetOpen(bool open = true);
    int GetOpen(bool &open);
    int Fire(double deltaT);

    // action interface
    // ----------------
    int OnEnable(MM::PropertyBase *pPropt, MM::ActionType eAct);

private:
    bool enabled_;
};

class TriggerSelect : public CStateDeviceBase<TriggerSelect>
{
public:
    TriggerSelect();
    ~TriggerSelect();

    // MMDevice API
    // ------------
    int Initialize();
    int Shutdown();

    void GetName(char *pszName) const;
    bool Busy() { return false; }

    unsigned long GetNumberOfPositions() const { return numPos_; }

    // action interface
    // ----------------
    int OnState(MM::PropertyBase *pProp, MM::ActionType eAct);

private:
    const unsigned long numPos_ = 5;
    const char *posLabels_[5] = {"Aux.", "CamFire1", "CamFireN", "CamFireAll", "Internal"};
    long position_;
};

class AnalogMod : public CSignalIOBase<AnalogMod>
{
public:
    AnalogMod(byte channel, const char *deviceName);
    ~AnalogMod();

    // MMDevice API
    // ------------
    int Initialize();
    int Shutdown();

    void GetName(char *pszName) const;
    bool Busy() { return false; }

    // DA API
    int SetGateOpen(bool open);
    int GetGateOpen(bool &open)
    {
        open = gate_;
        return DEVICE_OK;
    }
    int SetSignal(double volts);
    int GetSignal(double &volts) { return DEVICE_UNSUPPORTED_COMMAND; }
    int GetLimits(double &minVolts, double &maxVolts)
    {
        minVolts = minA_;
        maxVolts = maxA_;
        return DEVICE_OK;
    }

    int IsDASequenceable(bool &isSequenceable) const
    {
        isSequenceable = false;
        return DEVICE_OK;
    }

    void ResetModulation();

    // action interface
    // ----------------
    int OnAmplitude(MM::PropertyBase *pProp, MM::ActionType eAct);
    int OnGate(MM::PropertyBase *pProp, MM::ActionType eAct);
    int OnModulationA(MM::PropertyBase *pProp, MM::ActionType eAct);
    int OnModulationD(MM::PropertyBase *pProp, MM::ActionType eAct);

private:
    int SendModulationA(std::string modString);

    const double minA_ = 0.0;
    const double maxA_ = 1.0;
    const byte maxChannel_ = 4;
    bool gate_;

    byte channel_;
    std::string name_;

    long amplitude_;
    std::string modulationA_;
    std::string modulationD_;
};

#endif //_ArduControl_H_
