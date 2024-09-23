//
//  MidiDeviceWatcher.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-31
//

#pragma once

#include "MidiDeviceInfo.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <functional>

namespace winrt::MidiPipeBridge::implementation
{
	class MidiDeviceWatcher
	{
	private:
		class Impl;
		std::unique_ptr<Impl> impl;
	public:
		std::function<void()> OnRefreshDeviceList;
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> DeviceInfoList;
		MidiDeviceWatcher() = delete;
		MidiDeviceWatcher(bool isoutput, Microsoft::UI::Dispatching::DispatcherQueue dispqueue);
		~MidiDeviceWatcher();
		void StartWatcher();
		void StopWatcher();
	};
}
