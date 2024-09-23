//
//  DataTransferBridge.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#include "pch.h"
#include "DataTransferBridge.h"
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Storage.Streams.h>
#include <mutex>
#include "MidiDeviceInfo.h"
#include "DebugPrint.h"

using namespace winrt;

namespace winrt::MidiPipeBridge::implementation
{

	// ================================================================================
	// primitive classes

	struct ManualEvent
	{
		HANDLE hEvent = NULL;
		ManualEvent()
		{
			hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		}
		~ManualEvent()
		{
			if(hEvent) CloseHandle(hEvent);
		}
		bool Reset()
		{
			return hEvent ? ResetEvent(hEvent) : false;
		}
		bool Set()
		{
			return hEvent ? SetEvent(hEvent) : false;
		}
		operator HANDLE()
		{
			return hEvent;
		}
	};

	struct Overlapped : public OVERLAPPED
	{
		Overlapped()
		{
			ZeroMemory(this, sizeof(*this));
			hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		}
		~Overlapped()
		{
			if(hEvent) CloseHandle(hEvent);
		}
		void Reset()
		{
			HANDLE hsave = this->hEvent;
			ZeroMemory(this, sizeof(*this));
			hEvent = hsave;
			if(hEvent) ResetEvent(hEvent);
		}
	};

	class WinThread
	{
	protected:
		static unsigned int WINAPI threadProc(void* param)
		{
			WinThread* pthis = reinterpret_cast<WinThread*>(param);
			HRESULT r = 0;
			try
			{
				r = pthis->Run();
			}
			catch(const winrt::hresult_error& e)
			{
				r = e.code();
				DebugPrint(L"[WinThread ({})] exception: {}\n",  pthis->threadName, e.message());
			}
			catch(...)
			{
				r = E_FAIL;
				DebugPrint(L"[WinThread ({})] exception: unknown\n", pthis->threadName);
			}
			return r;
		}
		std::wstring threadName;
		HANDLE hThread = NULL;
		ManualEvent quitEvent;
		bool quitFlag = false;
	public:
		WinThread(const std::wstring& name) : threadName(name)
		{
		}
		virtual ~WinThread()
		{
			StopThread();
		}
		void StopThread()
		{
			if(!hThread) return;
			RequestToQuitThread();
			WaitForExitThread(INFINITE);
			CloseHandle(hThread);
			hThread = NULL;
		}
		bool WaitForExitThread(DWORD t)
		{
			if(!hThread) return true;
			return WaitForSingleObject(hThread, t) == WAIT_OBJECT_0;
		}
		bool StartThread()
		{
			StopThread();
			quitEvent.Reset();
			quitFlag = false;
			hThread = (HANDLE)_beginthreadex(nullptr, 0, threadProc, this, 0, nullptr);
			return hThread != NULL;
		}
		bool IsThreadRunning() const
		{
			return (hThread != NULL) && (WaitForSingleObject(hThread, 0) == WAIT_TIMEOUT);
		}
		operator HANDLE()
		{
			return hThread;
		}
		virtual void RequestToQuitThread()
		{
			quitFlag = true;
			quitEvent.Set();
		}
		virtual unsigned int Run() = 0;
	};

	// ================================================================================
	// stream transfer classes

