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

#include "ArduControl.h"
#include "ModuleInterface.h"
#include <sstream>
#include <cstdio>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "FixSnprintf.h"

constexpr auto BAUDRATE = "9600";
constexpr auto FIRMWARE_ID = "MM-AC";
constexpr auto SERIAL_TIMEOUT = 500;

constexpr unsigned char ASCII_START = 1;
constexpr unsigned char ASCII_END = 4;
constexpr unsigned char ASCII_ACK = 6;
constexpr unsigned char ASCII_NACK = 21;

constexpr unsigned char HEADER_ANAMOD = 'A';
constexpr unsigned char HEADER_ACQUIRE = 'B';
constexpr unsigned char HEADER_DIGMOD = 'D';
constexpr unsigned char HEADER_EXPOSURE = 'E';
constexpr unsigned char HEADER_FIRMWARE = 'F';
constexpr unsigned char HEADER_GATE = 'G';
constexpr unsigned char HEADER_ENABLE = 'H';
constexpr unsigned char HEADER_AMPLITUDE = 'I';
constexpr unsigned char HEADER_LOOPF = 'L';
constexpr unsigned char HEADER_NFRAMES = 'M';
constexpr unsigned char HEADER_NSTEPS = 'N';
constexpr unsigned char HEADER_MODA = 'O';
constexpr unsigned char HEADER_MODD = 'P';
constexpr unsigned char HEADER_RESET = 'R';
constexpr unsigned char HEADER_TRIGGER = 'S';
constexpr unsigned char HEADER_DELTAT = 'T';
constexpr unsigned char HEADER_VERSION = 'V';
constexpr unsigned char HEADER_WAITBEFORE = 'W';
constexpr unsigned char HEADER_WAITAFTER = 'X';

const char *g_DeviceNameArduControlHub = "ArduControl-Hub";
const char *g_DeviceNameTriggerState = "ArduControl-TriggerSelect";
const char *g_DeviceNameEnableShutter = "ArduControl-Enable";
const char *g_DeviceNameOutputP1 = "ArduControl-OutputP1";
const char *g_DeviceNameOutputP2 = "ArduControl-OutputP2";
const char *g_DeviceNameOutputO1 = "ArduControl-OutputO1";
const char *g_DeviceNameOutputO2 = "ArduControl-OutputO2";

// Global info about the state of the Arduino.  This should be folded into a class
const int g_Min_MMVersion = 1;
const int g_Max_MMVersion = 2;
const char *g_versionProp = "Version";

// static lock
MMThreadLock ControlHub::lock_;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_DeviceNameArduControlHub, MM::HubDevice, "Control Hub");
	RegisterDevice(g_DeviceNameTriggerState, MM::StateDevice, "Trigger Selecter");
	RegisterDevice(g_DeviceNameEnableShutter, MM::ShutterDevice, "Global Shutter");
	RegisterDevice(g_DeviceNameOutputP1, MM::SignalIODevice, "Output channel P1");
	RegisterDevice(g_DeviceNameOutputP2, MM::SignalIODevice, "Output channel P2");
	RegisterDevice(g_DeviceNameOutputO1, MM::SignalIODevice, "Output channel O1");
	RegisterDevice(g_DeviceNameOutputO2, MM::SignalIODevice, "Output channel O2");
}

MODULE_API MM::Device *CreateDevice(const char *deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_DeviceNameArduControlHub) == 0)
	{
		return new ControlHub;
	}
	else if (strcmp(deviceName, g_DeviceNameTriggerState) == 0)
	{
		return new TriggerSelect;
	}
	else if (strcmp(deviceName, g_DeviceNameEnableShutter) == 0)
	{
		return new EnableShutter;
	}
	else if (strcmp(deviceName, g_DeviceNameOutputP1) == 0)
	{
		return new AnalogMod(2, deviceName);
	}
	else if (strcmp(deviceName, g_DeviceNameOutputP2) == 0)
	{
		return new AnalogMod(0, deviceName);
	}
	else if (strcmp(deviceName, g_DeviceNameOutputO1) == 0)
	{
		return new AnalogMod(1, deviceName);
	}
	else if (strcmp(deviceName, g_DeviceNameOutputO2) == 0)
	{
		return new AnalogMod(3, deviceName);
	}

	return 0;
}

