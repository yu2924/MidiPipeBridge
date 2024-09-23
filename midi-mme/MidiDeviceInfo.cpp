//
//  MidiDeviceInfo.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-04
//

#include "pch.h"
#include "MidiDeviceInfo.h"
#if __has_include("MidiDeviceInfo.g.cpp")
#include "MidiDeviceInfo.g.cpp"
#endif

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	MidiDeviceInfo::MidiDeviceInfo(const hstring& devname, uint32_t devid)
		: deviceName(devname)
		, deviceId(devid)
	{
	}
	hstring MidiDeviceInfo::DeviceName()
	{
		return deviceName;
	}
	void MidiDeviceInfo::DeviceName(const hstring& value)
	{
		if(deviceName == value) return;
		deviceName = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"DeviceName" });
	}
	uint32_t MidiDeviceInfo::DeviceId()
	{
		return deviceId;
	}
	void MidiDeviceInfo::DeviceId(uint32_t value)
	{
		if(deviceId == value) return;
		deviceId = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"DeviceId" });
	}
	event_token MidiDeviceInfo::PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler)
	{
		return propertyChanged.add(handler);
	}
	void MidiDeviceInfo::PropertyChanged(const event_token& token)
	{
		propertyChanged.remove(token);
	}
	// --------------------------------------------------------------------------------
	// class methods
	MidiPipeBridge::MidiDeviceInfo MidiDeviceInfo::NoneMidiDeviceInfo()
	{
		static MidiPipeBridge::MidiDeviceInfo noneMidiDeviceInfo = winrt::make<MidiDeviceInfo>(L"(none)", (uint32_t)-2);
		return noneMidiDeviceInfo;
	}
	bool MidiDeviceInfo::IsValidDeviceId(uint32_t devid, bool output)
	{
		if(output)	return (0 <= (int)devid) || (devid == MIDI_MAPPER);
		else		return 0 <= (int)devid;
	}
}
