//
//  DataTransferBridge.cpp
//  MidiPipeBridge
//
//  created by yu2924 on 2024-09-09
//

#include "pch.h"
#include "DataTransferBridge.h"
#include <mmeapi.h>
#pragma comment(lib, "Winmm.lib")
#include <mutex>
#include "MidiDeviceInfo.h"
#include "DebugPrint.h"

#undef min
#undef max

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
	// MME midiport wrappers

	static constexpr int NumMidiBuffers = 16;
	static constexpr int MidiBufferSize = 256;

	static inline bool MMResultIsError(MMRESULT r)
	{
		return r != MMSYSERR_NOERROR;
	}

	struct MIDIHDREX : public MIDIHDR
	{
		char exbuffer[MidiBufferSize];
		void Initialize()
		{
			ZeroMemory(this, sizeof(*this));
			lpData = exbuffer;
			dwBufferLength = sizeof(exbuffer);
		}
	};

	class MidiOutPort
	{
	private:
		HMIDIOUT hMidiOut = NULL;
		std::vector<std::unique_ptr<MIDIHDREX> > hdrList;
		std::list<MIDIHDREX*> freeList;
		std::recursive_mutex lock;
		static void CALLBACK MidiOutProc(HMIDIOUT hmo, UINT msg, DWORD_PTR inst, DWORD_PTR param1, DWORD_PTR param2)
		{
			reinterpret_cast<MidiOutPort*>(inst)->OnMidiOutCallback(hmo, msg, param1, param2);
		}
		void OnMidiOutCallback(HMIDIOUT, UINT msg, DWORD_PTR param1, DWORD_PTR)
		{
			if(msg == MOM_DONE)
			{
				std::lock_guard<std::recursive_mutex> al(lock);
				MIDIHDREX* hdr = reinterpret_cast<MIDIHDREX*>(param1);
				hdr->dwFlags &= MHDR_PREPARED;
				freeList.push_back(hdr);
			}
		}
	public:
		MidiOutPort()
		{
		}
		~MidiOutPort()
		{
			CloseDevice();
		}
		bool IsDeviceOpen() const
		{
			return hMidiOut != NULL;
		}
		void CloseDevice()
		{
			if(!hMidiOut) return;
			midiOutReset(hMidiOut);
			for(auto&& hdr : hdrList)
			{
				midiOutUnprepareHeader(hMidiOut, hdr.get(), sizeof(MIDIHDR));
			}
			hdrList.clear();
			freeList.clear();
			midiOutClose(hMidiOut);
			hMidiOut = NULL;
		}
		MMRESULT OpenDevice(uint32_t devid)
		{
			CloseDevice();
			int r = MMSYSERR_NOERROR;
			try
			{
				r = midiOutOpen(&hMidiOut, devid, (DWORD_PTR)MidiOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
				if(MMResultIsError(r)) throw r;
				for(int c = NumMidiBuffers, i = 0; i < c; ++i)
				{
					std::unique_ptr<MIDIHDREX> hdr = std::make_unique<MIDIHDREX>();
					hdr->Initialize();
					r = midiOutPrepareHeader(hMidiOut, hdr.get(), sizeof(MIDIHDR));
					if(MMResultIsError(r)) throw r;
					freeList.push_back(hdr.get());
					hdrList.push_back(std::move(hdr));
				}
			}
			catch(...)
			{
				DebugPrint(L"[MidiOutPort] OpenDevice() failed\n");
			}
			if(MMResultIsError(r))
			{
				CloseDevice();
			}
			return r;
		}
		int GetBufferSize() const
		{
			return MidiBufferSize;
		}
		MMRESULT Send(const uint8_t* p, int c)
		{
			if(!hMidiOut) return MMSYSERR_INVALHANDLE;
			int i = 0; while(i < c)
			{
				MIDIHDREX* hdr = nullptr;
				{
					std::lock_guard<std::recursive_mutex> al(lock);
					if(freeList.empty()) return MMSYSERR_ERROR;
					hdr = freeList.front();
					freeList.pop_front();
				}
				int lseg = std::min((int)sizeof(hdr->exbuffer), c - i);
				memcpy(hdr->lpData, p + i, lseg);
				hdr->dwBufferLength = hdr->dwBytesRecorded = c - i;
				MMRESULT r = midiOutLongMsg(hMidiOut, hdr, sizeof(MIDIHDR));
				if(MMResultIsError(r)) return r;
				i += lseg;
			}
			return MMSYSERR_NOERROR;
		}
		MMRESULT Send(const std::vector<uint8_t>& v)
		{
			return Send(v.data(), (int)v.size());
		}
	};

	class MidiInPort
	{
	private:
		static int GuessShortMessageLength(uint8_t stat)
		{
			switch(stat & 0xf0)
			{
				case 0x80: // noteoff
				case 0x90: // noteon
				case 0xa0: // poly.aftertouch
				case 0xb0: // control
				case 0xe0: return 3; // pichbend
				case 0xc0: // program
				case 0xd0: return 2; // aftertouch
			}
			switch(stat)
			{
				case 0xf2: return 3; // SPP
				case 0xf3: return 2; // SS
			}
			return 1;
		}
		HMIDIIN hMidiIn = NULL;
		std::vector<std::unique_ptr<MIDIHDREX> > hdrList;
		bool quitFlag = false;
		static void CALLBACK MidiInProc(HMIDIIN hmi, UINT msg, DWORD_PTR inst, DWORD_PTR param1, DWORD_PTR param2)
		{
			reinterpret_cast<MidiInPort*>(inst)->OnMidiInCallback(hmi, msg, param1, param2);
		}
		void OnMidiInCallback(HMIDIIN hmi, UINT msg, DWORD_PTR param1, DWORD_PTR)
		{
			switch(msg)
			{
				case MIM_DATA:
				{
					const uint8_t* p = reinterpret_cast<const uint8_t*>(&param1);
					int c = GuessShortMessageLength(p[0]);
					if(OnMidiInReceived) OnMidiInReceived(p, c);
					break;
				}
				case MIM_LONGDATA:
				{
					MIDIHDREX* hdr = reinterpret_cast<MIDIHDREX*>(param1);
					if(OnMidiInReceived) OnMidiInReceived((const uint8_t*)hdr->lpData, hdr->dwBytesRecorded);
					if(!quitFlag) midiInAddBuffer(hmi, hdr, sizeof(MIDIHDR));
					break;
				}
			}
		}
	public:
		std::function<void(const uint8_t* p, int c)> OnMidiInReceived;
		MidiInPort()
		{
		}
		~MidiInPort()
		{
			CloseDevice();
		}
		bool IsDeviceOpen() const
		{
			return hMidiIn != NULL;
		}
		void CloseDevice()
		{
			if(!hMidiIn) return;
			quitFlag = true;
			midiInStop(hMidiIn);
			midiInReset(hMidiIn);
			for(auto&& hdr : hdrList)
			{
				midiInUnprepareHeader(hMidiIn, hdr.get(), sizeof(MIDIHDR));
			}
			hdrList.clear();
			midiInClose(hMidiIn);
			hMidiIn = NULL;
			quitFlag = false;
		}
		MMRESULT OpenDevice(uint32_t devid)
		{
			CloseDevice();
			int r = MMSYSERR_NOERROR;
			try
			{
				r = midiInOpen(&hMidiIn, devid, (DWORD_PTR)MidiInProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
				if(MMResultIsError(r)) throw r;
				for(int c = NumMidiBuffers, i = 0; i < c; ++i)
				{
					std::unique_ptr<MIDIHDREX> hdr = std::make_unique<MIDIHDREX>();
					hdr->Initialize();
					r = midiInPrepareHeader(hMidiIn, hdr.get(), sizeof(MIDIHDR));
					if(MMResultIsError(r)) throw r;
					hdrList.push_back(std::move(hdr));
					r = midiInAddBuffer(hMidiIn, hdrList.back().get(), sizeof(MIDIHDR));
					if(MMResultIsError(r)) throw r;
				}
			}
			catch(...)
			{
				DebugPrint(L"[MidiInPort] OpenDevice() failed\n");
			}
			if(MMResultIsError(r))
			{
				CloseDevice();
			}
			return r;
		}
		int GetBufferSize() const
		{
			return MidiBufferSize;
		}
		MMRESULT StopDevice()
		{
			if(!hMidiIn) return MMSYSERR_INVALHANDLE;
			return midiInStop(hMidiIn);
		}
		MMRESULT StartDevice()
		{
			if(!hMidiIn) return MMSYSERR_INVALHANDLE;
			return midiInStart(hMidiIn);
		}
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
		uint32_t midiDeviceId = MidiDeviceInfo::NoneMidiDeviceInfo().DeviceId();
		MidiOutPort midiOutPort;
		std::mutex reentrantMutex;
		MMRESULT deviceError = MMSYSERR_NOERROR;
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
			std::vector<uint8_t> buffer(midiOutPort.GetBufferSize());
			while(1)
			{
				if(quitFlag || FAILED(pipeError) || MMResultIsError(deviceError)) break;
				int cr = 0;
				buffer.resize(buffer.capacity());
				if(!ReadPipeOverlapped(buffer.data(), (int)buffer.capacity(), &cr))
				{
					if(NeedToReportPipeError(pipeError, isServer)) { if(OnPipeError) OnPipeError(pipeError); }
					break;
				}
				buffer.resize(cr);
				deviceError = midiOutPort.Send(buffer);
				if(MMResultIsError(deviceError))
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
			if(!hPipe || !midiOutPort.IsDeviceOpen()) return;
			deviceError = MMSYSERR_NOERROR;
			pipeError = S_OK;
			StartThread();
		}
		void InternalStop()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			StopThread();
		}
	public:
		std::function<void(MMRESULT)> OnDeviceError;
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
		uint32_t GetMidiDeviceId() const
		{
			return midiDeviceId;
		}
		void SetMidiDeviceId(uint32_t devid)
		{
			if(midiDeviceId == devid) return;
			InternalStop();
			deviceError = MMSYSERR_NOERROR;
			midiDeviceId = devid;
			if(MidiDeviceInfo::IsValidDeviceId(midiDeviceId, true))
			{
				deviceError = midiOutPort.OpenDevice(midiDeviceId);
				if(MMResultIsError(deviceError))
				{
					if(OnDeviceError) OnDeviceError(deviceError);
				}
			}
			InternalStart();
		}
		MMRESULT GetDeviceError() const
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
		uint32_t midiDeviceId = MidiDeviceInfo::NoneMidiDeviceInfo().DeviceId();
		MidiInPort midiInPort;
		std::mutex reentrantMutex;
		MMRESULT deviceError = MMSYSERR_NOERROR;
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
		void OnMidiMessageReceived(const uint8_t* p, int c)
		{
			if(FAILED(pipeError)) return;
			int cw = 0;
			if(!WritePipeOverlapped(p, c, &cw))
			{
				if(NeedToReportPipeError(pipeError, isServer)) { if(OnPipeError) OnPipeError(pipeError); }
			}
		}
		void InternalStart()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			if(!hPipe || !midiInPort.IsDeviceOpen()) return;
			deviceError = MMSYSERR_NOERROR;
			pipeError = S_OK;
			quitFlag = false;
			quitEvent.Reset();
			stoppedEvent.Reset();
			deviceError = midiInPort.StartDevice();
			if(MMResultIsError(deviceError))
			{
				quitFlag = true;
				quitEvent.Set();
				stoppedEvent.Set();
				if(OnDeviceError) OnDeviceError(deviceError);
			}
		}
		void InternalStop()
		{
			std::lock_guard<std::mutex> lock(reentrantMutex);
			quitFlag = true;
			if(hPipe) CancelIoEx(hPipe, &overlapped);
			quitEvent.Set();
			midiInPort.StopDevice();
			stoppedEvent.Set();
		}
	public:
		std::function<void(MMRESULT)> OnDeviceError;
		std::function<void(HRESULT)> OnPipeError;
		MidiInPipeOut()
		{
			midiInPort.OnMidiInReceived = [this](const uint8_t* p, int c) { OnMidiMessageReceived(p, c); };
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
		uint32_t GetMidiDeviceId() const
		{
			return midiDeviceId;
		}
		void SetMidiDeviceId(uint32_t devid)
		{
			if(midiDeviceId == devid) return;
			InternalStop();
			deviceError = MMSYSERR_NOERROR;
			midiDeviceId = devid;
			if(MidiDeviceInfo::IsValidDeviceId(midiDeviceId, false))
			{
				deviceError = midiInPort.OpenDevice(midiDeviceId);
				if(MMResultIsError(deviceError))
				{
					if(OnDeviceError) OnDeviceError(deviceError);
				}
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
			pipeInMidiOut.OnDeviceError = [this](MMRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnMidiOutError) outer->OnMidiOutError(r); }); };
			pipeInMidiOut.OnPipeError = [this](HRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnPipeError) outer->OnPipeError(r); }); };
			midiInPipeOut.OnDeviceError = [this](MMRESULT r) { dispatchQueue.TryEnqueue([this, r]() { if(outer->OnMidiInError) outer->OnMidiInError(r); }); };
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
		uint32_t GetMidiInDeviceId() const
		{
			return midiInPipeOut.GetMidiDeviceId();
		}
		void SetMidiInDeviceId(uint32_t v)
		{
			midiInPipeOut.SetMidiDeviceId(v);
		}
		uint32_t GetMidiOutDeviceId() const
		{
			return pipeInMidiOut.GetMidiDeviceId();
		}
		void SetMidiOutDeviceId(uint32_t v)
		{
			pipeInMidiOut.SetMidiDeviceId(v);
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
	uint32_t DataTransferBridge::GetMidiInDeviceId() const { return impl->GetMidiInDeviceId(); }
	void DataTransferBridge::SetMidiInDeviceId(uint32_t v) { impl->SetMidiInDeviceId(v); }
	uint32_t DataTransferBridge::GetMidiOutDeviceId() const { return impl->GetMidiOutDeviceId(); }
	void DataTransferBridge::SetMidiOutDeviceId(uint32_t v) { impl->SetMidiOutDeviceId(v); }
	bool DataTransferBridge::StartSession(const std::wstring& pipename, bool runasserver) { return impl->StartSession(pipename, runasserver); }
	void DataTransferBridge::StopSession() { impl->StopSession(); }
	bool DataTransferBridge::IsSessionRunning() const { return impl->IsSessionRunning(); }

} // namespace winrt::MidiPipeBridge::implementation
