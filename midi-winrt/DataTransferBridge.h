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
		std::function<void(HRESULT)> OnMidiInError;
		std::function<void(HRESULT)> OnMidiOutError;
		DataTransferBridge() = delete;
		DataTransferBridge(Microsoft::UI::Dispatching::DispatcherQueue dispqueue);
		~DataTransferBridge();
		hstring GetMidiInDeviceId() const;
		void SetMidiInDeviceId(const hstring& v);
		hstring GetMidiOutDeviceId() const;
		void SetMidiOutDeviceId(const hstring& v);
		bool StartSession(const std::wstring& pipename, bool runasserver);
		void StopSession();
		bool IsSessionRunning() const;
	};
}
