//
//  OnetimeInvoker.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-08-31
//

#include "pch.h"
#include "OnetimeInvoker.h"

using namespace winrt;
using namespace Microsoft::UI::Dispatching;

namespace winrt::MidiPipeBridge::implementation
{
	OnetimeInvoker::OnetimeInvoker(Microsoft::UI::Dispatching::DispatcherQueue dispqueue) : dispatcherQueue(dispqueue)
	{
	}
	void OnetimeInvoker::Trigger()
	{
		if(triggered) return;
		triggered = true;
		dispatcherQueue.TryEnqueue(Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this]()
		{
			if(OnInvoke) OnInvoke();
			triggered = false;
		});
	}
	TimedOnetimeInvoker::TimedOnetimeInvoker(Microsoft::UI::Dispatching::DispatcherQueue dispqueue, unsigned int ms)
	{
		timer = dispqueue.CreateTimer();
		timer.IsRepeating(false);
		timer.Interval(Windows::Foundation::TimeSpan{ ms * 10000ll });
		timer.Tick([this](const Microsoft::UI::Dispatching::DispatcherQueueTimer&, const Windows::Foundation::IInspectable&)
		{
			timer.Stop();
			if(OnInvoke) OnInvoke();
		});
	}
	void TimedOnetimeInvoker::Trigger()
	{
		timer.Start();
	}
}