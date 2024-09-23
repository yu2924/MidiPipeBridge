//
//  ResultError.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-04
//

#include "pch.h"
#include "ResultError.h"
#if __has_include("ResultError.g.cpp")
#include "ResultError.g.cpp"
#endif
#include <mmeapi.h>
#pragma comment(lib, "Winmm.lib")

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	ResultError::ResultError(MidiPipeBridge::ResultType rt) : resultType(rt)
	{
	}
	void ResultError::Reset()
	{
		Code(S_OK);
	}
	bool ResultError::IsError()
	{
		switch(resultType)
		{
			case MidiPipeBridge::ResultType::ResultTypeCom: return FAILED(resultCode);
			case MidiPipeBridge::ResultType::ResultTypeMidiIn:
			case MidiPipeBridge::ResultType::ResultTypeMidiOut: return resultCode != MMSYSERR_NOERROR;
		}
		return false;
	}
	int32_t ResultError::Code()
	{
		return resultCode;
	}
	void ResultError::Code(int32_t value)
	{
		if(resultCode == value) return;
		resultCode = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Code" });
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Message" });
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Visibility" });
	}
	hstring ResultError::Message()
	{
		if(!IsError()) return {};
		switch(resultType)
		{
			case MidiPipeBridge::ResultType::ResultTypeCom:
				return winrt::hresult_error(resultCode).message();
			case MidiPipeBridge::ResultType::ResultTypeMidiIn:
			case MidiPipeBridge::ResultType::ResultTypeMidiOut:
			{
				wchar_t msg[128]{};
				if(resultType == MidiPipeBridge::ResultType::ResultTypeMidiIn)	midiInGetErrorTextW(resultCode, msg, _countof(msg));
				else															midiOutGetErrorTextW(resultCode, msg, _countof(msg));
				return msg;
			}
		}
		return {};
	}
	Microsoft::UI::Xaml::Visibility ResultError::Visibility()
	{
		return IsError() ? Microsoft::UI::Xaml::Visibility::Visible : Microsoft::UI::Xaml::Visibility::Collapsed;
	}
	event_token ResultError::PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler)
	{
		return propertyChanged.add(handler);
	}
	void ResultError::PropertyChanged(const event_token& token)
	{
		propertyChanged.remove(token);
	}
}
