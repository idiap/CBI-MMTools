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

#ifndef _ArdIllu_H_
#define _ArdIllu_H_

#include "MMDevice.h"
#include "DeviceBase.h"
#include <string>
#include <map>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
constexpr auto ERR_UNKNOWN_POSITION = 101;
constexpr auto ERR_INITIALIZE_FAILED = 102;
constexpr auto ERR_WRITE_FAILED = 103;
constexpr auto ERR_CLOSE_FAILED = 104;
constexpr auto ERR_BOARD_NOT_FOUND = 105;
constexpr auto ERR_PORT_OPEN_FAILED = 106;
constexpr auto ERR_COMMUNICATION = 107;
constexpr auto ERR_EXTERNAL = 108;
constexpr auto ERR_VERSION_MISMATCH = 109;

class ArdIllu : public CShutterBase<ArdIllu>
{
public:
	ArdIllu();
	~ArdIllu();

	int Initialize();
	int Shutdown();
	void GetName(char *pszName) const;
	bool Busy();

	// Shutter API
	int SetOpen(bool open = true);
	int GetOpen(bool &open);
	int Fire(double deltaT);

	bool SupportsDeviceDetection(void);
	MM::DeviceDetectionStatus DetectDevice(void);

	// property handlers
	int OnPort(MM::PropertyBase *pPropt, MM::ActionType eAct);
	int OnExternal(MM::PropertyBase *pPropt, MM::ActionType eAct);
	int OnEmission(MM::PropertyBase *pProp, MM::ActionType eAct);
	int OnIntensity(MM::PropertyBase *pProp, MM::ActionType eAct);

private:
	int PurgeComPortH() { return PurgeComPort(port_.c_str()); }
	int WriteToComPortH(const unsigned char *command, unsigned len) { return WriteToComPort(port_.c_str(), command, len); }
	int ReadFromComPortH(unsigned char *answer, unsigned maxLen, unsigned long &bytesRead)
	{
		return ReadFromComPort(port_.c_str(), answer, maxLen, bytesRead);
	}

	int GetControllerVersion(int &);
	int WriteToPort(unsigned char header, long lnValue);

	std::string port_;
	bool initialized_;
	bool portAvailable_;
	bool external_;
	int version_;
	static MMThreadLock lock_;
	std::string shutterState_;
	double intensityState_;
	MM::MMTime changedTime_;
};

#endif //_ArdIllu_H_
