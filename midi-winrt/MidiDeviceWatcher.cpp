//
//  MidiDeviceWatcher.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-31
//

#include "pch.h"
#include "MidiDeviceWatcher.h"
#include <winrt/base.h>
#include <winrt/windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>
#include <cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#define INITGUID
#include <devpkey.h>
#include "MidiDeviceInfo.h"
#include "OnetimeInvoker.h"
#include "DebugPrint.h"

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	class MidiDeviceWatcher::Impl
	{
	public:
		// 
		// NOTE:
		// The device name provided by Windows::Devices::Enumeration::DeviceInformation is not sufficient to distinguish it,
		// so add the name of the device's parent node.
		// 
		static std::wstring GetDevNodePropString(DEVINST devinst, const DEVPROPKEY* pkey)
		{
			DEVPROPTYPE type = 0;
			ULONG cbbuf = 0;
			if((CM_Get_DevNode_PropertyW(devinst, pkey, &type, nullptr, &cbbuf, 0) != CR_BUFFER_SMALL) || (type != DEVPROP_TYPE_STRING)) return {};
			std::vector<BYTE> buf(cbbuf);
			if((CM_Get_DevNode_PropertyW(devinst, pkey, &type, buf.data(), &cbbuf, 0) != CR_SUCCESS) || (type != DEVPROP_TYPE_STRING)) return {};
			return std::wstring(reinterpret_cast<WCHAR*>(buf.data()), cbbuf / sizeof(WCHAR));
		}
		static std::wstring GetDeviceInstanceParentName(const std::wstring& devinstid)
		{
			if(devinstid.empty()) return {};
			DEVINST devinst{};
			if(CM_Locate_DevNodeW(&devinst, const_cast<WCHAR*>(devinstid.c_str()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) return {};
			DEVINST parentdevinst{};
			if(CM_Get_Parent(&parentdevinst, devinst, 0) != CR_SUCCESS) return {};
			std::wstring parentname = GetDevNodePropString(parentdevinst, &DEVPKEY_Device_FriendlyName);
			if(parentname.empty()) parentname = GetDevNodePropString(parentdevinst, &DEVPKEY_NAME);
			return parentname;
		}
		// --------------------------------------------------------------------------------
		MidiDeviceWatcher* outer;
		Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue = nullptr;
		std::unique_ptr<OnetimeInvoker> ontimeInvoker;
		hstring deviceSelectorString;
		Windows::Devices::Enumeration::DeviceWatcher deviceWatcher = nullptr;
		event_token evtAdded;
		event_token evtRemoved;
		event_token evtUpdated;
		event_token evtCompleted;
		bool isOutput = false;
		Impl(MidiDeviceWatcher* p, bool isoutput, Microsoft::UI::Dispatching::DispatcherQueue dispqueue)
			: outer(p)
			, dispatcherQueue(dispqueue)
			, isOutput(isoutput)
		{
			outer->DeviceInfoList = single_threaded_observable_vector<MidiPipeBridge::MidiDeviceInfo>();
			outer->DeviceInfoList.Append(MidiDeviceInfo::NoneMidiDeviceInfo());
			ontimeInvoker = std::make_unique<OnetimeInvoker>(dispqueue);
			ontimeInvoker->OnInvoke = [this]() { UpdateDeviceListAsync(); };
			deviceSelectorString = isoutput ? Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() : Windows::Devices::Midi::MidiInPort::GetDeviceSelector();
			deviceWatcher = Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(deviceSelectorString);
			evtAdded = deviceWatcher.Added([this](const Windows::Devices::Enumeration::DeviceWatcher&, const Windows::Devices::Enumeration::DeviceInformation&) { ontimeInvoker->Trigger(); });
			evtRemoved = deviceWatcher.Removed([this](const Windows::Devices::Enumeration::DeviceWatcher&, const Windows::Devices::Enumeration::DeviceInformationUpdate&) { ontimeInvoker->Trigger(); });
			evtUpdated = deviceWatcher.Updated([this](const Windows::Devices::Enumeration::DeviceWatcher&, const Windows::Devices::Enumeration::DeviceInformationUpdate&) { ontimeInvoker->Trigger(); });
			evtCompleted = deviceWatcher.EnumerationCompleted([this](const Windows::Devices::Enumeration::DeviceWatcher&, const Windows::Foundation::IInspectable&) { ontimeInvoker->Trigger(); });
		}
		~Impl()
		{
			deviceWatcher.Added(evtAdded);
			deviceWatcher.Removed(evtRemoved);
			deviceWatcher.Updated(evtUpdated);
			deviceWatcher.EnumerationCompleted(evtCompleted);
		}
		// --------------------------------------------------------------------------------
		// internals
		Windows::Foundation::IAsyncAction UpdateDeviceListAsync()
		{
			Windows::Devices::Enumeration::DeviceInformationCollection devinfolist = co_await Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(deviceSelectorString);
			std::vector<MidiPipeBridge::MidiDeviceInfo> tmplist;
			tmplist.reserve((devinfolist ? devinfolist.Size() : 0) + 1);
			tmplist.push_back(MidiDeviceInfo::NoneMidiDeviceInfo());
			if(devinfolist) for(const auto& devinfo : devinfolist)
			{
				if(!devinfo.IsEnabled()) continue;
				hstring devinstid = unbox_value_or<hstring>(devinfo.Properties().TryLookup(L"System.Devices.DeviceInstanceId"), L"");
				if(devinstid.empty()) continue;
				std::wstring parentname = GetDeviceInstanceParentName(devinstid.c_str());
				if(parentname.empty()) parentname = L"---";
				tmplist.push_back(winrt::make<MidiDeviceInfo>(devinfo.Name().c_str(), parentname.c_str(), devinfo.Id().c_str()));
			}
			std::stable_sort(tmplist.begin() + 1, tmplist.end(), [](const IMidiDeviceInfo& a, const IMidiDeviceInfo& b) { return a.ParentName() < b.ParentName(); });
			outer->DeviceInfoList.ReplaceAll(tmplist);
			DebugPrint(L"[DeviceListWatcher] update ({}) {} items\n", isOutput ? L"output" : L"input", outer->DeviceInfoList.Size());
			if(outer->OnRefreshDeviceList) outer->OnRefreshDeviceList();
		}
		// --------------------------------------------------------------------------------
		// public APIs
		void StartWatcher()
		{
			if(deviceWatcher.Status() != Windows::Devices::Enumeration::DeviceWatcherStatus::Started) deviceWatcher.Start();
		}
		void StopWatcher()
		{
			if(deviceWatcher.Status() != Windows::Devices::Enumeration::DeviceWatcherStatus::Stopped) deviceWatcher.Stop();
		}
	};
	MidiDeviceWatcher::MidiDeviceWatcher(bool isoutput, Microsoft::UI::Dispatching::DispatcherQueue dispqueue) { impl = std::make_unique<Impl>(this, isoutput, dispqueue); }
	MidiDeviceWatcher::~MidiDeviceWatcher() { impl.reset(); }
	void MidiDeviceWatcher::StartWatcher() { impl->StartWatcher(); }
	void MidiDeviceWatcher::StopWatcher() { impl->StopWatcher(); }
} // winrt::MidiPipeBridge::implementation
