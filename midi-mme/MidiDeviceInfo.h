//
//  MidiDeviceInfo.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-04
//

#pragma once

#include "MidiDeviceInfo.g.h"

namespace winrt::MidiPipeBridge::implementation
{
	struct MidiDeviceInfo : MidiDeviceInfoT<MidiDeviceInfo>
	{
	private:
		hstring deviceName;
		uint32_t deviceId;
		event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	public:
		MidiDeviceInfo() = delete;
		MidiDeviceInfo(const hstring& devname, uint32_t vdevid);
		hstring DeviceName();
		void DeviceName(const hstring& value);
		uint32_t DeviceId();
		void DeviceId(uint32_t value);
		event_token PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& value);
		void PropertyChanged(const event_token& token);
		static MidiPipeBridge::MidiDeviceInfo NoneMidiDeviceInfo();
		static bool IsValidDeviceId(uint32_t devid, bool output);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct MidiDeviceInfo : MidiDeviceInfoT<MidiDeviceInfo, implementation::MidiDeviceInfo>
	{
	};
}
