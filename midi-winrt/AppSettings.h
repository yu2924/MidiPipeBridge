//
//  AppSettings.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#pragma once

#include "AppSettings.g.h"

namespace winrt::MidiPipeBridge::implementation
{
	struct AppSettings : AppSettingsT<AppSettings>
	{
	private:
		class Impl;
		std::unique_ptr<Impl> impl;
	public:
		AppSettings() = delete;
		AppSettings(Microsoft::UI::Dispatching::DispatcherQueue dispqueue);
		virtual ~AppSettings() override;
		void SaveIfNeeded();
		bool HasProperty(hstring propname);
		bool Topmost();
		void Topmost(bool value);
		Windows::Graphics::PointInt32 WindowPosition();
		void WindowPosition(const Windows::Graphics::PointInt32& value);
		hstring PipeName();
		void PipeName(const hstring& value);
		bool RunAsServer();
		void RunAsServer(bool value);
		hstring MidiInDeviceId();
		void MidiInDeviceId(const hstring& value);
		hstring MidiOutDeviceId();
		void MidiOutDeviceId(const hstring& value);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct AppSettings : AppSettingsT<AppSettings, implementation::AppSettings>
	{
	};
}
