#include "stdafx.h"
#include "Tablet.h"
#include "TabletHandler.h"

#define LOG_MODULE "Tablet"
#include "Logger.h"


//
// USB Device Constructor
//
Tablet::Tablet(string usbGUID) : Tablet() {
	usbDevice = new USBDevice(usbGUID);
	if(usbDevice->isOpen) {
		this->isOpen = true;
		usbPipeId = 0x81;
	}
	else {
		delete usbDevice;
		usbDevice = NULL;
	}
}

//
// HID Device Constructor
//
Tablet::Tablet(USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage) : Tablet() {
	hidDevice = new HIDDevice(vendorId, productId, usagePage, usage);
	if(hidDevice->isOpen) {
		this->isOpen = true;
	}
	else {
		delete hidDevice;
		hidDevice = NULL;
	}
}

//
// Common constructor
//
Tablet::Tablet() {

	name = "Unknown";
	usbDevice = NULL;
	hidDevice = NULL;
	hidDeviceAux = NULL;

	usbPipeId = 0;

	// Init reports
	initFeature = NULL;
	initFeatureLength = 0;
	initReport = NULL;
	initReportLength = 0;

	// Timed filters
	filterTimed[0] = &smoothing;
	filterTimed[1] = &gravityFilter;
	filterTimedCount = 2;

	// Report filters
	filterReport[0] = &antiSmoothing;
	filterReport[1] = &noiseFilter;
	filterReportCount = 2;

	// Button map
	memset(&buttonMap, 0, sizeof(buttonMap));
	buttonMap[0] = 1;
	buttonMap[1] = 2;
	buttonMap[2] = 3;


	// Tablet connection open
	isOpen = false;

	// Skip first reports, some of those might be invalid.
	skipReports = 5;

	// Keep tip down report counter
	tipDownCounter = 0;

}

//
// Destructor
//
Tablet::~Tablet() {
	CloseDevice();
	if(usbDevice != NULL)
		delete usbDevice;
	if(hidDevice != NULL)
		delete hidDevice;
	if(hidDeviceAux != NULL)
		delete hidDeviceAux;
	if(initReport != NULL)
		delete initReport;
	if(initFeature != NULL)
		delete initFeature;
}


//
// Init
//
bool Tablet::Init() {

	// Init string requests
	if(initStrings.size() > 0) {
		for(int stringId : initStrings) {
			GetDeviceString(stringId);
		}
	}

	// Feature report
	if(initFeature != NULL) {
		if(hidDevice->SetFeature(initFeature, initFeatureLength)) {
			return true;
		}
		return false;
	}

	// Output report
	if(initReport != NULL) {
		if(hidDevice->Write(initReport, initReportLength)) {
			return true;
		}
		return false;
	}

	return true;
}


//
// Check if the tablet has enough configuration parameters set
//
bool Tablet::IsConfigured() {
	if(
		settings.maxX > 1 &&
		settings.maxY > 1 &&
		settings.maxPressure > 1 &&
		settings.width > 1 &&
		settings.height > 1
	) return true;
	return false;
}


//
// Get a device string from HID or USB device.
//
string Tablet::GetDeviceString(UCHAR stringId)
{
	string resultString = "";
	UCHAR buffer[256];
	int bytesRead = 0;

	// USB device
	if(usbDevice != NULL) {
		bytesRead = usbDevice->StringRequest(stringId, buffer, 256);
	}

	// HID device
	else if(hidDevice != NULL) {
		if(hidDevice->isReading) {
			throw runtime_error("HID string request can't be sent when the device is in use!");
		}
		else {
			bytesRead = hidDevice->StringRequest(stringId, buffer, 256);
		}
	}

	// Reply received?
	if(bytesRead > 0) {
		for(int i = 0; i < bytesRead; i += 2) {
			resultString.push_back(buffer[i]);
		}
	}

	return resultString;
}


