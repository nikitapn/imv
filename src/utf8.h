#pragma once

#ifndef UTF8_HPP_
#define UTF8_HPP_

#include <memory>
#include <string_view>
#include <string>
#include <stdexcept>

inline std::wstring wide(std::string_view str) {
	if (str.length() == 0) return std::wstring();
	std::wstring result;
	auto len = static_cast<int>(str.length());
	result.resize(len);
	int n = MultiByteToWideChar(CP_UTF8, 0, str.data(), len, result.data(), len);
	result.resize(n);
	return result;
}
inline std::string narrow(const wchar_t* str, size_t length) {
	if (length == 0) return std::string();
	constexpr auto stackBufferSize = 256;
	char stack_buffer[stackBufferSize];
	std::unique_ptr<char[]> buffer;
	char* bufPtr = stack_buffer;
	int bufferSize = stackBufferSize;
	for (;;) {
		int len = WideCharToMultiByte(CP_UTF8, 0, str, static_cast<int>(length), bufPtr, bufferSize, NULL, NULL);
		if (len == 0) {
			auto error = GetLastError();
			if (error == ERROR_INSUFFICIENT_BUFFER) {
				bufferSize *= 2;
				buffer = std::make_unique<char[]>(bufferSize);
				bufPtr = buffer.get();
			} else {
				throw std::runtime_error("Error in WideCharToMultiByte: " + std::to_string(error));
			}
		} else {
			return std::string(bufPtr, len);
		}
	}
}

inline std::string narrow(const wchar_t* str) {
	return narrow(str, wcslen(str));
}

inline std::string narrow(const std::wstring& str) {
	return narrow(str.c_str(), str.length());
}

#endif // UTF8_HPP_