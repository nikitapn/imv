#pragma once

#include <atlbase.h>
#include <atlwin.h>
#include <wrl.h>
#include <d2d1_3.h>
#include <d3d11_4.h>
#include <d2d1effects_2.h>
#include <dwrite.h>
#include <wincodec.h> //	Defines C and C++ versions of the primary WIC APIs.
#include <string>
#include "d2d1_assert.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "dxguid.lib") // effects clsids

namespace wrl = Microsoft::WRL;
namespace d2d = D2D1;

class GlobalResourses {
	float m_fontSize;
	std::wstring m_font;

	void ReCreateFont()
	{
		HR(dwriteFactory->CreateTextFormat(m_font.c_str(),
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			m_fontSize,
			L"", //locale
			textFormatCenter.ReleaseAndGetAddressOf()));

		// Center the text horizontally and vertically.
		HR(textFormatCenter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
		HR(textFormatCenter->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));

		// Left Alignment
		HR(dwriteFactory->CreateTextFormat(m_font.c_str(),
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			m_fontSize,
			L"", //locale
			textFormatLeft.ReleaseAndGetAddressOf()));

		// Center the text horizontally and vertically.
		HR(textFormatLeft->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
		HR(textFormatLeft->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));

		// Create a DirectWrite text format object.
		HR(dwriteFactory->CreateTextFormat(m_font.c_str(),
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			m_fontSize,
			L"", //locale
			textFormatRight.ReleaseAndGetAddressOf()));

		// Center the text horizontally and vertically.
		HR(textFormatRight->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING));
		HR(textFormatRight->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
	}
public:
	wrl::ComPtr<ID3D11Device> d3dDevice;
	wrl::ComPtr<ID3D11DeviceContext> d3dContext;
	wrl::ComPtr<ID2D1Device1> d2dDevice;
	wrl::ComPtr<IDXGIAdapter> dxgiAdapter;
	
	wrl::ComPtr<ID2D1Factory2> d2dFactory;
	wrl::ComPtr<IDXGIFactory2> dxgiFactory;
	wrl::ComPtr<IDWriteFactory> dwriteFactory;
	wrl::ComPtr<IWICImagingFactory2> wicFactory;
	wrl::ComPtr<IDWriteTextFormat> textFormatCenter;
	wrl::ComPtr<IDWriteTextFormat> textFormatLeft;
	wrl::ComPtr<IDWriteTextFormat> textFormatRight;

	void CreateDevice() {
		// This flag adds support for surfaces with a different color channel ordering than the API default.
		// You need it for compatibility with Direct2D.
		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

		// This array defines the set of DirectX hardware feature levels this app  supports.
		// The ordering is important and you should  preserve it.
		// Don't forget to declare your app's minimum required feature level in its
		// description.  All apps are assumed to support 9.1 unless otherwise stated.
		static const D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};

		// Create the DX11 API device object, and get a corresponding context.

		HR(D3D11CreateDevice(
			nullptr,                    // specify null to use the default adapter
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			creationFlags,              // optionally set debug and Direct2D compatibility flags
			featureLevels,              // list of feature levels this app can support
			ARRAYSIZE(featureLevels),   // number of possible feature levels
			D3D11_SDK_VERSION,
			d3dDevice.GetAddressOf(),               // returns the Direct3D device created
			nullptr,            // returns feature level of device created
			d3dContext.GetAddressOf()                    // returns the device immediate context
		));

		wrl::ComPtr<IDXGIDevice1> dxgiDevice;
		// Obtain the underlying DXGI device of the Direct3D11 device.
		HR(d3dDevice.As(&dxgiDevice));
		// Obtain the Direct2D device for 2-D rendering.
		HR(d2dFactory->CreateDevice(dxgiDevice.Get(), d2dDevice.GetAddressOf()));
		// Identify the physical adapter (GPU or card) this device is runs on.
		HR(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));
		// Ensure that DXGI doesn't queue more than one frame at a time.
		HR(dxgiDevice->SetMaximumFrameLatency(1));
	}

	HRESULT CreateTextLayoutLeft(const std::wstring& text, const D2D1_RECT_F& rect, wrl::ComPtr<IDWriteTextLayout>& layout) const {
		return dwriteFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()),
			textFormatLeft.Get(),
			rect.right - rect.left, rect.bottom - rect.top,
			layout.ReleaseAndGetAddressOf());
	}

	HRESULT CreateTextLayoutCenter(const std::wstring& text, const D2D1_RECT_F& rect, wrl::ComPtr<IDWriteTextLayout>& layout) const {
		return dwriteFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()),
			textFormatCenter.Get(),
			rect.right - rect.left, rect.bottom - rect.top,
			layout.ReleaseAndGetAddressOf());
	}

	HRESULT CreateTextLayoutRight(const std::wstring& text, const D2D1_RECT_F& rect, wrl::ComPtr<IDWriteTextLayout>& layout) const {
		return dwriteFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()),
			textFormatRight.Get(),
			rect.right - rect.left, rect.bottom - rect.top,
			layout.ReleaseAndGetAddressOf());
	}

	GlobalResourses() {
		D2D1_FACTORY_OPTIONS options;
		//	
#ifdef DEBUG
		options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#else 
		options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif // DEBUG

		HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory2),
			&options, (void**)d2dFactory.GetAddressOf()));

		HR(CreateDXGIFactory1(__uuidof(dxgiFactory),
			reinterpret_cast<void **>(dxgiFactory.GetAddressOf())));

		HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf())));

		m_font = L"Verdana";
		m_fontSize = 18;

		// Create the COM imaging factory
		HR(CoCreateInstance(CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER,
			__uuidof(IWICImagingFactory2), (void**)wicFactory.GetAddressOf()));

		ReCreateFont();
		CreateDevice();
	}

	void Uninitialize() {
		textFormatRight.Reset();
		textFormatLeft.Reset();
		textFormatCenter.Reset();
		wicFactory.Reset();
		dwriteFactory.Reset();
		dxgiFactory.Reset();
		d2dFactory.Reset();
		dxgiAdapter.Reset();
		d2dDevice.Reset();
		d3dContext.Reset();
		d3dDevice.Reset();
	}
};

#include "singleton.hpp"
using GR = singleton<GlobalResourses>;