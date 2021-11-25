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

#include "ArdIllu.h"
#include "ModuleInterface.h"
#include <sstream>
#include <cstdio>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "FixSnprintf.h"

constexpr auto BAUDRATE = "9600";

const unsigned char RESET_CODE = 0;
const unsigned char FIRMWARE_CODE = 1;
const unsigned char VERSION_CODE = 2;
const unsigned char TRIG_CODE = 3;
const unsigned char MOD_CODE = 4;
const unsigned char EXT_CODE = 5;

const unsigned char ACK = 0;
const unsigned char ERR = 1;
const unsigned char WARN = 2;

const char FIRMWARE_ID[] = "MM-ArdIllu";

const char *g_DeviceNameArduinoDevice = "Arduino-Illuminator";

// Global info about the state of the Arduino.
const int g_Min_MMVersion = 0;
const int g_Max_MMVersion = 2;
const char *g_versionProp = "Version";

// static lock
MMThreadLock ArdIllu::lock_;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_DeviceNameArduinoDevice, MM::ShutterDevice, "Arduino Illuminator Controller");
}

MODULE_API MM::Device *CreateDevice(const char *deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_DeviceNameArduinoDevice) == 0)
	{
		return new ArdIllu;
	}

	return 0;
}

MODULE_API void DeleteDevice(MM::Device *pDevice)
{
	delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// ArdIllu
///////////////////////////////////////////////////////////////////////////////
ArdIllu::ArdIllu() : initialized_(false),
					 shutterState_("OFF"),
					 intensityState_(0.00),
					 external_(false),
					 port_("Undefined"),
					 version_(-1)
{
	portAvailable_ = false;

	InitializeDefaultErrorMessages();

	EnableDelay();

	SetErrorText(ERR_PORT_OPEN_FAILED, "Failed opening Arduino USB device");
	SetErrorText(ERR_BOARD_NOT_FOUND, "Did not find an Arduino board with the correct firmware.  Is the Arduino board connected to this serial port?");
	SetErrorText(ERR_COMMUNICATION, "Communication error: the Arduino sent an incorrect response.");
	SetErrorText(ERR_EXTERNAL, "Warning: the Arduino is set to external control.");
	std::ostringstream errorText;
	errorText << "The firmware version on the Arduino is not compatible with this adapter.  Please use firmware version ";
	errorText << g_Min_MMVersion << " to " << g_Max_MMVersion;
	SetErrorText(ERR_VERSION_MISMATCH, errorText.str().c_str());

	CPropertyAction *pAct = new CPropertyAction(this, &ArdIllu::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

ArdIllu::~ArdIllu()
{
	Shutdown();
}

void ArdIllu::GetName(char *name) const
{
	CDeviceUtils::CopyLimitedString(name, g_DeviceNameArduinoDevice);
}

bool ArdIllu::Busy()
{
	return false;
}

int ArdIllu::GetControllerVersion(int &version)
{
	int ret = DEVICE_OK;
	unsigned char command[1];
	command[0] = FIRMWARE_CODE;
	version = 0;

	ret = WriteToComPortH((const unsigned char *)command, 1);
	if (ret != DEVICE_OK)
		return ret;

	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), "\r\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	if (answer != FIRMWARE_ID)
		return ERR_BOARD_NOT_FOUND;

	// Check version number of the Arduino
	command[0] = VERSION_CODE;
	ret = WriteToComPortH((const unsigned char *)command, 1);
	if (ret != DEVICE_OK)
		return ret;

	std::string ans;
	ret = GetSerialAnswer(port_.c_str(), "\r\n", ans);
	if (ret != DEVICE_OK)
	{
		return ret;
	}
	std::istringstream is(ans);
	is >> version;

	return ret;
}

bool ArdIllu::SupportsDeviceDetection(void)
{
	return true;
}

MM::DeviceDetectionStatus ArdIllu::DetectDevice(void)
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
			MMThreadGuard myLock(lock_);
			PurgeComPort(port_.c_str());
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

int ArdIllu::Initialize()
{
	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_DeviceNameArduinoDevice, MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	// The first second or so after opening the serial port, the Arduino is waiting for firmwareupgrades.  Simply sleep 1 second.
	CDeviceUtils::SleepMs(2000);

	MMThreadGuard myLock(lock_);

	// Check that we have a controller:
	PurgeComPort(port_.c_str());
	ret = GetControllerVersion(version_);
	if (DEVICE_OK != ret)
		return ret;

	if (version_ < g_Min_MMVersion || version_ > g_Max_MMVersion)
		return ERR_VERSION_MISMATCH;

	std::ostringstream sversion;
	sversion << version_;
	CreateProperty(g_versionProp, sversion.str().c_str(), MM::Integer, true);

	CPropertyAction *pAct = new CPropertyAction(this, &ArdIllu::OnExternal);
	CreateProperty("Control", "?", MM::String, true, pAct);

	pAct = new CPropertyAction(this, &ArdIllu::OnEmission);
	ret = CreateProperty("Emission", "OFF", MM::String, false, pAct);
	if (ret != DEVICE_OK)
		return ret;

	std::vector<std::string> vals;
	vals.push_back("ON");
	vals.push_back("OFF");
	ret = SetAllowedValues("Emission", vals);
	if (ret != DEVICE_OK)
		return ret;

	pAct = new CPropertyAction(this, &ArdIllu::OnIntensity);
	ret = CreateProperty("Intensity", "0", MM::Integer, false, pAct);
	if (DEVICE_OK != ret)
		return ret;

	ret = SetPropertyLimits("Intensity", 0, 255);
	if (ret != DEVICE_OK)
		return ret;

	// reset the controller
	WriteToComPortH(&RESET_CODE, 1);

	ret = UpdateStatus();
	if (ret != DEVICE_OK)
		return ret;

	// turn off verbose serial debug messages
	// GetCoreCallback()->SetDeviceProperty(port_.c_str(), "Verbose", "0");

	changedTime_ = GetCurrentMMTime();
	initialized_ = true;
	return DEVICE_OK;
}

int ArdIllu::Shutdown()
{
	// reset controller (shutdown)
	if (initialized_)
		WriteToComPortH(&RESET_CODE, 1);
	initialized_ = false;
	return DEVICE_OK;
}

int ArdIllu::OnPort(MM::PropertyBase *pProp, MM::ActionType pAct)
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

int ArdIllu::OnExternal(MM::PropertyBase *pProp, MM::ActionType pAct)
{
	if (pAct == MM::BeforeGet)
	{
		if (version_ == 0)
		{
			pProp->Set("Software");
		}

		else
		{
			int ret = DEVICE_OK;
			unsigned char command[1];

			command[0] = VERSION_CODE;
			ret = WriteToComPortH((const unsigned char *)command, 1);

			if (ret != DEVICE_OK)
				return ret;

			std::string ans;
			ret = GetSerialAnswer(port_.c_str(), "\r\n", ans);
			if (ret != DEVICE_OK)
			{
				return ret;
			}

			bool isExternal;
			std::istringstream is(ans);
			is >> isExternal;

			if (isExternal)
			{
				pProp->Set("External");
			}
			else
			{
				pProp->Set("Software");
			}
		}
	}

	return DEVICE_OK;
}

int ArdIllu::SetOpen(bool open)
{
	std::ostringstream os;
	os << "Request " << open;
	LogMessage(os.str().c_str(), true);

	if (open)
		return SetProperty("Emission", "ON");
	else
		return SetProperty("Emission", "OFF");
}

int ArdIllu::GetOpen(bool &open)
{
	char buf[MM::MaxStrLength];
	int ret = GetProperty("Emission", buf);
	if (ret != DEVICE_OK)
		return ret;
	open = (strcmp(buf, "ON") == 0);
	return DEVICE_OK;
}

int ArdIllu::Fire(double /*deltaT*/)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

int ArdIllu::WriteToPort(unsigned char header, long value)
{
	if (!portAvailable_)
		return ERR_BOARD_NOT_FOUND;

	MMThreadGuard myLock(lock_);

	PurgeComPortH();

	unsigned char command[2];

	command[0] = header;
	command[1] = (unsigned char)value;

	int ret = WriteToComPortH((const unsigned char *)command, 2);
	if (ret != DEVICE_OK)
		return ret;

	MM::MMTime startTime = GetCurrentMMTime();
	unsigned long bytesRead = 0;
	unsigned char answer[1] = {ERR};
	while ((bytesRead < 1) && ((GetCurrentMMTime() - startTime).getMsec() < 250))
	{
		ret = ReadFromComPortH(answer, 1, bytesRead);
		if (ret != DEVICE_OK)
			return ret;
	}
	if (answer[0] != ACK)
	{
		if (answer[0] == WARN)
		{
			return ERR_EXTERNAL;
		}
		return ERR_COMMUNICATION;
	}

	return DEVICE_OK;
}

int ArdIllu::OnEmission(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		// use cached state
		pProp->Set(shutterState_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		std::string state;
		pProp->Get(state);
		int ret;
		if (state == "OFF")
			ret = WriteToPort(TRIG_CODE, 0);
		else
			ret = WriteToPort(TRIG_CODE, 1);
		if (ret != DEVICE_OK)
			return ret;
		shutterState_ = state;
		changedTime_ = GetCurrentMMTime();
	}

	return DEVICE_OK;
}

int ArdIllu::OnIntensity(MM::PropertyBase *pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		// use cached state
		pProp->Set(intensityState_);
	}
	else if (eAct == MM::AfterSet)
	{
		double state;
		pProp->Get(state);
		int ret;
		ret = WriteToPort(MOD_CODE, state);
		if (ret != DEVICE_OK)
			return ret;
		intensityState_ = state;
	}

	return DEVICE_OK;
}
