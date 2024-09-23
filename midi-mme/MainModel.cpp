//
//  MainModel.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-06
//

#include "pch.h"
#include "MainModel.h"
#if __has_include("MainModel.g.cpp")
#include "MainModel.g.cpp"
#endif
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Storage.h>
#include "MidiDeviceList.h"
#include "DataTransferBridge.h"
#include "DebugPrint.h"

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	class MainModel::Impl
	{
	public:
		// 
		// NOTE:
		// In non-packaged apps, the Microsoft::UI::Xaml::LaunchActivatedEventArgs class is useless, so use native Win32 API to obtain command line parameters.
		// ---
		// examples of command line parameters
		// - run as server with specific pipe name:
		//		server pipename="\\.\pipe\midipipe"
		// - select a specific device:
		//		midiout="Port 1 on Micro"
		// - do not select any device:
		//		midiin="" midiout=""
		// 
		struct CommandLineOptions
		{
			std::optional<hstring> pipename;
			std::optional<hstring> midiindevicename;
			std::optional<hstring> midioutdevicename;
			std::optional<bool> runasserver;
			CommandLineOptions()
			{
				static const hstring OptPipeName{ L"pipename=" };
				static const hstring OptMidiIn	{ L"midiin=" };
				static const hstring OptMidiOut	{ L"midiout=" };
				static const hstring OptServer	{ L"server" };
				LPCWSTR cmdline = GetCommandLineW();
				int argc = 0;
				LPWSTR* argv = CommandLineToArgvW(cmdline, &argc);
				for(int i = 0; i < argc; ++i)
				{
					LPCWSTR arg = argv[i];
					if     (!pipename			.has_value() && (_wcsnicmp(arg, OptPipeName	.c_str(), OptPipeName	.size()) == 0)) pipename			= arg + OptPipeName	.size();
					else if(!midiindevicename	.has_value() && (_wcsnicmp(arg, OptMidiIn	.c_str(), OptMidiIn		.size()) == 0)) midiindevicename	= arg + OptMidiIn	.size();
					else if(!midiindevicename	.has_value() && (_wcsnicmp(arg, OptMidiOut	.c_str(), OptMidiOut	.size()) == 0)) midiindevicename	= arg + OptMidiOut	.size();
					else if(!runasserver		.has_value() && (_wcsnicmp(arg, OptServer	.c_str(), OptServer		.size()) == 0)) runasserver			= true;
				}
				LocalFree(argv);
			}
		};
		// --------------------------------------------------------------------------------
		MainModel* outer = nullptr;
		MidiPipeBridge::AppSettings appSettings = nullptr;
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> midiInDeviceList;
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> midiOutDeviceList;
		std::unique_ptr<DataTransferBridge> dataTtransferBridge;
		hstring pipeName;
//		bool topmost = false;
		bool runAsServer = false;
		bool isConnecting = false;
		MidiPipeBridge::MidiDeviceInfo midiInDeviceInfo = nullptr;
		MidiPipeBridge::MidiDeviceInfo midiOutDeviceInfo = nullptr;
		MidiPipeBridge::ResultError pipeError = nullptr;
		MidiPipeBridge::ResultError midiInError = nullptr;
		MidiPipeBridge::ResultError midiOutError = nullptr;
		event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
		Impl(MainModel* p, Microsoft::UI::Dispatching::DispatcherQueue dispqueue, MidiPipeBridge::AppSettings settings)
			: outer(p)
			, appSettings(settings)
		{
			static const struct
			{
				hstring pipeName{ L"\\\\.\\pipe\\midipipe" };
				hstring midiInDeviceName{};
				hstring midiOutDeviceName{};
				bool runAsServer = false;
			} Defaults;
			CommandLineOptions cmdopt;
			midiInDeviceList = EnumMidiDevices(false);
			midiOutDeviceList = EnumMidiDevices(true);
			dataTtransferBridge = std::make_unique<DataTransferBridge>(dispqueue);
			dataTtransferBridge->OnPipeError = [this](HRESULT r) { pipeError.Code(r); IsConnecting(false); };
			dataTtransferBridge->OnMidiInError = [this](MMRESULT r) { midiInError.Code(r); IsConnecting(false); };
			dataTtransferBridge->OnMidiOutError = [this](MMRESULT r) { midiOutError.Code(r); IsConnecting(false); };
			pipeName = cmdopt.pipename.has_value() ? cmdopt.pipename.value() : (appSettings.HasProperty(L"PipeName") ? appSettings.PipeName() : Defaults.pipeName);
			runAsServer = cmdopt.runasserver.has_value() ? cmdopt.runasserver.value() : (appSettings.HasProperty(L"RunAsServer") ? appSettings.RunAsServer() : Defaults.runAsServer);
			isConnecting = false;
			hstring midiindevname = cmdopt.midiindevicename.has_value() ? cmdopt.midiindevicename.value() : (appSettings.HasProperty(L"MidiInDeviceName") ? appSettings.MidiInDeviceName() : Defaults.midiInDeviceName);
			hstring midioutdevname = cmdopt.midioutdevicename.has_value() ? cmdopt.midioutdevicename.value() : (appSettings.HasProperty(L"MidiOutDeviceName") ? appSettings.MidiOutDeviceName() : Defaults.midiOutDeviceName);
			pipeError = winrt::make<ResultError>(MidiPipeBridge::ResultType::ResultTypeCom);
			midiInError = winrt::make<ResultError>(MidiPipeBridge::ResultType::ResultTypeMidiIn);
			midiOutError = winrt::make<ResultError>(MidiPipeBridge::ResultType::ResultTypeMidiOut);
			MidiInDeviceInfo(ResolveSelectedMidiInDevice(midiindevname));
			MidiOutDeviceInfo(ResolveSelectedMidiOutDevice(midioutdevname));
		}
		// --------------------------------------------------------------------------------
		// internals
		MidiPipeBridge::MidiDeviceInfo ResolveSelectedMidiInDevice(const hstring& devname)
		{
			MidiPipeBridge::MidiDeviceInfo newinf = MidiDeviceInfo::NoneMidiDeviceInfo();
			for(const auto& inf : midiInDeviceList) { if(inf.DeviceName() == devname) { newinf = inf; break; } }
			return newinf;
		}
		MidiPipeBridge::MidiDeviceInfo ResolveSelectedMidiOutDevice(const hstring& devname)
		{
			MidiPipeBridge::MidiDeviceInfo newinf = MidiDeviceInfo::NoneMidiDeviceInfo();
			for(const auto& inf : midiOutDeviceList) { if(inf.DeviceName() == devname) { newinf = inf; break; } }
			return newinf;
		}
		// --------------------------------------------------------------------------------
		// public APIs
		void Shutdown()
		{
			// Don't call StopSession() here, it may cause asynchronous callbacks
			dataTtransferBridge.reset();
		}
		hstring PipeName()
		{
			return pipeName;
		}
		void PipeName(const hstring& value)
		{
			if(pipeName == value) return;
			pipeName = value;
			appSettings.PipeName(pipeName);
			propertyChanged(*outer, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"PipeName" });
		}
		bool RunAsServer()
		{
			return runAsServer;
		}
		void RunAsServer(bool value)
		{
			if(runAsServer == value) return;
			runAsServer = value;
			appSettings.RunAsServer(runAsServer);
			propertyChanged(*outer, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"RunAsServer" });
		}
		bool IsConnecting()
		{
			return isConnecting;
		}
		void IsConnecting(bool value)
		{
			if(isConnecting == value) return;
			isConnecting = value;
			propertyChanged(*outer, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"IsConnecting" });
			propertyChanged(*outer, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"IsDisconnected" });
			if(isConnecting)
			{
				pipeError.Reset();
				dataTtransferBridge->StartSession((std::wstring)pipeName, runAsServer);
			}
			else
			{
				dataTtransferBridge->StopSession();
			}
		}
		bool IsDisconnected()
		{
			return !isConnecting;
		}
		MidiPipeBridge::MidiDeviceInfo MidiInDeviceInfo()
		{
			return midiInDeviceInfo;
		}
		void MidiInDeviceInfo(const MidiPipeBridge::MidiDeviceInfo& value)
		{
			// ignore transient null
			if(!value) return;
			if(midiInDeviceInfo == value) return;
			midiInDeviceInfo = value;
			appSettings.MidiInDeviceName(midiInDeviceInfo.DeviceName());
			midiInError.Reset();
			dataTtransferBridge->SetMidiInDeviceId(midiInDeviceInfo.DeviceId());
			propertyChanged(*outer, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"MidiInDeviceInfo" });
		}
		MidiPipeBridge::MidiDeviceInfo MidiOutDeviceInfo()
		{
			return midiOutDeviceInfo;
		}
		void MidiOutDeviceInfo(const MidiPipeBridge::MidiDeviceInfo& value)
		{
			// ignore transient null
			if(!value) return;
			if(midiOutDeviceInfo == value) return;
			midiOutDeviceInfo = value;
			appSettings.MidiOutDeviceName(midiOutDeviceInfo.DeviceName());
			midiOutError.Reset();
			dataTtransferBridge->SetMidiOutDeviceId(midiOutDeviceInfo.DeviceId());
			propertyChanged(*outer, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ L"MidiOutDeviceInfo" });
		}
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> MidiInDeviceInfoList()
		{
			return midiInDeviceList;
		}
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> MidiOutDeviceInfoList()
		{
			return midiOutDeviceList;
		}
		MidiPipeBridge::ResultError PipeError()
		{
			return pipeError;
		}
		MidiPipeBridge::ResultError MidiInError()
		{
			return midiInError;
		}
		MidiPipeBridge::ResultError MidiOutError()
		{
			return midiOutError;
		}
		event_token PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler)
		{
			return propertyChanged.add(handler);
		}
		void PropertyChanged(const event_token& token)
		{
			propertyChanged.remove(token);
		}
	};
	MainModel::MainModel(Microsoft::UI::Dispatching::DispatcherQueue dispqueue, MidiPipeBridge::AppSettings settings) { impl = std::make_unique<Impl>(this, dispqueue, settings); }
	MainModel::~MainModel() { impl.reset(); }
	void MainModel::Shutdown() { impl->Shutdown(); }
	hstring MainModel::PipeName() { return impl->PipeName(); }
	void MainModel::PipeName(const hstring& value) { impl->PipeName(value); }
	bool MainModel::RunAsServer() { return impl->RunAsServer(); }
	void MainModel::RunAsServer(bool value) { impl->RunAsServer(value); }
	bool MainModel::IsConnecting() { return impl->IsConnecting(); }
	void MainModel::IsConnecting(bool value) { impl->IsConnecting(value); }
	bool MainModel::IsDisconnected() { return impl->IsDisconnected(); }
	MidiPipeBridge::MidiDeviceInfo MainModel::MidiInDeviceInfo() { return impl->MidiInDeviceInfo(); }
	void MainModel::MidiInDeviceInfo(const MidiPipeBridge::MidiDeviceInfo& value) { impl->MidiInDeviceInfo(value); }
	MidiPipeBridge::MidiDeviceInfo MainModel::MidiOutDeviceInfo() { return impl->MidiOutDeviceInfo(); }
	void MainModel::MidiOutDeviceInfo(const MidiPipeBridge::MidiDeviceInfo& value) { impl->MidiOutDeviceInfo(value); }
	Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> MainModel::MidiInDeviceInfoList() { return impl->MidiInDeviceInfoList(); }
	Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> MainModel::MidiOutDeviceInfoList() { return impl->MidiOutDeviceInfoList(); }
	MidiPipeBridge::ResultError MainModel::PipeError() { return impl->PipeError(); }
	MidiPipeBridge::ResultError MainModel::MidiInError() { return impl->MidiInError(); }
	MidiPipeBridge::ResultError MainModel::MidiOutError() { return impl->MidiOutError(); }
	event_token MainModel::PropertyChanged(const Microsoft::UI::Xaml::Data::PropertyChangedEventHandler& handler) { return impl->PropertyChanged(handler); }
	void MainModel::PropertyChanged(const event_token& token) { return impl->PropertyChanged(token); }
} // winrt::MidiPipeBridge::implementation
