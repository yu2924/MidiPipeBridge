//
//  MidiDeviceList.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-31
//

#pragma once

#include "MidiDeviceInfo.h"

namespace winrt::MidiPipeBridge::implementation
{
	Windows::Foundation::Collections::IObservableVector<MidiPipeBridge::MidiDeviceInfo> EnumMidiDevices(bool isoutput);
}