MODULE_API void DeleteDevice(MM::Device *pDevice)
{
	delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// ControlHub
///////////////////////////////////////////////////////////////////////////////

ControlHub::ControlHub() : initialized_(false),
						   portAvailable_(false),
						   version_(-1),
						   exposure_(10),
						   frameT_(10),
						   deltaT_(0),
						   waitBefore_(10),
						   waitAfter_(10),
						   NSteps_(0),
						   NFrames_(0),
						   digMod_(0),
						   anaMod_(0),
						   loopFrame_(0),
						   acquire_(0),
						   selectTrigger_(-1)
{

	InitializeDefaultErrorMessages();

	SetErrorText(ERR_INVALID_VALUE, "Invalid property value");
	SetErrorText(ERR_BOARD_NOT_FOUND, "Board not found");
	SetErrorText(ERR_COMMUNICATION, "Communication error");
	SetErrorText(ERR_NO_PORT_SET, "ArduControl Hub device is not connected");
	SetErrorText(ERR_MOD_PROTOCOL, "Invalid encoding protocol");
	SetErrorText(ERR_MOD_LENGTH, "Modulation sequence length must be NSteps * NFrames");
	SetErrorText(ERR_TRIGGER, "Manual acquisition is only allowed with internal trigger selected");

	std::ostringstream errorText1;
	errorText1 << "NSteps * NFrames must be between 1 and " << MAX_MOD_LENGTH;
	errorText1 << ", and Frame T > 0";
	SetErrorText(ERR_NMOD, errorText1.str().c_str());

	std::ostringstream errorText;
	errorText << "The firmware version on the Arduino is not compatible with this adapter.  Please use firmware version ";
	errorText << g_Min_MMVersion << " to " << g_Max_MMVersion;
	SetErrorText(ERR_VERSION_MISMATCH, errorText.str().c_str());

	CPropertyAction *pAct = new CPropertyAction(this, &ControlHub::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

ControlHub::~ControlHub()
{
	Shutdown();
}

void ControlHub::GetName(char *name) const
{
	CDeviceUtils::CopyLimitedString(name, g_DeviceNameArduControlHub);
}

int ControlHub::SendCommand(unsigned char header, const unsigned char *payload, unsigned payload_len)
{
	MMThreadGuard threadLock(lock_);

	if (!portAvailable_)
		return ERR_NO_PORT_SET;

	unsigned msg_len = payload_len + 3;

	unsigned char *msg = new unsigned char[msg_len];

	msg[0] = ASCII_START;
	msg[1] = header;

	for (unsigned i = 0; i < payload_len; i++)
	{
		msg[2 + i] = payload[i];
	}

	msg[msg_len - 1] = ASCII_END;

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
	{
		return ret;
	}
	ret = WriteToComPort(port_.c_str(), (const unsigned char *)msg, msg_len);
	if (ret != DEVICE_OK)
	{
		return ret;
	}

	delete[] msg;

	unsigned char answer[1] = {ASCII_NACK};

	MM::MMTime startTime = GetCurrentMMTime();
	unsigned long bytesRead = 0;
	while ((bytesRead < 1) && ((GetCurrentMMTime() - startTime).getMsec() < SERIAL_TIMEOUT))
	{
		ret = ReadFromComPort(port_.c_str(), answer, 1, bytesRead);
		if (ret != DEVICE_OK)
			return ret;
	}

	if (answer[0] != ASCII_ACK)
	{
		return ERR_COMMUNICATION;
	}

	return DEVICE_OK;
}

int ControlHub::SendCommand(unsigned char header, unsigned long payload)
{
	char msg[9];
	int ret = snprintf(msg, 9, "%08lx", payload);
	assert(ret == 8);
	return SendCommand(header, (const unsigned char *)msg, 8);
}

int ControlHub::SendCommand(unsigned char header, int payload)
{
	char msg[5];
	if (payload < 0)
	{
		payload = 65535;
	}
	int ret = snprintf(msg, 5, "%04x", payload);
	assert(ret == 4);
	return SendCommand(header, (const unsigned char *)msg, 4);
}

int ControlHub::SendCommand(unsigned char header, byte payload)
{
	char msg[3];
	int ret = snprintf(msg, 3, "%02x", payload);
	assert(ret == 2);
	return SendCommand(header, (const unsigned char *)msg, 2);
}

int ControlHub::SendCommand(unsigned char header, bool payload)
{
	const unsigned char msg = payload ? '1' : '0';
	return SendCommand(header, &msg, 1);
}

int ControlHub::SendCommand(unsigned char header)
{
	return SendCommand(header, NULL, 0);
}

int ControlHub::AskAnswer(unsigned char header, std::string &answer)
{

	MMThreadGuard threadLock(lock_);

	if (!portAvailable_)
		return ERR_NO_PORT_SET;

	unsigned char msg[3];

	msg[0] = ASCII_START;
	msg[1] = header;
	msg[2] = ASCII_END;

	int ret = PurgeComPort(port_.c_str());
	if (ret != DEVICE_OK)
	{
		return ret;
	}
	ret = WriteToComPort(port_.c_str(), (const unsigned char *)msg, 3);
	if (ret != DEVICE_OK)
	{
		return ret;
	}

	unsigned char answer_[1] = {ASCII_NACK};
	MM::MMTime startTime = GetCurrentMMTime();
	unsigned long bytesRead = 0;
	while ((bytesRead < 1) && ((GetCurrentMMTime() - startTime).getMsec() < SERIAL_TIMEOUT))
	{
		ret = ReadFromComPort(port_.c_str(), answer_, 1, bytesRead);
		if (ret != DEVICE_OK)
			return ret;
	}

	if (answer_[0] != ASCII_ACK)
	{
		return ERR_COMMUNICATION;
	}

	return GetSerialAnswer(port_.c_str(), "\r\n", answer);
}

int ControlHub::GetControllerVersion(int &version)
{
	std::string answer;
	int ret = AskAnswer(HEADER_FIRMWARE, answer);
	if (ret != DEVICE_OK)
		return ret;

	if (answer != FIRMWARE_ID)
		return ERR_BOARD_NOT_FOUND;

	ret = AskAnswer(HEADER_VERSION, answer);
	if (ret != DEVICE_OK)
		return ret;

	std::istringstream is(answer);
	is >> version;

	return ret;
}

bool ControlHub::SupportsDeviceDetection(void)
{
	return true;
}

MM::DeviceDetectionStatus ControlHub::DetectDevice(void)
{
	if (initialized_)
		return MM::CanCommunicate;

	// all conditions must be satisfied...
	MM::DeviceDetectionStatus result = MM::Misconfigured;
	char answerTO[MM::MaxStrLength];

	try
	{
		std::string portLowerCase = port_;
		for (std::string::iterator its = portLowerCase.begin(); its != portLowerCase.end(); ++its)
		{
			*its = (char)tolower(*its);
		}
		if (0 < portLowerCase.length() && 0 != portLowerCase.compare("undefined") && 0 != portLowerCase.compare("unknown"))
		{
			result = MM::CanNotCommunicate;
			// record the default answer time out
			GetCoreCallback()->GetDeviceProperty(port_.c_str(), "AnswerTimeout", answerTO);

			// device specific default communication parameters
			// for Arduino Duemilanova
			GetCoreCallback()->SetDeviceProperty(port_.c_str(), MM::g_Keyword_Handshaking, "Off");
			GetCoreCallback()->SetDeviceProperty(port_.c_str(), MM::g_Keyword_BaudRate, BAUDRATE);
			GetCoreCallback()->SetDeviceProperty(port_.c_str(), MM::g_Keyword_StopBits, "1");
			// Arduino timed out in GetControllerVersion even if AnswerTimeout  = 300 ms
			GetCoreCallback()->SetDeviceProperty(port_.c_str(), "AnswerTimeout", "500.0");
			GetCoreCallback()->SetDeviceProperty(port_.c_str(), "DelayBetweenCharsMs", "0");
			MM::Device *pS = GetCoreCallback()->GetDevice(this, port_.c_str());
			pS->Initialize();

			// The first second or so after opening the serial port, the Arduino is waiting for firmwareupgrades.  Simply sleep 2 seconds.
			CDeviceUtils::SleepMs(2000);

			int v = 0;
			int ret = GetControllerVersion(v);
			// later, Initialize will explicitly check the version #
			if (DEVICE_OK != ret)
			{
				LogMessageCode(ret, true);
			}
			else
			{
				// to succeed must reach here....
				result = MM::CanCommunicate;
			}
			pS->Shutdown();
			// always restore the AnswerTimeout to the default
			GetCoreCallback()->SetDeviceProperty(port_.c_str(), "AnswerTimeout", answerTO);
		}
	}
	catch (...)
	{
		LogMessage("Exception in DetectDevice!", false);
	}

	return result;
}

int ControlHub::Initialize()
{
	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameArduControlHub, MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	// The first second or so after opening the serial port, the Arduino is waiting for firmwareupgrades.  Simply sleep 1 second.
	CDeviceUtils::SleepMs(2000);

	MMThreadGuard myLock(lock_);

	// Check that we have a controller:
	ret = GetControllerVersion(version_);
	if (DEVICE_OK != ret)
		return ret;

	if (version_ < g_Min_MMVersion || version_ > g_Max_MMVersion)
		return ERR_VERSION_MISMATCH;

	SendCommand(HEADER_RESET);

	std::ostringstream sversion;
	sversion << version_;
	CreateProperty(g_versionProp, sversion.str().c_str(), MM::Integer, true);

	ret = UpdateStatus();
	if (ret != DEVICE_OK)
		return ret;

	// turn off verbose serial debug messages
	// GetCoreCallback()->SetDeviceProperty(port_.c_str(), "Verbose", "0");

	CPropertyAction *pAct = new CPropertyAction(this, &ControlHub::OnExposure);
	int nRet = CreateProperty("Exposure", "10.0", MM::Float, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &ControlHub::OnFrameT);
	nRet = CreateProperty("FramePeriod", "10.0", MM::Float, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &ControlHub::OnDeltaT);
	nRet = CreateProperty("StepTime", "0.0", MM::Float, true, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &ControlHub::OnWaitBefore);
	nRet = CreateProperty("WaitBefore", "10.0", MM::Float, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &ControlHub::OnWaitAfter);
	nRet = CreateProperty("WaitAfter", "10.0", MM::Float, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &ControlHub::OnNSteps);
	nRet = CreateProperty("NSteps", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	SetPropertyLimits("NSteps", 0, 255);

	pAct = new CPropertyAction(this, &ControlHub::OnNFrames);
	nRet = CreateProperty("NFrames", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	SetPropertyLimits("NFrames", 0, 255);

	pAct = new CPropertyAction(this, &ControlHub::OnDigMod);
	nRet = CreateProperty("DigitalModulation", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	std::vector<std::string> binary_values;
	binary_values.push_back("0");
	binary_values.push_back("1");
	SetAllowedValues("DigitalModulation", binary_values);

	pAct = new CPropertyAction(this, &ControlHub::OnAnaMod);
	nRet = CreateProperty("AnalogModulation", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	SetAllowedValues("AnalogModulation", binary_values);

	pAct = new CPropertyAction(this, &ControlHub::OnLoopFrame);
	nRet = CreateProperty("LoopFrame", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	SetAllowedValues("LoopFrame", binary_values);

	pAct = new CPropertyAction(this, &ControlHub::OnAcquire);
	nRet = CreateProperty("AcquireFrames", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	initialized_ = true;

	return DEVICE_OK;
}

int ControlHub::DetectInstalledDevices()
{
	if (MM::CanCommunicate == DetectDevice())
	{
		std::vector<std::string> peripherals;
		peripherals.clear();
		peripherals.push_back(g_DeviceNameTriggerState);
		peripherals.push_back(g_DeviceNameEnableShutter);
		peripherals.push_back(g_DeviceNameOutputP1);
		peripherals.push_back(g_DeviceNameOutputP2);
		peripherals.push_back(g_DeviceNameOutputO1);
		peripherals.push_back(g_DeviceNameOutputO2);

		for (size_t i = 0; i < peripherals.size(); i++)
		{
			MM::Device *pDev = CreateDevice(peripherals[i].c_str());
			if (pDev)
			{
				AddInstalledDevice(pDev);
			}
		}
	}

	return DEVICE_OK;
}

int ControlHub::Shutdown()
{
	if (initialized_)
	{
		SendCommand(HEADER_RESET);
	}
	initialized_ = false;
	return DEVICE_OK;
}

long ControlHub::GetSequenceLength()
{
	return NSteps_ * NFrames_;
}

void ControlHub::RegisterModulator(AnalogMod *modulator)
{
	modulators_.push_back(modulator);
}

void ControlHub::SelectTrigger(long trigger)
{
	selectTrigger_ = trigger;
}

void ControlHub::ResetModulations()
{
	for (AnalogMod *modulator : modulators_)
	{
		modulator->ResetModulation();
	}
}

int ControlHub::SendTimeMs(unsigned char header, double timeMs)
{
	if (timeMs < 0)
	{
		return ERR_INVALID_VALUE;
	}

	unsigned long timeUs = (unsigned long)((timeMs * 1000) + 0.5);
	return SendCommand(header, timeUs);
}

int ControlHub::OnPort(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (pAct == MM::AfterSet)
	{
		pProp->Get(port_);
		portAvailable_ = true;
	}
	return DEVICE_OK;
}

int ControlHub::UpdateDeltaT()
{
	double masterT = exposure_;
	if (loopFrame_ || exposure_ == 0)
	{
		masterT = frameT_;
	}
	double deltaT = 0;

	if (NSteps_ != 0)
	{
		deltaT = masterT / NSteps_;
	}

	int ret = SendTimeMs(HEADER_DELTAT, deltaT);
	if (ret != DEVICE_OK)
	{
		return ret;
	}
	deltaT_ = deltaT;
	return DEVICE_OK;
}

int ControlHub::OnExposure(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set(exposure_);
	}
	else if (pAct == MM::AfterSet)
	{
		double exposureMs;
		pProp->Get(exposureMs);

		int ret = SendTimeMs(HEADER_EXPOSURE, exposureMs);
		if (ret != DEVICE_OK)
		{
			return ret;
		}

		exposure_ = exposureMs;

		return UpdateDeltaT();
	}

	return DEVICE_OK;
}

int ControlHub::OnFrameT(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set(frameT_);
	}
	else if (pAct == MM::AfterSet)
	{
		double frameT;
		pProp->Get(frameT);

		if (frameT <= 0)
		{
			return ERR_INVALID_VALUE;
		}

		int ret = UpdateDeltaT();
		if (ret != DEVICE_OK)
			return ret;

		frameT_ = frameT;
	}
	return DEVICE_OK;
}

int ControlHub::OnDeltaT(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set(deltaT_);
	}
	return DEVICE_OK;
}

int ControlHub::OnWaitBefore(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set(waitBefore_);
	}
	else if (pAct == MM::AfterSet)
	{
		double waitBefore;
		pProp->Get(waitBefore);

		int ret = SendTimeMs(HEADER_WAITBEFORE, waitBefore);
		if (ret != DEVICE_OK)
		{
			return ret;
		}
		waitBefore_ = waitBefore;
	}
	return DEVICE_OK;
}

int ControlHub::OnWaitAfter(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set(waitAfter_);
	}
	else if (pAct == MM::AfterSet)
	{
		double waitAfter;
		pProp->Get(waitAfter);

		int ret = SendTimeMs(HEADER_WAITAFTER, waitAfter);
		if (ret != DEVICE_OK)
		{
			return ret;
		}
		waitAfter_ = waitAfter;
	}
	return DEVICE_OK;
}

int ControlHub::OnNSteps(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set((long)NSteps_);
	}
	else if (pAct == MM::AfterSet)
	{
		long NSteps;
		pProp->Get(NSteps);

		if (NSteps == NSteps_ && NSteps != 0)
		{
			return DEVICE_OK;
		}

		if (NFrames_ * NSteps > MAX_MOD_LENGTH)
		{
			return ERR_NMOD;
		}

		int ret = SendCommand(HEADER_NSTEPS, (byte)NSteps);
		if (ret != DEVICE_OK)
			return ret;

		NSteps_ = NSteps;
		ResetModulations();

		digMod_ = false;
		anaMod_ = false;

		return UpdateDeltaT();
	}
	return DEVICE_OK;
}

int ControlHub::OnNFrames(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set((long)NFrames_);
	}
	else if (pAct == MM::AfterSet)
	{
		long NFrames;
		pProp->Get(NFrames);

		if (NFrames == NFrames_ && NFrames != 0)
		{
			return DEVICE_OK;
		}

		if (NFrames * NSteps_ > MAX_MOD_LENGTH)
		{
			return ERR_NMOD;
		}

		int ret = SendCommand(HEADER_NFRAMES, (byte)NFrames);
		if (ret != DEVICE_OK)
			return ret;

		NFrames_ = NFrames;
		ResetModulations();

		digMod_ = false;
		anaMod_ = false;
	}
	return DEVICE_OK;
}

int ControlHub::OnDigMod(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set((long)digMod_);
	}
	else if (pAct == MM::AfterSet)
	{
		long digMod;
		pProp->Get(digMod);

		if (digMod && (GetSequenceLength() == 0 || deltaT_ == 0))
		{
			return ERR_NMOD;
		}

		int ret = SendCommand(HEADER_DIGMOD, (bool)digMod);
		if (ret != DEVICE_OK)
			return ret;

		digMod_ = digMod;
	}
	return DEVICE_OK;
}

int ControlHub::OnAnaMod(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set((long)anaMod_);
	}
	else if (pAct == MM::AfterSet)
	{
		long anaMod;
		pProp->Get(anaMod);

		if (anaMod && (GetSequenceLength() == 0 || deltaT_ == 0))
		{
			return ERR_NMOD;
		}

		int ret = SendCommand(HEADER_ANAMOD, (bool)anaMod);
		if (ret != DEVICE_OK)
			return ret;

		anaMod_ = anaMod;
	}
	return DEVICE_OK;
}