//
// Read Position
//
int Tablet::ReadPosition() {
	UCHAR buffer[1024];
	UCHAR *data;
	int buttonIndex;


	// Read report
	if(!this->Read(buffer, settings.reportLength)) {
		return -1;
	}

	// Skip reports
	if(skipReports > 0) {
		skipReports--;
		return Tablet::ReportInvalid;
	}


	// Set data pointer
	if(settings.dataFormat == TabletSettings::TabletFormatWacomDrivers) {
		data = buffer + 1;
	}
	else {
		data = buffer;
	}


	//
	// Wacom Intuos data format V2
	//
	if(settings.dataFormat == TabletSettings::TabletFormatWacomIntuosV2) {

		// Wacom driver device
		if(settings.reportLength == 11) {
			data = buffer + 1;
		}

		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = ((data[2] * 0x100 + data[3]) << 1) | ((data[9] >> 1) & 1);
		reportData.y = ((data[4] * 0x100 + data[5]) << 1) | (data[9] & 1);
		reportData.pressure = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
		//distance = buffer[9] >> 2;


	}

	//
	// Wacom Intuos data format V3 (Wacom 4100)
	//
	else if(settings.dataFormat == TabletSettings::TabletFormatWacomIntuosV3) {

		// Wacom driver device
		if(settings.reportLength == 193) {
			data = buffer + 1;
		}

		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = (data[2] | (data[3] << 8) | (data[4] << 16));
		reportData.y = (data[5] | (data[6] << 8) | (data[7] << 16));
		reportData.pressure = (data[8] | (data[9] << 8));

	}

	//
	// Skip first data byte (VEIKK S640)
	//
	else if(settings.dataFormat == TabletSettings::TabletFormatSkipFirstDataByte) {

		// Validate report length
		if(settings.reportLength >= 9) {

			// Offset report data
			memcpy(&reportData, (data + 1), sizeof(reportData));

			// Set report id
			reportData.reportId = data[0];
		}

	}

	//
	// Copy buffer to struct
	//
	else {
		memcpy(&reportData, data, sizeof(reportData));
	}


	// Validate report id
	if(settings.reportId > 0 && reportData.reportId != settings.reportId) {
		return Tablet::ReportInvalid;
	}



	// Detect mask
	if(settings.detectMask > 0 && (reportData.buttons & settings.detectMask) != settings.detectMask) {
		return Tablet::ReportPositionInvalid;
	}

	// Ignore mask
	if(settings.ignoreMask > 0 && (reportData.buttons & settings.ignoreMask) == settings.ignoreMask) {
		return Tablet::ReportIgnore;
	}

	//
	// Use pen pressure to detect the pen tip click
	//
	if(settings.clickPressure > 0) {
		reportData.buttons &= ~1;
		if(reportData.pressure > settings.clickPressure) {
			reportData.buttons |= 1;
		}


	}

	// Force tip button down if pressure is detected
	else if(reportData.pressure > 10) {
		reportData.buttons |= 1;
	}

	// Keep pen tip button down for a few reports
	if(settings.keepTipDown > 0) {
		if(reportData.buttons & 0x01) {
			tipDownCounter = settings.keepTipDown;
		}
		if(tipDownCounter-- >= 0) {
			reportData.buttons |= 1;
		}
	}


	// Set valid
	state.isValid = true;

	state.time = chrono::high_resolution_clock::now();


	// Button map
	reportData.buttons = reportData.buttons & 0x0F;
	state.buttons = 0;
	for(buttonIndex = 0; buttonIndex < sizeof(buttonMap); buttonIndex++) {

		// Button is set
		if(buttonMap[buttonIndex] > 0) {

			// Button is pressed
			if((reportData.buttons & (1 << buttonIndex)) > 0) {
				state.buttons |= (1 << (buttonMap[buttonIndex] - 1));
			}
		}
	}

	// Convert report data to state
	state.position.x = ((double)reportData.x / (double)settings.maxX) * settings.width;
	state.position.y = ((double)reportData.y / (double)settings.maxY) * settings.height;
	if(settings.skew != 0) {
		state.position.x += state.position.y * settings.skew;
	}
	state.pressure = ((double)reportData.pressure / (double)settings.maxPressure);

	// Tablet measurement update
	if(measurement.isRunning) {
		state.buttons = reportData.buttons & 0x0F;
		measurement.Update(state);
		return Tablet::ReportInvalid;
	}

	// Report and position is valid
	return Tablet::ReportValid;
}


//
// Read report from tablet
//
bool Tablet::Read(void *buffer, int length) {
	if(!isOpen) return false;
	bool status = false;
	if(usbDevice != NULL) {
		status = usbDevice->Read(usbPipeId, buffer, length) > 0;
	}
	else if(hidDevice != NULL) {
		status = hidDevice->Read(buffer, length);
	}
	if(logger.debugEnabled && status) {
		LOG_DEBUGBUFFER(buffer, length, "Read: ");
	}
	return status;
}

//
// Write report to the tablet
//
bool Tablet::Write(void *buffer, int length) {
	if(!isOpen) return false;
	if(usbDevice != NULL) {
		return usbDevice->Write(usbPipeId, buffer, length) > 0;
	}
	else if(hidDevice != NULL) {
		return hidDevice->Write(buffer, length);
	}
	return false;
}

//
// Close tablet
//
void Tablet::CloseDevice() {
	if(isOpen) {
		if(usbDevice != NULL) {
			usbDevice->CloseDevice();
		}
		else if(hidDevice != NULL) {
			hidDevice->CloseDevice();
		}
	}
	isOpen = false;
}