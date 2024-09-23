//
//  HresultError.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-04
//

#pragma once

#include "HresultError.g.h"
#include <winrt/Windows.UI.Xaml.h>

namespace winrt::MidiPipeBridge::implementation
{
	struct HresultError : HresultErrorT<HresultError>
	{
	private:
		HRESULT hresultCode = S_OK;
		event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	public:
		HresultError();
		void Reset();
		HRESULT Code();
		void Code(const HRESULT& value);
		hstring Message();
		Microsoft::UI::Xaml::Visibility Visibility();
		event_token PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler);
		void PropertyChanged(const event_token& token);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct HresultError : HresultErrorT<HresultError, implementation::HresultError>
	{
	};
}
