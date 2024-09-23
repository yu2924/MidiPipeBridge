//
//  DataTransferBridge.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#pragma once

#include <winrt/Microsoft.UI.Dispatching.h>
#include <functional>

namespace winrt::MidiPipeBridge::implementation
{
	class DataTransferBridge
	{
	private:
		class Impl;
		std::unique_ptr<Impl> impl;
	public:
		std::function<void(HRESULT)> OnPipeError;
		std::function<void(MMRESULT)> OnMidiInError;
		std::function<void(MMRESULT)> OnMidiOutError;
		DataTransferBridge() = delete;
		DataTransferBridge(Microsoft::UI::Dispatching::DispatcherQueue dispqueue);
		~DataTransferBridge();
		uint32_t GetMidiInDeviceId() const;
		void SetMidiInDeviceId(uint32_t v);
		uint32_t GetMidiOutDeviceId() const;
		void SetMidiOutDeviceId(uint32_t v);
		bool StartSession(const std::wstring& pipename, bool runasserver);
		void StopSession();
		bool IsSessionRunning() const;
	};
}
