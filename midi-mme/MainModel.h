//
//  MainModel.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-06
//

#pragma once

#include "MainModel.g.h"
#include "AppSettings.h"
#include "MidiDeviceInfo.h"
#include "ResultError.h"

namespace winrt::MidiPipeBridge::implementation
{
	struct MainModel : MainModelT<MainModel>
	{
	private:
		class Impl;
		std::unique_ptr<Impl> impl;
	public:
		MainModel() = delete;
		MainModel(Microsoft::UI::Dispatching::DispatcherQueue dispqueue, MidiPipeBridge::AppSettings settings);
		virtual ~MainModel() override;
		void Shutdown();
		hstring PipeName();
		void PipeName(const hstring& value);
		bool RunAsServer();
		void RunAsServer(bool value);
		bool IsConnecting();
		void IsConnecting(bool value);
		bool IsDisconnected();
		MidiPipeBridge::MidiDeviceInfo MidiInDeviceInfo();
		void MidiInDeviceInfo(const MidiPipeBridge::MidiDeviceInfo& value);
		MidiPipeBridge::MidiDeviceInfo MidiOutDeviceInfo();
		void MidiOutDeviceInfo(const MidiPipeBridge::MidiDeviceInfo& value);
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> MidiInDeviceInfoList();
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> MidiOutDeviceInfoList();
		MidiPipeBridge::ResultError PipeError();
		MidiPipeBridge::ResultError MidiInError();
		MidiPipeBridge::ResultError MidiOutError();
		event_token PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler);
		void PropertyChanged(const event_token& token);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct MainModel : MainModelT<MainModel, implementation::MainModel>
	{
	};
}