int ControlHub::OnLoopFrame(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set((long)loopFrame_);
	}
	else if (pAct == MM::AfterSet)
	{
		long loopFrame;
		pProp->Get(loopFrame);

		int ret = SendCommand(HEADER_LOOPF, (bool)loopFrame);
		if (ret != DEVICE_OK)
			return ret;

		loopFrame_ = loopFrame;

		return UpdateDeltaT();
	}
	return DEVICE_OK;
}

int ControlHub::OnAcquire(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		pProp->Set((long)acquire_);
	}
	else if (pAct == MM::AfterSet)
	{
		long acquireFrames;
		pProp->Get(acquireFrames);

		if (acquireFrames && selectTrigger_ != 4)
		{
			return ERR_TRIGGER;
		}

		int ret = SendCommand(HEADER_ACQUIRE, (int)acquireFrames);
		if (ret != DEVICE_OK)
			return ret;

		acquire_ = acquireFrames;
	}
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// TriggerSelect
///////////////////////////////////////////////////////////////////////////////

TriggerSelect::TriggerSelect() : position_(4)
{
	InitializeDefaultErrorMessages();

	// add custom error messages
	SetErrorText(ERR_INVALID_VALUE, "Invalid property value");
	SetErrorText(ERR_BOARD_NOT_FOUND, "Board not found");
	SetErrorText(ERR_COMMUNICATION, "Communication error");
	SetErrorText(ERR_NO_PORT_SET, "ArduControl Hub device is not connected");
	SetErrorText(ERR_MOD_PROTOCOL, "Invalid property value");
	SetErrorText(ERR_MOD_LENGTH, "Modulation sequence length must be NSteps * NFrames");

	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameTriggerState, MM::String, true);
	assert(DEVICE_OK == ret);
}

