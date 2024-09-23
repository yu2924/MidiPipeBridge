//
//  MainWindow.xaml.h
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#pragma once

#include "MainWindow.g.h"
#include "NoWheelComboBox.g.h"
#include "AppSettings.h"
#include "MainModel.h"
#include "OnetimeInvoker.h"

namespace winrt::MidiPipeBridge::implementation
{
	struct MainWindow : MainWindowT<MainWindow>
	{
	private:
		MidiPipeBridge::AppSettings appSettings = nullptr;
		MidiPipeBridge::MainModel mainModel = nullptr;
		std::unique_ptr<TimedOnetimeInvoker> onetimeInvoker;
		void ApplyTopmostWindowStyle(bool v);
		void AdjustWindowSize();
		Windows::Foundation::IAsyncAction ExportDeviceListAsync(bool foroutputs);
	public:
		MainWindow();
		void InitializeComponent();
		MidiPipeBridge::MainModel Model();
		bool Topmost();
		void Topmost(bool value);
		void OnWindowClosed(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::WindowEventArgs& args);
		void OnMidiInExportButtonClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& args);
		void OnMidiOutExportButtonClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& args);
		void OnPipeErrorIndicatorDoubleTapped(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs& args);
		void OnMidiInErrorIndicatorDoubleTapped(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs& args);
		void OnMidiOutErrorIndicatorDoubleTapped(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs& args);
	};
	struct NoWheelComboBox : NoWheelComboBoxT<NoWheelComboBox>
	{
		NoWheelComboBox();
		void OnPointerWheelChanged(const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& args);
	};
}

namespace winrt::MidiPipeBridge::factory_implementation
{
	struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
	{
	};
	struct NoWheelComboBox : NoWheelComboBoxT<NoWheelComboBox, implementation::NoWheelComboBox>
	{
	};
}