	class PipeInMidiOut : private WinThread
	{
	private:
		static bool NeedToReportPipeError(HRESULT r, bool isserver)
		{
			if(SUCCEEDED(r)) return false;
			if(isserver && (HRESULT_CODE(r) == ERROR_BROKEN_PIPE)) return false;
			return true;
		}
		HANDLE hPipe = NULL;
		Overlapped overlapped;
		hstring midiDeviceId;
		Windows::Devices::Midi::IMidiOutPort midiOutPort{ nullptr };
		std::mutex reentrantMutex;
		HRESULT deviceError = S_OK;
		HRESULT pipeError = S_OK;
		bool isServer = false;
		bool ReadPipeOverlapped(uint8_t* p, int c, int* cr)
		{
			overlapped.Reset();
			DWORD cb = 0;
			if(ReadFile(hPipe, p, c, &cb, &overlapped)) { *cr = cb; return true; }
			DWORD r = GetLastError();
			if(r != ERROR_IO_PENDING) { if(!quitFlag) pipeError = HRESULT_FROM_WIN32(r); return false; }
			HANDLE hw[] = { overlapped.hEvent, quitEvent };
			if(WaitForMultipleObjects(_countof(hw), hw, FALSE, INFINITE) != WAIT_OBJECT_0) { if(!quitFlag) pipeError = HRESULT_FROM_WIN32(GetLastError()); return false; }
			if(!GetOverlappedResult(hPipe, &overlapped, &cb, FALSE)) { if(!quitFlag) pipeError = HRESULT_FROM_WIN32(GetLastError()); return false; }
			*cr = cb;
			return true;
		}
		virtual unsigned int Run() override
		{
			DebugPrint(L"[PipeInMidiOut] thread begin\n");
			constexpr uint32_t BUFFERSIZE = 256;
			Windows::Storage::Streams::Buffer buffer(BUFFERSIZE);
			while(1)
			{
				if(quitFlag || FAILED(pipeError) || FAILED(deviceError)) break;
				int cr = 0;
				if(!ReadPipeOverlapped(buffer.data(), buffer.Capacity(), &cr))
				{
					if(NeedToReportPipeError(pipeError, isServer)) { if(OnPipeError) OnPipeError(pipeError); }
					break;
				}
				buffer.Length(cr);
				try
				{
					midiOutPort.SendBuffer(buffer);
				}
				catch(const winrt::hresult_error& e)
				{
					deviceError = e.code();
				}
				if(FAILED(deviceError))
				{
					if(OnDeviceError) OnDeviceError(deviceError);
					break;
				}
			}
			DebugPrint(L"[PipeInMidiOut] thread end\n");
			return 0;
		}
		virtual void RequestToQuitThread() override
		{
			quitFlag = true;
			CancelIoEx(hPipe, &overlapped);
			WinThread::RequestToQuitThread();
		}
		void InternalStart()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			if(IsThreadRunning()) return;
			if(!hPipe || !midiOutPort) return;
			deviceError = S_OK;
			pipeError = S_OK;
			StartThread();
		}
		void InternalStop()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			StopThread();
		}
	public:
		std::function<void(HRESULT)> OnDeviceError;
		std::function<void(HRESULT)> OnPipeError;
		PipeInMidiOut() : WinThread(L"PipeInMidiOut")
		{
		}
		HANDLE GetPipeHandle() const
		{
			return hPipe;
		}
		void SetPipeHandle(HANDLE h, bool server)
		{
			InternalStop();
			pipeError = S_OK;
			hPipe = h;
			isServer = server;
			InternalStart();
		}
		hstring GetMidiDeviceId() const
		{
			return midiDeviceId;
		}
		Windows::Foundation::IAsyncAction SetMidiDeviceIdAsync(const hstring& devid)
		{
			if(midiDeviceId == devid) co_return;
			InternalStop();
			deviceError = S_OK;
			midiDeviceId = devid;
			midiOutPort = MidiDeviceInfo::IsValidId(devid) ? co_await Windows::Devices::Midi::MidiOutPort::FromIdAsync(devid) : nullptr;
			if(!midiDeviceId.empty() && !midiOutPort)
			{
				deviceError = HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
				if(OnDeviceError) OnDeviceError(deviceError);
			}
			InternalStart();
		}
		HRESULT GetDeviceError() const
		{
			return deviceError;
		}
		HRESULT GetPipeError() const
		{
			return pipeError;
		}
		operator HANDLE()
		{
			return hThread;
		}
	};

	class MidiInPipeOut
	{
	private:
		static bool NeedToReportPipeError(HRESULT r, bool isserver)
		{
			if(SUCCEEDED(r)) return false;
			if(isserver && (HRESULT_CODE(r) == ERROR_BROKEN_PIPE)) return false;
			return true;
		}
		HANDLE hPipe = NULL;
		ManualEvent quitEvent;
		ManualEvent stoppedEvent;
		Overlapped overlapped;
		hstring midiDeviceId;
		Windows::Devices::Midi::IMidiInPort midiInPort{ nullptr };
		event_token evtoken{};
		std::mutex reentrantMutex;
		HRESULT deviceError = S_OK;
		HRESULT pipeError = S_OK;
		bool isServer = false;
		bool quitFlag = false;
		bool WritePipeOverlapped(const uint8_t* p, int c, int* cw)
		{
			overlapped.Reset();
			DWORD cb = 0;
			if(WriteFile(hPipe, p, c, &cb, &overlapped)) { *cw = (int)cb; return true; }
			if(GetLastError() != ERROR_IO_PENDING) { if(!quitFlag) pipeError = HRESULT_FROM_WIN32(GetLastError()); return false; }
			HANDLE hw[] = { overlapped.hEvent, quitEvent };
			if(WaitForMultipleObjects(_countof(hw), hw, FALSE, INFINITE) != WAIT_OBJECT_0) { if(!quitFlag) pipeError = HRESULT_FROM_WIN32(GetLastError()); return false; }
			if(!GetOverlappedResult(hPipe, &overlapped, &cb, FALSE)) { if(!quitFlag) pipeError = HRESULT_FROM_WIN32(GetLastError()); return false; }
			*cw = (int)cb;
			return true;
		}
		void OnMidiMessageReceived(Windows::Devices::Midi::MidiInPort const&, Windows::Devices::Midi::MidiMessageReceivedEventArgs const& args)
		{
			if(FAILED(pipeError)) return;
			Windows::Storage::Streams::IBuffer buffer = args.Message().RawData();
			int cw = 0;
			if(!WritePipeOverlapped(buffer.data(), buffer.Length(), &cw))
			{
				if(NeedToReportPipeError(pipeError, isServer)) { if(OnPipeError) OnPipeError(pipeError); }
			}
		}
		void InternalStart()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			if((bool)evtoken) return;
			if(!hPipe || !midiInPort) return;
			deviceError = S_OK;
			pipeError = S_OK;
			quitFlag = false;
			quitEvent.Reset();
			stoppedEvent.Reset();
			evtoken = midiInPort.MessageReceived([this](Windows::Devices::Midi::MidiInPort const& mp, Windows::Devices::Midi::MidiMessageReceivedEventArgs const& args) { OnMidiMessageReceived(mp, args); });
		}
		void InternalStop()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			quitFlag = true;
			if(hPipe) CancelIoEx(hPipe, &overlapped);
			quitEvent.Set();
			if(evtoken) midiInPort.MessageReceived(evtoken);
			evtoken = {};
			stoppedEvent.Set();
		}
	public:
		std::function<void(HRESULT)> OnDeviceError;
		std::function<void(HRESULT)> OnPipeError;
		MidiInPipeOut()
		{
		}
		~MidiInPipeOut()
		{
			InternalStop();
		}
		HANDLE GetPipeHandle() const
		{
			return hPipe;
		}
		void SetPipeHandle(HANDLE h, bool server)
		{
			InternalStop();
			pipeError = S_OK;
			hPipe = h;
			isServer = server;
			InternalStart();
		}
		hstring GetMidiDeviceId() const
		{
			return midiDeviceId;
		}
		Windows::Foundation::IAsyncAction SetMidiDeviceIdAsync(const hstring& devid)
		{
			if(midiDeviceId == devid) co_return;
			InternalStop();
			deviceError = S_OK;
			midiDeviceId = devid;
			midiInPort = MidiDeviceInfo::IsValidId(devid) ? co_await Windows::Devices::Midi::MidiInPort::FromIdAsync(devid) : nullptr;
			if(!midiDeviceId.empty() && !midiInPort)
			{
				deviceError = HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
				if(OnDeviceError) OnDeviceError(deviceError);
			}
			InternalStart();
		}
		HRESULT GetDeviceError() const
		{
			return deviceError;
		}
		HRESULT GetPipeError() const
		{
			return pipeError;
		}
		operator HANDLE()
		{
			return stoppedEvent;
		}
	};

	// ================================================================================
	// pipe connection session classes

	struct IPipeSession
	{
		std::function<void(HRESULT)> OnSessionError;
		virtual ~IPipeSession() {}
		virtual bool StartSession() = 0;
		virtual void StopSession() = 0;
		virtual bool IsSessionRunning() const = 0;
		virtual HRESULT GetSessionError() const = 0;
	};

	class PipeServer : public IPipeSession, private WinThread
	{
	private:
		std::wstring pipeName;
		PipeInMidiOut& pipeInMidiOut;
		MidiInPipeOut& midiInPipeOut;
		HANDLE hPipe = NULL;
		Overlapped overlapped;
		HRESULT sessionError = S_OK;
		bool ConnectPipeOverlapped()
		{
			overlapped.Reset();
			BOOL rconnect = ConnectNamedPipe(hPipe, &overlapped);
			DWORD r = GetLastError();
			if(rconnect) { if(!quitFlag) sessionError = HRESULT_FROM_WIN32(r); return false; } // overlapped ConnectNamedPipe() should return FALSE
			if(r == ERROR_PIPE_CONNECTED) return true;
			if((r != ERROR_IO_PENDING) && (r != ERROR_PIPE_LISTENING)) { if(!quitFlag) sessionError = HRESULT_FROM_WIN32(r); return false; }
			HANDLE hw[] = { overlapped.hEvent, quitEvent };
			if(WaitForMultipleObjects(_countof(hw), hw, FALSE, INFINITE) != WAIT_OBJECT_0) { if(!quitFlag) sessionError = HRESULT_FROM_WIN32(GetLastError()); return false; }
			return true;
		}
		virtual unsigned int Run() override
		{
			DebugPrint(L"[PipeServer] thread begin\n");
			while(1)
			{
				if(quitFlag) break;
				if(!ConnectPipeOverlapped())
				{
					DebugPrint(L"[PipeServer] failed ConnectNamedPipe()\n");
					if(FAILED(sessionError)) { if(OnSessionError) OnSessionError(sessionError); }
					break;
				}
				DebugPrint(L"[PipeServer] connected\n");
				pipeInMidiOut.SetPipeHandle(hPipe, true);
				midiInPipeOut.SetPipeHandle(hPipe, true);
				while(1)
				{
					HANDLE hw[] = { pipeInMidiOut, midiInPipeOut, quitEvent };
					WaitForMultipleObjects(_countof(hw), hw, FALSE, INFINITE);
					if( FAILED(pipeInMidiOut.GetDeviceError()) ||
						FAILED(pipeInMidiOut.GetPipeError()) ||
						FAILED(midiInPipeOut.GetDeviceError()) ||
						FAILED(midiInPipeOut.GetPipeError()) ||
						quitFlag) break;
				}
				// FlushFileBuffers(hPipe); // unnecessary?
				DisconnectNamedPipe(hPipe);
				pipeInMidiOut.SetPipeHandle(NULL, false);
				midiInPipeOut.SetPipeHandle(NULL, false);
				DebugPrint(L"[PipeServer] disconnected\n");
			}
			DebugPrint(L"[PipeServer] thread end\n");
			return 0;
		}
		virtual void RequestToQuitThread() override
		{
			quitFlag = true;
			CancelIoEx(hPipe, &overlapped);
			WinThread::RequestToQuitThread();
		}
	public:
		PipeServer(const std::wstring& pipename, PipeInMidiOut& p2m, MidiInPipeOut& m2p)
			: WinThread(L"PipeServer")
			, pipeName(pipename)
			, pipeInMidiOut(p2m)
			, midiInPipeOut(m2p)
		{
		}
		virtual ~PipeServer() override
		{
			StopSession();
		}
		virtual bool StartSession() override
		{
			StopSession();
			sessionError = S_OK;
			constexpr DWORD BUFFERSIZE = 1024;
			hPipe = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, BUFFERSIZE, BUFFERSIZE, 0, nullptr);
			if(hPipe == INVALID_HANDLE_VALUE)
			{
				hPipe = NULL;
				sessionError = HRESULT_FROM_WIN32(GetLastError());
				if(OnSessionError) OnSessionError(sessionError);
				DebugPrint(L"[PipeServer] failed CreateNamedPipe()\n");
				return false;
			}
			return StartThread();
		}
		virtual void StopSession() override
		{
			StopThread();
			if(hPipe) CloseHandle(hPipe);
			hPipe = NULL;
		}
		virtual bool IsSessionRunning() const override
		{
			return IsThreadRunning();
		}
		virtual HRESULT GetSessionError() const override
		{
			return sessionError;
		}
	};

	class PipeClient : public IPipeSession
	{
	private:
		std::wstring pipeName;
		PipeInMidiOut& pipeInMidiOut;
		MidiInPipeOut& midiInPipeOut;
		HANDLE hPipe = NULL;
		HRESULT sessionError = S_OK;
	public:
		PipeClient(const std::wstring& pipename, PipeInMidiOut& p2m, MidiInPipeOut& m2p)
			: pipeName(pipename)
			, pipeInMidiOut(p2m)
			, midiInPipeOut(m2p)
		{
		}
		virtual ~PipeClient() override
		{
			StopSession();
		}
		virtual bool StartSession() override
		{
			StopSession();
			sessionError = S_OK;
			hPipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
			if(hPipe == INVALID_HANDLE_VALUE)
			{
				hPipe = NULL;
				sessionError = HRESULT_FROM_WIN32(GetLastError());
				if(OnSessionError) OnSessionError(sessionError);
				DebugPrint(L"[PipeClient] failed CreateFile()\n");
				return false;
			}
			DWORD mode = PIPE_READMODE_BYTE;
			SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);
			pipeInMidiOut.SetPipeHandle(hPipe, false);
			midiInPipeOut.SetPipeHandle(hPipe, false);
			return true;
		}
		virtual void StopSession() override
		{
			pipeInMidiOut.SetPipeHandle(NULL, false);
			midiInPipeOut.SetPipeHandle(NULL, false);
			if(hPipe) CloseHandle(hPipe);
			hPipe = NULL;
		}
		virtual bool IsSessionRunning() const override
		{
			return hPipe != NULL;
		}
		virtual HRESULT GetSessionError() const override
		{
			return sessionError;
		}
	};

	// ================================================================================
	// the DataTransferBridge

	class DataTransferBridge::Impl
	{
	public:
		DataTransferBridge* outer;
		Microsoft::UI::Dispatching::DispatcherQueue dispatchQueue;
		PipeInMidiOut pipeInMidiOut;
		MidiInPipeOut midiInPipeOut;
		std::wstring pipeName;
		bool runAsServer = false;
		std::unique_ptr<IPipeSession> pipeSession;
		Impl(DataTransferBridge* p, Microsoft::UI::Dispatching::DispatcherQueue dispqueue) : outer(p), dispatchQueue(dispqueue)
		{
			pipeInMidiOut.OnDeviceError = [this](HRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnMidiOutError) outer->OnMidiOutError(r); }); };
			pipeInMidiOut.OnPipeError = [this](HRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnPipeError) outer->OnPipeError(r); }); };
			midiInPipeOut.OnDeviceError = [this](HRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnMidiInError) outer->OnMidiInError(r); }); };
			midiInPipeOut.OnPipeError = [this](HRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnPipeError) outer->OnPipeError(r); }); };
		}
		~Impl()
		{
			// prevent async callback
			pipeInMidiOut.OnDeviceError = nullptr;
			pipeInMidiOut.OnPipeError = nullptr;
			midiInPipeOut.OnDeviceError = nullptr;
			midiInPipeOut.OnPipeError = nullptr;
			outer->OnPipeError = nullptr;
			outer->OnMidiInError = nullptr;
			outer->OnMidiOutError = nullptr;
			StopSession();
		}
		// --------------------------------------------------------------------------------
		// public APIs
		hstring GetMidiInDeviceId() const
		{
			return midiInPipeOut.GetMidiDeviceId();
		}
		void SetMidiInDeviceId(const hstring& v)
		{
			midiInPipeOut.SetMidiDeviceIdAsync(v);
		}
		hstring GetMidiOutDeviceId() const
		{
			return pipeInMidiOut.GetMidiDeviceId();
		}
		void SetMidiOutDeviceId(const hstring& v)
		{
			pipeInMidiOut.SetMidiDeviceIdAsync(v);
		}
		bool IsRunning() const
		{
			return pipeSession ? pipeSession->IsSessionRunning() : false;
		}
		bool StartSession(const std::wstring& pipename, bool runasserver)
		{
			StopSession();
			pipeName = pipename;
			runAsServer = runasserver;
			if(runAsServer)	pipeSession = std::make_unique<PipeServer>(pipeName, pipeInMidiOut, midiInPipeOut);
			else			pipeSession = std::make_unique<PipeClient>(pipeName, pipeInMidiOut, midiInPipeOut);
			pipeSession->OnSessionError = [this](HRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnPipeError) outer->OnPipeError(r); }); };
			return pipeSession->StartSession();
		}
		void StopSession()
		{
			pipeSession.reset();
		}
		bool IsSessionRunning() const
		{
			return pipeSession ? pipeSession->IsSessionRunning() : false;
		}
	};

	DataTransferBridge::DataTransferBridge(Microsoft::UI::Dispatching::DispatcherQueue dispqueue) { impl = std::make_unique<Impl>(this, dispqueue); }
	DataTransferBridge::~DataTransferBridge() { impl.reset(); }
	hstring DataTransferBridge::GetMidiInDeviceId() const { return impl->GetMidiInDeviceId(); }
	void DataTransferBridge::SetMidiInDeviceId(const hstring& v) { impl->SetMidiInDeviceId(v); }
	hstring DataTransferBridge::GetMidiOutDeviceId() const { return impl->GetMidiOutDeviceId(); }
	void DataTransferBridge::SetMidiOutDeviceId(const hstring& v) { impl->SetMidiOutDeviceId(v); }
	bool DataTransferBridge::StartSession(const std::wstring& pipename, bool runasserver) { return impl->StartSession(pipename, runasserver); }
	void DataTransferBridge::StopSession() { impl->StopSession(); }
	bool DataTransferBridge::IsSessionRunning() const { return impl->IsSessionRunning(); }

} // namespace winrt::MidiPipeBridge::implementation
