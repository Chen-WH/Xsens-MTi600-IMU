
//  Copyright (c) 2003-2022 Xsens Technologies B.V. or subsidiaries worldwide.
//  All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//  
//  1.	Redistributions of source code must retain the above copyright notice,
//  	this list of conditions, and the following disclaimer.
//  
//  2.	Redistributions in binary form must reproduce the above copyright notice,
//  	this list of conditions, and the following disclaimer in the documentation
//  	and/or other materials provided with the distribution.
//  
//  3.	Neither the names of the copyright holders nor the names of their contributors
//  	may be used to endorse or promote products derived from this software without
//  	specific prior written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
//  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
//  THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
//  OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR
//  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.THE LAWS OF THE NETHERLANDS 
//  SHALL BE EXCLUSIVELY APPLICABLE AND ANY DISPUTES SHALL BE FINALLY SETTLED UNDER THE RULES 
//  OF ARBITRATION OF THE INTERNATIONAL CHAMBER OF COMMERCE IN THE HAGUE BY ONE OR MORE 
//  ARBITRATORS APPOINTED IN ACCORDANCE WITH SAID RULES.
//  

//--------------------------------------------------------------------------------
// Public Xsens device API C++ example MTi receive data.
//--------------------------------------------------------------------------------
#include <xscontroller/xscontrol_def.h>
#include <xscontroller/xsdevice_def.h>
#include <xscontroller/xsscanner.h>
#include <xstypes/xsoutputconfigurationarray.h>
#include <xstypes/xsdatapacket.h>
#include <xstypes/xstime.h>
#include <xscommon/xsens_mutex.h>

#include <iostream>
#include <iomanip>
#include <list>
#include <string>
#include <sstream>

Journaller* gJournal = 0;

using namespace std;

class CallbackHandler : public XsCallback
{
public:
	CallbackHandler(size_t maxBufferSize = 5)
		: m_maxNumberOfPacketsInBuffer(maxBufferSize)
		, m_numberOfPacketsInBuffer(0)
	{
	}

	virtual ~CallbackHandler() throw()
	{
	}

	bool packetAvailable() const
	{
		xsens::Lock locky(&m_mutex);
		return m_numberOfPacketsInBuffer > 0;
	}

	XsDataPacket getNextPacket()
	{
		assert(packetAvailable());
		xsens::Lock locky(&m_mutex);
		XsDataPacket oldestPacket(m_packetBuffer.front());
		m_packetBuffer.pop_front();
		--m_numberOfPacketsInBuffer;
		return oldestPacket;
	}

protected:
	void onLiveDataAvailable(XsDevice*, const XsDataPacket* packet) override
	{
		xsens::Lock locky(&m_mutex);
		assert(packet != 0);
		while (m_numberOfPacketsInBuffer >= m_maxNumberOfPacketsInBuffer)
			(void)getNextPacket();

		m_packetBuffer.push_back(*packet);
		++m_numberOfPacketsInBuffer;
		assert(m_numberOfPacketsInBuffer <= m_maxNumberOfPacketsInBuffer);
	}
private:
	mutable xsens::Mutex m_mutex;

	size_t m_maxNumberOfPacketsInBuffer;
	size_t m_numberOfPacketsInBuffer;
	list<XsDataPacket> m_packetBuffer;
};

