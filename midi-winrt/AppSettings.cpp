//
//  AppSettings.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#include "pch.h"
#include "AppSettings.h"
#if __has_include("AppSettings.g.cpp")
#include "AppSettings.g.cpp"
#endif
#include <winrt/Windows.Data.Json.h>
#include <shlobj_core.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Version.lib")
#include <strsafe.h>
#include <sstream>
#include <fstream>
#include <regex>
#include "OnetimeInvoker.h"
#include "DebugPrint.h"

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{
	class AppSettings::Impl
	{
	public:
		// 
		// NOTE:
		// Non-packaged apps do not have a package ID and cannot use the Windows::Storage::ApplicationData class,
		// so use the Win32 native version resource as a hint to determine own data file path.
		// 
		static std::wstring FilterInvalidFileSystemChars(const std::wstring& src)
		{
			static std::wregex re(LR"(["<>|:*?\\/])");
			return std::regex_replace(src, re, L"_");
		}
		static std::vector<uint8_t> GetFileVersionInfoData()
		{
			WCHAR modulepath[MAX_PATH]; GetModuleFileNameW(NULL, modulepath, _countof(modulepath));
			DWORD cb = GetFileVersionInfoSizeW(modulepath, nullptr);
			std::vector<uint8_t> verinfo(cb);
			GetFileVersionInfoW(modulepath, 0, (DWORD)verinfo.size(), verinfo.data());
			return verinfo;
		}
		struct LCPAIR
		{
			WORD language; // loword
			WORD codepage; // hiword
		};
		static std::vector<LCPAIR> GetFileVersionTranslationArray(const std::vector<uint8_t>& verinfo)
		{
			LCPAIR* p = nullptr; UINT cb = 0;
			if(!VerQueryValueW(verinfo.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&p, &cb)) return {};
			return std::vector<LCPAIR>(p, p + (cb / sizeof(LCPAIR)));
		}
		static std::wstring GetFileVersionStringValue(const std::vector<uint8_t>& verinfo, const std::vector<LCPAIR>& lclist, LPCWSTR stringname)
		{
			for(const auto& lc : lclist)
			{
				WCHAR key[64]{};
				StringCchPrintfW(key, _countof(key), L"\\StringFileInfo\\%04x%04x\\", lc.language, lc.codepage);
				StringCchCatW(key, _countof(key), stringname);
				LPCWSTR pv = nullptr; UINT cv = 0;
				if(!VerQueryValueW(verinfo.data(), key, (LPVOID*)&pv, &cv)) continue;
				if(!pv || !cv) return {};
				// the obtained length may contain a terminating zero, so let the wstring constructor determine the true length
				return std::wstring(std::wstring(pv, cv).c_str());
			}
			return {};
		}
		static std::wstring GetUserAppDataFolderPath()
		{
			// return "C:\Users\<username>\AppData\Roaming\<companyname>\<productname>"
			PWSTR dir = nullptr;
			SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &dir);
			std::wstring appdatadir = dir;
			CoTaskMemFree(dir);
			std::vector<uint8_t> verinfo = GetFileVersionInfoData();
			std::vector<LCPAIR> lclist = GetFileVersionTranslationArray(verinfo);
			std::wstring companyname = FilterInvalidFileSystemChars(GetFileVersionStringValue(verinfo, lclist, L"CompanyName"));
			std::wstring productname = FilterInvalidFileSystemChars(GetFileVersionStringValue(verinfo, lclist, L"ProductName"));
			if(companyname.empty()) companyname = L"__MyCompanyName__";
			if(productname.empty()) productname = L"__MyProductName__";
			DebugPrint(L"[AppSettings] CompanyName={}\n", companyname);
			DebugPrint(L"[AppSettings] ProductName={}\n", productname);
			return appdatadir + L"\\" + companyname + L"\\" + productname;
		}
		static hstring PointInt32ToString(const Windows::Graphics::PointInt32& pt)
		{
			return hstring(std::to_wstring(pt.X) + L"," + std::to_wstring(pt.Y));
		}
		static Windows::Graphics::PointInt32 StringToPointInt32(const hstring& s)
		{
			std::wistringstream str((std::wstring)s);
			Windows::Graphics::PointInt32 pt; wchar_t del;
			str >> pt.X >> del >> pt.Y;
			return pt;
		}
		// --------------------------------------------------------------------------------
		AppSettings* outer;
		std::unique_ptr<OnetimeInvoker> onetimeInvoker;
		std::wstring jsonPath;
		Windows::Data::Json::JsonObject jsonObject;
		bool isDirty = false;
		Impl(AppSettings* p, Microsoft::UI::Dispatching::DispatcherQueue dispqueue) : outer(p)
		{
			onetimeInvoker = std::make_unique<OnetimeInvoker>(dispqueue);
			onetimeInvoker->OnInvoke = [this]() { SaveIfNeeded(); };
			std::wstring dir = GetUserAppDataFolderPath();
			CreateDirectoryW(dir.c_str(), NULL);
			jsonPath = dir + L"\\settings.json";
			DebugPrint(L"[AppSettings] settings path={}\n", jsonPath);
			std::fstream istr(jsonPath, std::ios_base::in);
			std::string sjson;
			std::getline(istr, sjson, '\0');
			if(!Windows::Data::Json::JsonObject::TryParse(winrt::to_hstring(sjson), jsonObject))
				DebugPrint(L"[AppSettings] JsonObject::TryParse() failed\n");
		}
		~Impl()
		{
			SaveIfNeeded();
		}
		// --------------------------------------------------------------------------------
		// public APIs
		void SaveIfNeeded()
		{
			if(!isDirty) return;
			std::fstream ostr(jsonPath, std::ios_base::out | std::ios_base::trunc);
			ostr << winrt::to_string(jsonObject.Stringify());
			isDirty = false;
		}
		bool HasProperty(hstring propname)
		{
			return jsonObject.HasKey(propname);
		}
		bool Topmost()
		{
			return jsonObject.GetNamedBoolean(L"Topmost", false);
		}
		void Topmost(bool value)
		{
			if(Topmost() == value) return;
			jsonObject.Insert(L"Topmost", Windows::Data::Json::JsonValue::CreateBooleanValue(value));
			isDirty = true;
			onetimeInvoker->Trigger();
		}
		Windows::Graphics::PointInt32 WindowPosition()
		{
			return StringToPointInt32(jsonObject.GetNamedString(L"WindowPosition", L""));
		}
		void WindowPosition(const Windows::Graphics::PointInt32& value)
		{
			if(WindowPosition() == value) return;
			jsonObject.Insert(L"WindowPosition", Windows::Data::Json::JsonValue::CreateStringValue(PointInt32ToString(value)));
			isDirty = true;
			onetimeInvoker->Trigger();
		}
		hstring PipeName()
		{
			return jsonObject.GetNamedString(L"PipeName", L"");
		}
		void PipeName(const hstring& value)
		{
			if(PipeName() == value) return;
			jsonObject.Insert(L"PipeName", Windows::Data::Json::JsonValue::CreateStringValue(value));
			isDirty = true;
			onetimeInvoker->Trigger();
		}
		bool RunAsServer()
		{
			return jsonObject.GetNamedBoolean(L"RunAsServer", false);
		}
		void RunAsServer(bool value)
		{
			if(RunAsServer() == value) return;
			jsonObject.Insert(L"RunAsServer", Windows::Data::Json::JsonValue::CreateBooleanValue(value));
			isDirty = true;
			onetimeInvoker->Trigger();
		}
		hstring MidiInDeviceId()
		{
			return jsonObject.GetNamedString(L"MidiInDeviceId", L"");
		}
		void MidiInDeviceId(const hstring& value)
		{
			if(MidiInDeviceId() == value) return;
			jsonObject.Insert(L"MidiInDeviceId", Windows::Data::Json::JsonValue::CreateStringValue(value));
			isDirty = true;
			onetimeInvoker->Trigger();
		}
		hstring MidiOutDeviceId()
		{
			return jsonObject.GetNamedString(L"MidiOutDeviceId", L"");
		}
		void MidiOutDeviceId(const hstring& value)
		{
			if(MidiOutDeviceId() == value) return;
			jsonObject.Insert(L"MidiOutDeviceId", Windows::Data::Json::JsonValue::CreateStringValue(value));
			isDirty = true;
			onetimeInvoker->Trigger();
		}
	};
	AppSettings::AppSettings(Microsoft::UI::Dispatching::DispatcherQueue dispqueue) { impl = std::make_unique<Impl>(this, dispqueue); }
	AppSettings::~AppSettings() { impl.reset(); }
	void AppSettings::SaveIfNeeded() { impl->SaveIfNeeded(); }
	bool AppSettings::HasProperty(hstring propname) { return impl->HasProperty(propname); }
	bool AppSettings::Topmost() { return impl->Topmost(); }
	void AppSettings::Topmost(bool value) { impl->Topmost(value); }
	Windows::Graphics::PointInt32 AppSettings::WindowPosition() { return impl->WindowPosition(); }
	void AppSettings::WindowPosition(const Windows::Graphics::PointInt32& value) { impl->WindowPosition(value); }
	hstring AppSettings::PipeName() { return impl->PipeName(); }
	void AppSettings::PipeName(const hstring& value) { impl->PipeName(value); }
	bool AppSettings::RunAsServer() { return impl->RunAsServer(); }
	void AppSettings::RunAsServer(bool value) { impl->RunAsServer(value); }
	hstring AppSettings::MidiInDeviceId() { return impl->MidiInDeviceId(); }
	void AppSettings::MidiInDeviceId(const hstring& value) { impl->MidiInDeviceId(value); }
	hstring AppSettings::MidiOutDeviceId() { return impl->MidiOutDeviceId(); }
	void AppSettings::MidiOutDeviceId(const hstring& value) { impl->MidiOutDeviceId(value); }
} // winrt::MidiPipeBridge::implementation
