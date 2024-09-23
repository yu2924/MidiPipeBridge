//
//  MainWindow.xaml.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#if __has_include("NoWheelComboBox.g.cpp")
#include "NoWheelComboBox.g.cpp"
#endif
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <shellscalingapi.h>
#include "AppSettings.h"
#include "DebugPrint.h"

#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <shobjidl_core.h>
#pragma comment(lib, "Shcore.lib")

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	// ================================================================================
	// MainWindow
	struct MainWindowHelper
	{
		static HMONITOR GetFocusedDisplay()
		{
			HWND hwndactive = GetForegroundWindow();
			return MonitorFromWindow(hwndactive, MONITOR_DEFAULTTOPRIMARY);
		}
		static Windows::Graphics::RectInt32 GetDisplayRect(HMONITOR hmon)
		{
			MONITORINFO info{ sizeof(info) };
			GetMonitorInfoW(hmon, &info);
			return { info.rcWork.left, info.rcWork.top, info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top };
		}
		static Windows::Graphics::SizeInt32 ScaleSizeWithMonitorDpi(HMONITOR hmon, const Windows::Graphics::SizeInt32& ext)
		{
			UINT dpix = 96, dpiy = 96;
			GetDpiForMonitor(hmon, MONITOR_DPI_TYPE::MDT_EFFECTIVE_DPI, &dpix, &dpiy);
			int32_t cx = ext.Width * dpix / 96;
			int32_t cy = ext.Height * dpiy / 96;
			return { cx, cy };
		}
		static Windows::Graphics::RectInt32 GetCenteredRect(const Windows::Graphics::RectInt32& rc, const Windows::Graphics::SizeInt32& ext)
		{
			int32_t x = rc.X + (rc.Width - ext.Width) / 2;
			int32_t y = rc.Y + (rc.Height - ext.Height) / 2;
			return { x, y, ext.Width, ext.Height };
		}
	};
	// --------------------------------------------------------------------------------
	MainWindow::MainWindow()
	{
	}
	void MainWindow::InitializeComponent()
	{
		Microsoft::UI::Dispatching::DispatcherQueue dispqueue = DispatcherQueue();
		appSettings = winrt::make<AppSettings>(dispqueue);
		mainModel = winrt::make<MainModel>(dispqueue, appSettings);
		MainWindowT::InitializeComponent();
		// set window frame
		Microsoft::UI::Windowing::OverlappedPresenter presenter = Microsoft::UI::Windowing::OverlappedPresenter::Create();
		presenter.IsMaximizable(false);
		presenter.IsResizable(false);
		AppWindow().SetPresenter(presenter);
		// set window icon
		HINSTANCE hinst = GetModuleHandle(nullptr);
		HICON hicon = (HICON)LoadImageW(hinst, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
		::ABI::Microsoft::UI::IconId iconid = Microsoft::UI::GetIconIdFromIcon(hicon);
		AppWindow().SetIcon(iconid);
		// set topmost style
		ApplyTopmostWindowStyle(appSettings.Topmost());
		// set window position
		static const Windows::Graphics::SizeInt32 EstimatedWindowSize{ 358, 346 };
		Windows::Graphics::PointInt32 wndpos{};
		if(appSettings.HasProperty(L"WindowPosition"))
		{
			wndpos = appSettings.WindowPosition();
		}
		else
		{
			HMONITOR hmon = MainWindowHelper::GetFocusedDisplay();
			Windows::Graphics::RectInt32 rcwnd = MainWindowHelper::GetCenteredRect(MainWindowHelper::GetDisplayRect(hmon), MainWindowHelper::ScaleSizeWithMonitorDpi(hmon, EstimatedWindowSize));
			wndpos = { rcwnd.X, rcwnd.Y };
		}
		AppWindow().Move(wndpos);
		// track window position
		onetimeInvoker = std::make_unique<TimedOnetimeInvoker>(DispatcherQueue(), 200);
		onetimeInvoker->OnInvoke = [this]()
		{
			appSettings.WindowPosition(AppWindow().Position());
		};
		AppWindow().Changed([this](const Microsoft::UI::Windowing::AppWindow&, const Microsoft::UI::Windowing::AppWindowChangedEventArgs& args)
		{
			if(args.DidPositionChange()) onetimeInvoker->Trigger();
		});
		// adjust window size
		AppWindow().ResizeClient({ 0, 0 });
		if(Microsoft::UI::Xaml::FrameworkElement panel = Content().try_as<Microsoft::UI::Xaml::FrameworkElement>())
		{
			if(panel.IsLoaded()) AdjustWindowSize();
			else panel.Loaded([this](const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::RoutedEventArgs&) { AdjustWindowSize(); });
		}
		// startup
		mainModel.StartWatcher();
	}
	MidiPipeBridge::MainModel MainWindow::Model()
	{
		return mainModel;
	}
	bool MainWindow::Topmost()
	{
		return appSettings.Topmost();
	}
	void MainWindow::Topmost(bool value)
	{
		if(appSettings.Topmost() == value) return;
		appSettings.Topmost(value);
		ApplyTopmostWindowStyle(appSettings.Topmost());
	}
	void MainWindow::OnWindowClosed(const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::WindowEventArgs&)
	{
		appSettings.SaveIfNeeded();
		mainModel.Shutdown();
	}
	void MainWindow::OnMidiInExportButtonClick(const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::RoutedEventArgs&)
	{
		ExportDeviceListAsync(false);
	}
	void MainWindow::OnMidiOutExportButtonClick(const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::RoutedEventArgs&)
	{
		ExportDeviceListAsync(true);
	}
	void MainWindow::OnPipeErrorIndicatorDoubleTapped(const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs&)
	{
		mainModel.PipeError().Reset();
	}
	void MainWindow::OnMidiInErrorIndicatorDoubleTapped(const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs&)
	{
		mainModel.MidiInError().Reset();
	}
	void MainWindow::OnMidiOutErrorIndicatorDoubleTapped(const Windows::Foundation::IInspectable&, const Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs&)
	{
		mainModel.MidiOutError().Reset();
	}
	// --------------------------------------------------------------------------------
	// internals
	void MainWindow::ApplyTopmostWindowStyle(bool v)
	{
		auto iwnd = m_inner.try_as<::IWindowNative>();
		if(!iwnd) return;
		HWND hwnd{}; iwnd->get_WindowHandle(&hwnd);
		SetWindowPos(hwnd, v ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
	void MainWindow::AdjustWindowSize()
	{
		Windows::Foundation::Size size = Content().DesiredSize();
		AppWindow().ResizeClient({ (int)size.Width, (int)size.Height });
	}
	Windows::Foundation::IAsyncAction MainWindow::ExportDeviceListAsync(bool foroutputs)
	{
		auto iwnd = m_inner.try_as<::IWindowNative>();
		if(!iwnd) co_return;
		HWND hwnd{}; iwnd->get_WindowHandle(&hwnd);
		Windows::Storage::Pickers::FileSavePicker picker;
		picker.as<::IInitializeWithWindow>()->Initialize(hwnd);
		auto choices = picker.FileTypeChoices();
		choices.Insert(L"CSV files", winrt::single_threaded_vector<hstring>({ L".csv", L".tsv" }));
		picker.SuggestedFileName(hstring(L"midi_devices_") + (foroutputs ? L"output" : L"input") + L".csv");
		picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
		Windows::Storage::StorageFile file = co_await picker.PickSaveFileAsync();
		if(!file) co_return;
		hstring del = (_wcsnicmp(PathFindExtensionW(file.Path().c_str()), L".tsv", 4) == 0) ? L"\t" : L",";
		HRESULT hr = S_OK;
		try
		{
			co_await Windows::Storage::FileIO::WriteTextAsync(file, L"Name" + del + L"ParentName" + del + L"DeviceId\n", Windows::Storage::Streams::UnicodeEncoding::Utf8);
			for(const auto& inf : foroutputs ? mainModel.MidiOutDeviceInfoList() : mainModel.MidiInDeviceInfoList())
			{
				if(!MidiDeviceInfo::IsValidId(inf.DeviceId())) continue;
				co_await Windows::Storage::FileIO::AppendTextAsync(file, inf.Name() + del + inf.ParentName() + del + inf.DeviceId() + L"\n", Windows::Storage::Streams::UnicodeEncoding::Utf8);
			}
		}
		catch(const winrt::hresult_error& e)
		{
			hr = e.code();
		}
		if(SUCCEEDED(hr)) co_return;
		Microsoft::UI::Xaml::FrameworkElement panel = Content().try_as<Microsoft::UI::Xaml::FrameworkElement>();
		if(!panel) co_return; // unexpected
		hstring msg = hstring(L"Export as CSV Failed\nfile: \"") + file.Path() + L"\"\n\n" + winrt::hresult_error(hr).message();
		Microsoft::UI::Xaml::Controls::ContentDialog dlg;
		dlg.XamlRoot(panel.XamlRoot());
		dlg.Title(winrt::box_value(L"ERROR"));
		dlg.Content(winrt::box_value(msg));
		dlg.CloseButtonText(L"OK");
		dlg.ShowAsync();
	}
	// ================================================================================
	// NoWheelComboBox
	// This is a ComboBox subclass just to prevent scrolling by the mouse wheel
	NoWheelComboBox::NoWheelComboBox()
	{
	}
	void NoWheelComboBox::OnPointerWheelChanged(const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs& args)
	{
		args.Handled(false);
	}
} // winrt::MidiPipeBridge::implementation
