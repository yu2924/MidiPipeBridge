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
		hstring name;
		hstring parentName;
		hstring deviceId;
		event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	public:
		MidiDeviceInfo() = delete;
		MidiDeviceInfo(const hstring& vname, const hstring& vpname, const hstring& vdevid);
		hstring Name();
		void Name(const hstring& value);
		hstring ParentName();
		void ParentName(const hstring& value);
		hstring DeviceId();
		void DeviceId(const hstring& value);
		event_token PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& value);
		void PropertyChanged(const event_token& token);
		// class methods
		static MidiPipeBridge::MidiDeviceInfo NoneMidiDeviceInfo();
		static bool IsValidId(const hstring& id);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct MidiDeviceInfo : MidiDeviceInfoT<MidiDeviceInfo, implementation::MidiDeviceInfo>
	{
	};
}