TriggerSelect::~TriggerSelect()
{
	Shutdown();
}

void TriggerSelect::GetName(char *name) const
{
	CDeviceUtils::CopyLimitedString(name, g_DeviceNameTriggerState);
}

int TriggerSelect::Initialize()
{
	ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
	if (!hub || !hub->IsPortAvailable())
	{
		return ERR_NO_PORT_SET;
	}
	char hubLabel[MM::MaxStrLength];
	hub->GetLabel(hubLabel);
	SetParentID(hubLabel); // for backward comp.

	// set property list
	// -----------------

	// create positions and labels
	for (unsigned long i = 0; i < numPos_; i++)
	{
		SetPositionLabel(i, posLabels_[i]);
	}

	// State
	// -----
	CPropertyAction *pAct = new CPropertyAction(this, &TriggerSelect::OnState);
	int nRet = CreateProperty(MM::g_Keyword_State, "4", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	SetPropertyLimits(MM::g_Keyword_State, 0, numPos_ - 1);
	hub->SelectTrigger(4);

	// Label
	// -----
	pAct = new CPropertyAction(this, &CStateBase::OnLabel);
	nRet = CreateProperty(MM::g_Keyword_Label, "Int", MM::String, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	nRet = UpdateStatus();
	if (nRet != DEVICE_OK)
		return nRet;

	// parent ID display
	CreateHubIDProperty();

	return DEVICE_OK;
}

int TriggerSelect::Shutdown()
{
	return DEVICE_OK;
}

int TriggerSelect::OnState(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(position_);
	}
	else if (eAct == MM::AfterSet)
	{
		ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
		if (!hub || !hub->IsPortAvailable())
			return ERR_NO_PORT_SET;

		long pos;
		pProp->Get(pos);

		int ret = hub->SendCommand(HEADER_TRIGGER, (byte)pos);
		if (ret != DEVICE_OK)
		{
			return ret;
		}
		position_ = pos;
		hub->SelectTrigger(pos);
	}

	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// EnableShutter
///////////////////////////////////////////////////////////////////////////////

EnableShutter::EnableShutter() : enabled_(false)
{
	InitializeDefaultErrorMessages();

	// add custom error messages
	SetErrorText(ERR_INVALID_VALUE, "Invalid property value");
	SetErrorText(ERR_BOARD_NOT_FOUND, "Board not found");
	SetErrorText(ERR_COMMUNICATION, "Communication error");
	SetErrorText(ERR_NO_PORT_SET, "ArduControl Hub device is not connected");
	SetErrorText(ERR_MOD_PROTOCOL, "Invalid property value");
	SetErrorText(ERR_MOD_LENGTH, "Modulation sequence length must be NSteps * NFrames");

	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameEnableShutter, MM::String, true);
	assert(DEVICE_OK == ret);
}

EnableShutter::~EnableShutter()
{
	Shutdown();
}

void EnableShutter::GetName(char *name) const
{
	CDeviceUtils::CopyLimitedString(name, g_DeviceNameEnableShutter);
}

int EnableShutter::Initialize()
{
	ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
	if (!hub || !hub->IsPortAvailable())
	{
		return ERR_NO_PORT_SET;
	}
	char hubLabel[MM::MaxStrLength];
	hub->GetLabel(hubLabel);
	SetParentID(hubLabel); // for backward comp.

	// set property list
	// -----------------

	// OnOff
	// ------
	CPropertyAction *pAct = new CPropertyAction(this, &EnableShutter::OnEnable);
	int ret = CreateProperty("Enable", "0", MM::Integer, false, pAct);
	if (ret != DEVICE_OK)
		return ret;

	std::vector<std::string> vals;
	vals.push_back("0");
	vals.push_back("1");
	ret = SetAllowedValues("Enable", vals);
	if (ret != DEVICE_OK)
		return ret;

	ret = UpdateStatus();
	if (ret != DEVICE_OK)
		return ret;

	// parent ID display
	CreateHubIDProperty();

	return DEVICE_OK;
}

int EnableShutter::Shutdown()
{
	return DEVICE_OK;
}

int EnableShutter::SetOpen(bool open)
{
	if (open)
		return SetProperty("Enable", "1");
	else
		return SetProperty("Enable", "0");
}

int EnableShutter::GetOpen(bool &open)
{
	open = enabled_;

	return DEVICE_OK;
}

int EnableShutter::Fire(double /*deltaT*/)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

int EnableShutter::OnEnable(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
	if (eAct == MM::BeforeGet)
	{
		pProp->Set((long)enabled_);
	}
	else if (eAct == MM::AfterSet)
	{
		long pos;
		pProp->Get(pos);

		int ret = hub->SendCommand(HEADER_ENABLE, (bool)pos);
		if (ret != DEVICE_OK)
		{
			return ret;
		}

		enabled_ = pos;
	}

	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// AnalogMod
///////////////////////////////////////////////////////////////////////////////

AnalogMod::AnalogMod(byte channel, const char *deviceName) : channel_(channel),
															 name_(deviceName),
															 gate_(false),
															 amplitude_(0),
															 modulationA_(""),
															 modulationD_("")
{
	InitializeDefaultErrorMessages();

	// add custom error messages
	SetErrorText(ERR_INVALID_VALUE, "Invalid property value");
	SetErrorText(ERR_BOARD_NOT_FOUND, "Board not found");
	SetErrorText(ERR_COMMUNICATION, "Communication error");
	SetErrorText(ERR_NO_PORT_SET, "ArduControl Hub device is not connected");
	SetErrorText(ERR_MOD_PROTOCOL, "Invalid property value");
	SetErrorText(ERR_MOD_LENGTH, "Modulation sequence length must be NSteps * NFrames");

	// Name
	int nRet = CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);
	assert(DEVICE_OK == nRet);
}

AnalogMod::~AnalogMod()
{
	Shutdown();
}

void AnalogMod::GetName(char *name) const
{
	CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

int AnalogMod::Initialize()
{
	ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
	if (!hub || !hub->IsPortAvailable())
	{
		return ERR_NO_PORT_SET;
	}
	char hubLabel[MM::MaxStrLength];
	hub->GetLabel(hubLabel);
	SetParentID(hubLabel);

	CPropertyAction *pAct = new CPropertyAction(this, &AnalogMod::OnAmplitude);
	int nRet = CreateProperty("Amplitude", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	SetPropertyLimits("Amplitude", 0, 255);

	pAct = new CPropertyAction(this, &AnalogMod::OnGate);
	nRet = CreateProperty("Gate", "0", MM::Integer, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;
	std::vector<std::string> vals;
	vals.push_back("0");
	vals.push_back("1");
	nRet = SetAllowedValues("Gate", vals);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &AnalogMod::OnModulationA);
	nRet = CreateProperty("ModulationA", "", MM::String, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	pAct = new CPropertyAction(this, &AnalogMod::OnModulationD);
	nRet = CreateProperty("ModulationD", "", MM::String, false, pAct);
	if (nRet != DEVICE_OK)
		return nRet;

	nRet = UpdateStatus();
	if (nRet != DEVICE_OK)
		return nRet;

	hub->RegisterModulator(this);

	// parent ID display
	CreateHubIDProperty();

	return DEVICE_OK;
}

int AnalogMod::Shutdown()
{
	return DEVICE_OK;
}

int AnalogMod::SetSignal(double volts)
{
	int amplitude = (int)(volts * 255 + 0.5);
	std::string value = std::to_string(amplitude);

	return SetProperty("Amplitude", value.c_str());
}

int AnalogMod::SetGateOpen(bool open)
{
	if (open)
		return SetProperty("Gate", "1");
	else
		return SetProperty("Gate", "0");
}

int AnalogMod::OnAmplitude(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(amplitude_);
	}
	else if (eAct == MM::AfterSet)
	{
		ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
		if (!hub || !hub->IsPortAvailable())
			return ERR_NO_PORT_SET;

		long amplitude;
		pProp->Get(amplitude);

		char payload[4];
		int ret = snprintf(payload, 2, "%d", channel_);
		assert(ret == 1);
		ret = snprintf(payload + 1, 3, "%02x", (byte)amplitude);
		assert(ret == 2);

		ret = hub->SendCommand(HEADER_AMPLITUDE, (const unsigned char *)payload, 3);
		if (ret != DEVICE_OK)
		{
			return ret;
		}

		amplitude_ = amplitude;

		if (modulationA_ != "")
		{
			return SendModulationA(modulationA_);
		}
	}

	return DEVICE_OK;
}

int AnalogMod::OnGate(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set((long)gate_);
	}
	else if (eAct == MM::AfterSet)
	{
		ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
		if (!hub || !hub->IsPortAvailable())
			return ERR_NO_PORT_SET;

		long gate;
		pProp->Get(gate);

		char payload[2];
		int ret = snprintf(payload, 2, "%d", channel_);
		assert(ret == 1);

		payload[1] = gate ? '1' : '0';

		ret = hub->SendCommand(HEADER_GATE, (const unsigned char *)payload, 2);
		if (ret != DEVICE_OK)
		{
			return ret;
		}

		gate_ = gate;
	}

	return DEVICE_OK;
}

int AnalogMod::SendModulationA(std::string modString)
{
	ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
	if (!hub || !hub->IsPortAvailable())
		return ERR_NO_PORT_SET;

	std::vector<int> values;
	std::stringstream ss(modString);
	std::string substr;

	while (getline(ss, substr, '-'))
	{
		int val = std::stoi(substr);
		if (val > 255 || val < 0)
		{
			return ERR_INVALID_VALUE;
		}

		val = (val * amplitude_) / 255;

		values.push_back(val);
	}

	long seqLen = hub->GetSequenceLength();
	if (values.size() != seqLen)
	{
		return ERR_MOD_LENGTH;
	}

	unsigned int payloadLen = 2 * seqLen + 1;
	char *payload = new char[payloadLen + 1];

	int ret = snprintf(payload, 2, "%d", channel_);
	assert(ret == 1);

	for (long i = 0; i < seqLen; i++)
	{
		ret = snprintf(payload + 2 * i + 1, 3, "%02x", values[i]);
		assert(ret == 2);
	}
	ret = hub->SendCommand(HEADER_MODA, (const unsigned char *)payload, payloadLen);

	delete[] payload;

	return ret;
}

int AnalogMod::OnModulationA(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(modulationA_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
		if (!hub || !hub->IsPortAvailable())
			return ERR_NO_PORT_SET;

		std::string modTableA;
		pProp->Get(modTableA);

		int ret = SendModulationA(modTableA);

		if (ret != DEVICE_OK)
		{
			return ret;
		}

		modulationA_ = modTableA;
	}

	return DEVICE_OK;
}

int AnalogMod::OnModulationD(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(modulationD_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		ControlHub *hub = static_cast<ControlHub *>(GetParentHub());
		if (!hub || !hub->IsPortAvailable())
			return ERR_NO_PORT_SET;

		std::string modTableD;
		pProp->Get(modTableD);

		std::vector<bool> values;
		std::stringstream ss(modTableD);
		std::string substr;

		while (getline(ss, substr, '-'))
		{
			try
			{
				int val = std::stoi(substr);
				if (val > 1 || val < 0)
				{
					return ERR_INVALID_VALUE;
				}
				values.push_back(val);
			}
			catch (const std::exception &e)
			{
				return ERR_INVALID_VALUE;
			}
		}

		long seqLen = hub->GetSequenceLength();
		if (values.size() != seqLen)
		{
			return ERR_MOD_LENGTH;
		}

		unsigned int payloadLen = seqLen + 1;
		char *payload = new char[payloadLen + 1];

		int ret = snprintf(payload, 2, "%d", channel_);
		assert(ret == 1);

		for (long i = 0; i < seqLen; i++)
		{
			payload[i + 1] = values[i] ? '1' : '0';
		}
		ret = hub->SendCommand(HEADER_MODD, (const unsigned char *)payload, payloadLen);

		delete[] payload;

		if (ret != DEVICE_OK)
		{
			return ret;
		}

		modulationD_ = modTableD;
	}

	return DEVICE_OK;
}

void AnalogMod::ResetModulation()
{
	modulationA_ = "";
	modulationD_ = "";
}
