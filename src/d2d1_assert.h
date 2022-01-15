#pragma once

#include <comdef.h> // _com_error
#include <exception>

#if defined(_DEBUG) || defined(DEBUG)
#	include <crtdbg.h>
inline void TRACE(const TCHAR* format, ...) {
	va_list args;
	va_start(args, format);

	TCHAR output[512];
	_vstprintf_s(output, format, args);
	
	OutputDebugString(output);

	va_end(args);
}
#	define ASSERT(expression) _ASSERTE(expression)
#	define VERIFY(expression) ASSERT(expression)
#	define HR(_hr) if(auto hr = _hr; hr != S_OK) { _com_error e(hr); TRACE("Failure with HRESULT of 0x%08X - %s\n", hr, e.ErrorMessage()); ASSERT(FALSE); }
#else
#	define ASSERT(expression) __noop
#	define VERIFY(expression) __noop
#	define TRACE __noop;
	class com_exceprion 
		: public std::exception {
		HRESULT hresult_;
	public:
		virtual const char* what() const override {
			static char s_str[256] = { 0 };
			_com_error e(hresult_);
			sprintf_s(s_str, "Failure with HRESULT of 0x%08X - %s\n", hresult_, e.ErrorMessage());
			return s_str;
		}
		HRESULT hr() const noexcept { return hresult_; }
		com_exceprion(HRESULT hr) : hresult_(hr) {}
	};

	inline void HR(HRESULT hr) {
		if (hr != S_OK) throw com_exceprion(hr);
	}
#endif