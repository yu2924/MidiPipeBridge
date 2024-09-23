//
//  HresultError.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-04
//

#include "pch.h"
#include "HresultError.h"
#if __has_include("HresultError.g.cpp")
#include "HresultError.g.cpp"
#endif

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	HresultError::HresultError()
	{
	}
	void HresultError::Reset()
	{
		Code(S_OK);
	}
	HRESULT HresultError::Code()
	{
		return hresultCode;
	}
	void HresultError::Code(const HRESULT& value)
	{
		if(hresultCode == value) return;
		hresultCode = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Code" });
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Message" });
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"Visibility" });
	}
	hstring HresultError::Message()
	{
		return FAILED(hresultCode) ? winrt::hresult_error(hresultCode).message() : winrt::hresult_error().message();
	}
	Microsoft::UI::Xaml::Visibility HresultError::Visibility()
	{
		return FAILED(hresultCode) ? Microsoft::UI::Xaml::Visibility::Visible : Microsoft::UI::Xaml::Visibility::Collapsed;
	}
	event_token HresultError::PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler)
	{
		return propertyChanged.add(handler);
	}
	void HresultError::PropertyChanged(const event_token& token)
	{
		propertyChanged.remove(token);
	}
}
