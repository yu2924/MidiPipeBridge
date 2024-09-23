//
//  OnetimeInvoker.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-31
//

#pragma once

#include <winrt/Microsoft.UI.Dispatching.h>
#include <functional>

namespace winrt::MidiPipeBridge::implementation
{
	class OnetimeInvoker
	{
	private:
		Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue = nullptr;
		bool triggered = false;
	public:
		std::function<void()> OnInvoke;
		OnetimeInvoker(Microsoft::UI::Dispatching::DispatcherQueue dispqueue);
		void Trigger();
	};
	class TimedOnetimeInvoker
	{
	private:
		Microsoft::UI::Dispatching::DispatcherQueueTimer timer = nullptr;
	public:
		std::function<void()> OnInvoke;
		TimedOnetimeInvoker(Microsoft::UI::Dispatching::DispatcherQueue dispqueue, unsigned int ms);
		void Trigger();
	};
}