//--------------------------------------------------------------------------------
int main(void)
{
	cout << "Creating XsControl object..." << endl;
	XsControl* control = XsControl::construct();
	assert(control != 0);

	// Lambda function for error handling
	auto handleError = [=](string errorString)
	{
		control->destruct();
		cout << errorString << endl;
		cout << "Press [ENTER] to continue." << endl;
		cin.get();
		return -1;
	};

	cout << "Scanning for devices..." << endl;
	XsPortInfoArray portInfoArray = XsScanner::scanPorts();

	// Find an MTi device
	XsPortInfo mtPort[8];
	int PortNum = 0;
	for (auto const &portInfo : portInfoArray)
	{
		if (portInfo.deviceId().isMti() || portInfo.deviceId().isMtig())
		{
			mtPort[PortNum] = portInfo;
			++PortNum;
		}
	}

	if (mtPort[0].empty())
		return handleError("No MTi device found. Aborting.");
	for (int i = 0; i < PortNum; ++i)
	{
		cout << "Found a device with ID: " << mtPort[i].deviceId().toString().toStdString() << " @ port: " << mtPort[i].portName().toStdString() << ", baudrate: " << mtPort[i].baudrate() << endl;
	}

	cout << "Opening port..." << endl;
	for (int i = 0; i < PortNum; ++i)
	{
		if (!control->openPort(mtPort[i].portName().toStdString(), mtPort[i].baudrate()))
			return handleError("Could not open port. Aborting.");
	}
	
	// Get the device object
	XsDevice* device[8];
	for (int i = 0; i < PortNum; ++i)
	{
		device[i] = control->device(mtPort[i].deviceId());
		assert(device[i] != 0);

		cout << "Device: " << device[i]->productCode().toStdString() << ", with ID: " << device[i]->deviceId().toString() << " opened." << endl;
	}

	// Create and attach callback handler to device
	CallbackHandler callback[8];
	for (int i = 0; i < PortNum; ++i)
	{
		device[i]->addCallbackHandler(&callback[i]);
	}

	// Put the device into configuration mode before configuring the device
	cout << "Putting device into configuration mode..." << endl;
	for (int i = 0; i < PortNum; ++i)
	{
		if (!device[i]->gotoConfig())
			return handleError("Could not put device into configuration mode. Aborting.");
	}

	cout << "Configuring the device..." << endl;

	// Important for Public XDA!
	XsOutputConfigurationArray configArray;
	configArray.push_back(XsOutputConfiguration(XDI_PacketCounter, 0));
	configArray.push_back(XsOutputConfiguration(XDI_SampleTimeFine, 0));

	if (device[0]->deviceId().isImu())
	{
		configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 400));
		configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 400));
		configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 400));
	}
	else if (device[0]->deviceId().isVru() || device[0]->deviceId().isAhrs())
	{
		configArray.push_back(XsOutputConfiguration(XDI_Quaternion, 400));
	}
	else if (device[0]->deviceId().isGnss())
	{
		configArray.push_back(XsOutputConfiguration(XDI_Quaternion, 400));
		configArray.push_back(XsOutputConfiguration(XDI_LatLon, 400));
		configArray.push_back(XsOutputConfiguration(XDI_AltitudeEllipsoid, 400));
		configArray.push_back(XsOutputConfiguration(XDI_VelocityXYZ, 400));
	}
	else
	{
		return handleError("Unknown device while configuring. Aborting.");
	}

	for (int i = 0; i < PortNum; ++i)
	{
		if (!device[i]->setOutputConfiguration(configArray))
			return handleError("Could not configure MTi device. Aborting.");

		cout << "Putting device into measurement mode..." << endl;
		if (!device[i]->gotoMeasurement())
			return handleError("Could not put device into measurement mode. Aborting.");
	}

	cout << "\nMain loop. Recording data for 10 seconds." << endl;
	cout << string(79, '-') << endl;

	int64_t startTime = XsTime::timeStampNow();

	while (XsTime::timeStampNow() - startTime <= 10000)
	{
		for (int i = 0; i < PortNum; ++i)
		{
			if (callback[i].packetAvailable())
			{
				cout << setw(5) << fixed << setprecision(2);

				// Retrieve a packet
				XsDataPacket packet = callback[i].getNextPacket();
				if (packet.containsCalibratedData())
				{
					XsVector acc = packet.calibratedAcceleration();
					cout << "\r"
						<< "Acc X:" << acc[0]
						<< ", Acc Y:" << acc[1]
						<< ", Acc Z:" << acc[2];

					XsVector gyr = packet.calibratedGyroscopeData();
					cout << " |Gyr X:" << gyr[0]
						<< ", Gyr Y:" << gyr[1]
						<< ", Gyr Z:" << gyr[2];

					XsVector mag = packet.calibratedMagneticField();
					cout << " |Mag X:" << mag[0]
						<< ", Mag Y:" << mag[1]
						<< ", Mag Z:" << mag[2];
				}

				if (packet.containsOrientation())
				{
					XsQuaternion quaternion = packet.orientationQuaternion();
					cout << "\r"
						<< "q0:" << quaternion.w()
						<< ", q1:" << quaternion.x()
						<< ", q2:" << quaternion.y()
						<< ", q3:" << quaternion.z();

					XsEuler euler = packet.orientationEuler();
					cout << " |Roll:" << euler.roll()
						<< ", Pitch:" << euler.pitch()
						<< ", Yaw:" << euler.yaw() << "\n";
				}

				if (packet.containsLatitudeLongitude())
				{
					XsVector latLon = packet.latitudeLongitude();
					cout << " |Lat:" << latLon[0]
						<< ", Lon:" << latLon[1];
				}

				if (packet.containsAltitude())
					cout << " |Alt:" << packet.altitude();

				if (packet.containsVelocity())
				{
					XsVector vel = packet.velocity(XDI_CoordSysEnu);
					cout << " |E:" << vel[0]
						<< ", N:" << vel[1]
						<< ", U:" << vel[2];
				}
			}
		}
		cout << flush;
		XsTime::msleep(0);
	}
	cout << "\n" << string(79, '-') << "\n";
	cout << endl;

	cout << "Closing port..." << endl;
	for (int i = 0; i < PortNum; ++i)
	{
		control->closePort(mtPort[i].portName().toStdString());
	}

	cout << "Freeing XsControl object..." << endl;
	control->destruct();

	cout << "Successful exit." << endl;

	cout << "Press [ENTER] to continue." << endl;
	cin.get();

	return 0;
}
