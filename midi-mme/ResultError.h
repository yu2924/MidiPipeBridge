//
//  ResultError.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-04
//

#pragma once

#include "ResultError.g.h"
#include <winrt/Windows.UI.Xaml.h>

namespace winrt::MidiPipeBridge::implementation
{
	struct ResultError : ResultErrorT<ResultError>
	{
	private:
		MidiPipeBridge::ResultType resultType = MidiPipeBridge::ResultType::ResultTypeCom;
		int32_t resultCode = S_OK;
		event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	public:
		ResultError() = delete;
		ResultError(MidiPipeBridge::ResultType rt);
		void Reset();
		bool IsError();
		int32_t Code();
		void Code(int32_t value);
		hstring Message();
		Microsoft::UI::Xaml::Visibility Visibility();
		event_token PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler);
		void PropertyChanged(const event_token& token);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct ResultError : ResultErrorT<ResultError, implementation::ResultError>
	{
	};
}
