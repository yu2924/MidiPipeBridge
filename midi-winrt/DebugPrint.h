//
//  DebugPrint.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-25
//

#pragma once

#include <format>

#if defined(_DEBUG)
#define DebugPrint(...) OutputDebugStringW(std::format(__VA_ARGS__).c_str())
#else
#define DebugPrint __noop
#endif
