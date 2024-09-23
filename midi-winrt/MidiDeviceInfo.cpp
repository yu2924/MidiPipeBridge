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
	MidiDeviceInfo::MidiDeviceInfo(const hstring& vname, const hstring& vpname, const hstring& vdevid)
		: name(vname)
		, parentName(vpname)
		, deviceId(vdevid)
	{
	}
	hstring MidiDeviceInfo::Name()
	{
		return name;
	}
	void MidiDeviceInfo::Name(const hstring& value)
	{
		if(name == value) return;
		name = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Name" });
	}
	hstring MidiDeviceInfo::ParentName()
	{
		return parentName;
	}
	void MidiDeviceInfo::ParentName(const hstring& value)
	{
		if(parentName == value) return;
		parentName = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"ParentName" });
	}
	hstring MidiDeviceInfo::DeviceId()
	{
		return deviceId;
	}
	void MidiDeviceInfo::DeviceId(const hstring& value)
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
		static MidiPipeBridge::MidiDeviceInfo noneMidiDeviceInfo = winrt::make<MidiDeviceInfo>(L"(none)", L"---", L"");
		return noneMidiDeviceInfo;
	}
	bool MidiDeviceInfo::IsValidId(const hstring& devid)
	{
		return ((std::wstring_view)devid).substr(0, 4) == L"\\\\?\\";
	}
}
