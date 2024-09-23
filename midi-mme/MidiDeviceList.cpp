//
//  MidiDeviceList.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-31
//

#include "pch.h"
#include <mmeapi.h>
#pragma comment(lib, "Winmm.lib")
#include "MidiDeviceList.h"
#include "DebugPrint.h"

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> EnumMidiDevices(bool isoutput)
	{
		Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> devlist = single_threaded_observable_vector<MidiPipeBridge::MidiDeviceInfo>();
		devlist.Append(MidiDeviceInfo::NoneMidiDeviceInfo());
		if(isoutput)
		{
			for(uint32_t c = midiOutGetNumDevs(), i = 0; i < c; ++i)
			{
				MIDIOUTCAPSW caps{};
				midiOutGetDevCapsW(i, &caps, sizeof(caps));
				devlist.Append(winrt::make<MidiDeviceInfo>(caps.szPname, i));
			}
		}
		else
		{
			for(uint32_t c = midiInGetNumDevs(), i = 0; i < c; ++i)
			{
				MIDIINCAPSW caps{};
				midiInGetDevCapsW(i, &caps, sizeof(caps));
				devlist.Append(winrt::make<MidiDeviceInfo>(caps.szPname, i));
			}
		}
		return devlist;
	}
} // winrt::MidiPipeBridge::implementation
