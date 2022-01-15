#pragma once

#include "d2d1_common.h"
#include "math2d.h"
#define WM_OCCLUSION (WM_USER + 0)

template <typename T>
struct D2DWindow 
	: CWindowImpl<T, CWindow, CWinTraits<WS_OVERLAPPEDWINDOW | WS_VISIBLE>>
{
	// other
	wrl::ComPtr<ID3D11Device> m_d3dDevice;
	wrl::ComPtr<ID3D11DeviceContext> m_d3dContext;
	// device dependent
	wrl::ComPtr<ID2D1Device1> m_d2dDevice;
	wrl::ComPtr<IDXGISwapChain1> m_swapChain;
	wrl::ComPtr<ID2D1DeviceContext1> m_d2dContext;
	
	//
	float m_dpiX;
	float m_dpiY;
	BOOL m_visible;
	DWORD m_occlusion;
	D2D1_SIZE_F m_dimf;
protected:
	D2D1::Matrix3x2F matrix_;
	
	D2D1::MyPoint2F GetCenterPositionScreen() const noexcept {
		return D2D1::MyPoint2F {m_dimf.width / 2.0f, m_dimf.height / 2.0f};
	}

	D2D1::MyPoint2F GetCenterPosition() const noexcept {
		return GetCenterPositionScreen() * D2D1::Invert(matrix_);
	}
private:
	using Base = CWindowImpl<T, CWindow, CWinTraits<WS_OVERLAPPEDWINDOW | WS_VISIBLE>>;

	BEGIN_MSG_MAP(c)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_PAINT, PaintHandler)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgr)

		MESSAGE_HANDLER(WM_DESTROY, DestroyHandler)
		MESSAGE_HANDLER(WM_SIZE, SizeHandler)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, GetMinMaxInfoHandler)
		MESSAGE_HANDLER(WM_ACTIVATE, ActivateHandler)
		MESSAGE_HANDLER(WM_OCCLUSION, OcclusionHandler)
		MESSAGE_HANDLER(WM_POWERBROADCAST, PowerHandler)
	END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		//GR::get_const_instance().d2dFactory->GetDesktopDpi(&m_dpiX, &m_dpiY);
		m_dpiX = m_dpiY = 96.0f;

		CRect rc;
		this->GetClientRect(rc);

		// DX
		CreateDeviceResources(this->m_hWnd);
		CreateDeviceSizeResources(rc.Width(), rc.Height());
		static_cast<T*>(this)->CreateResources();
		static_cast<T*>(this)->OnBoundsChanged();
		static_cast<T*>(this)->OnResourcesCreated();

		return 0;
	}

	LRESULT OnEraseBkgr(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		return 0;
	}

	LRESULT ActivateHandler(UINT, WPARAM wParam, LPARAM, BOOL&)
	{
		m_visible = !HIWORD(wParam);
		return 0;
	}

	LRESULT PaintHandler(UINT, WPARAM, LPARAM, BOOL&)
	{
		PAINTSTRUCT ps;
		this->BeginPaint(&ps);
		static_cast<T*>(this)->Render();
		this->EndPaint(&ps);
		return 0;
	}

	LRESULT GetMinMaxInfoHandler(UINT, WPARAM, LPARAM lParam, BOOL&)
	{
		auto info = reinterpret_cast<MINMAXINFO*>(lParam);
		info->ptMinTrackSize.y = 200;
		return 0;
	}

	LRESULT SizeHandler(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
	{
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);

		if (m_d2dContext && SIZE_MINIMIZED != wParam) 
		{
			m_d2dContext->SetTarget(nullptr);
			if (S_OK == m_swapChain->ResizeBuffers(0,
				0, 0,
				DXGI_FORMAT_UNKNOWN,
				0
			))
			{
				CreateDeviceSwapChainBitmap();
				CreateDeviceSizeResources(width, height);
				static_cast<T*>(this)->OnBoundsChanged();
				Render();
			}
			else
			{
				ReleaseDevice();
			}
		}

		return 0;
	}
	LRESULT OcclusionHandler(UINT, WPARAM, LPARAM, BOOL &)
	{
		ASSERT(m_occlusion);

		if (S_OK == m_swapChain->Present(0, DXGI_PRESENT_TEST))
		{
			GR::get_instance().dxgiFactory->UnregisterOcclusionStatus(m_occlusion);
			m_occlusion = 0;
			m_visible = true;
		}

		return 0;
	}
	LRESULT PowerHandler(UINT, WPARAM, LPARAM lparam, BOOL &)
	{
		auto const ps = reinterpret_cast<POWERBROADCAST_SETTING *>(lparam);
		m_visible = 0 != *reinterpret_cast<DWORD const *>(ps->Data);

		if (m_visible) this->PostMessage(WM_NULL);

		return TRUE;
	}
	LRESULT DestroyHandler(UINT, WPARAM, LPARAM, BOOL&)
	{
		PostQuitMessage(0);
		return 0;
	}
	
	void ReleaseDevice()
	{
		m_d2dContext.Reset();
		m_swapChain.Reset();
		m_d3dDevice.Reset();
		m_d3dContext.Reset();

		static_cast<T*>(this)->ReleaseDeviceResources();
	}

	void Render() {
		m_d2dContext->BeginDraw();
		static_cast<T*>(this)->Draw();
		m_d2dContext->EndDraw();
		
		auto const hr = m_swapChain->Present(1, 0);
		
		if (S_OK == hr) {
			// do nothing
		} else if (DXGI_STATUS_OCCLUDED == hr) {
			HR(GR::get_instance().dxgiFactory->RegisterOcclusionStatusWindow(this->m_hWnd, WM_USER, &m_occlusion));
			m_visible = false;
		} else {
			ReleaseDevice();
		}
	}

	BOOL SubclassWindow(_In_ HWND hWnd) {
		ATLASSUME(this->m_hWnd == NULL);
		ATLASSERT(::IsWindow(hWnd));

		BOOL ok = __super::SubclassWindow(hWnd);
	
		if (ok) {
			CRect rc;
			this->GetClientRect(rc);
			GR::get_const_instance().d2dFactory->GetDesktopDpi(&m_dpiX, &m_dpiY);
			CreateDeviceIndependentResources();
			CreateDeviceResources(this->m_hWnd);
			CreateDeviceSizeResources(rc.right - rc.left, rc.bottom - rc.top);
			static_cast<T*>(this)->CreateResources();
			static_cast<T*>(this)->OnBoundsChanged();
		}

		return ok;
	}

	HWND UnsubclassWindow(_In_ BOOL bForce = FALSE) {
		CleanUp();
		return __super::UnsubclassWindow(bForce);
	}

	void CreateDeviceResources(HWND hwnd) {
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
			m_d3dDevice.GetAddressOf(),               // returns the Direct3D device created
			nullptr,            // returns feature level of device created
			m_d3dContext.GetAddressOf()                    // returns the device immediate context
		));

		wrl::ComPtr<IDXGIDevice1> dxgiDevice;
		// Obtain the underlying DXGI device of the Direct3D11 device.
		HR(m_d3dDevice.As(&dxgiDevice));
		// Obtain the Direct2D device for 2-D rendering.
		HR(GR::get_instance().d2dFactory->CreateDevice(dxgiDevice.Get(), m_d2dDevice.GetAddressOf()));
		
		// Get Direct2D device's corresponding device context object.
		HR(m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			m_d2dContext.GetAddressOf()));

		m_d2dContext->SetTransform(matrix_);

		// Allocate a descriptor.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // this is the most common swapchain format
		swapChainDesc.SampleDesc.Count = 1;                // don't use multi-sampling
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;                     // use double buffering to enable flip

		// Identify the physical adapter (GPU or card) this device is runs on.
		wrl::ComPtr<IDXGIAdapter> dxgiAdapter;
		HR(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

		// Get the factory object that created the DXGI device.
		wrl::ComPtr<IDXGIFactory2> dxgiFactory;
		HR(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

		// Get the final swap chain for this window from the DXGI factory.
		HR(dxgiFactory->CreateSwapChainForHwnd(
			m_d3dDevice.Get(),
			hwnd,
			&swapChainDesc,
			nullptr,    // allow on all displays
			nullptr,
			m_swapChain.GetAddressOf()));

		// Ensure that DXGI doesn't queue more than one frame at a time.
		HR(dxgiDevice->SetMaximumFrameLatency(1));

		CreateDeviceSwapChainBitmap();
	}

	void CreateDeviceIndependentResources()
	{
		
	}

	//
	void CreateDeviceSwapChainBitmap() 
	{
		// Direct2D needs the dxgi version of the backbuffer surface pointer.
		wrl::ComPtr<IDXGISurface> dxgiBackBuffer;
		HR(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer)));

		D2D1_BITMAP_PROPERTIES1 bitmapProperties =
			D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
				m_dpiX, m_dpiY
			);

		wrl::ComPtr<ID2D1Bitmap1> bitmap;
		// Get a D2D surface from the DXGI back buffer to use as the D2D render target.
		HR(m_d2dContext->CreateBitmapFromDxgiSurface(
				dxgiBackBuffer.Get(),
				&bitmapProperties,
				bitmap.GetAddressOf()
			));
		// Now we set up the Direct2D render target bitmap linked to the swapchain. 
		// Whenever we render to this bitmap, it is directly rendered to the 
		// swap chain associated with the window.
		m_d2dContext->SetDpi(m_dpiX, m_dpiY);
		m_d2dContext->SetTarget(bitmap.Get());
	}
	void CreateDeviceSizeResources(UINT width, UINT height) {
		m_dimf = {static_cast<float>(width), static_cast<float>(height)};
	//	D2D1_BITMAP_PROPERTIES1 bitmapProperties =
	//		D2D1::BitmapProperties1(
	//			D2D1_BITMAP_OPTIONS_TARGET,
	//			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
	//			m_dpiX, m_dpiY
	//		);

	//	HR(m_d2dContext->CreateBitmap(D2D1::SizeU(width, height),
	//		nullptr, 0,
	//		bitmapProperties,
	//		m_bitmap.ReleaseAndGetAddressOf()
	//	));

		//m_bounds = m_d2dContext->GetSize();
	}

	// stubs
	void OnBoundsChanged() {}
	void ReleaseDeviceResources() {}
	void CreateResources() {}
	void OnResourcesCreated() {}


	void Draw() {
		m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::GreenYellow));
	}

	void CleanUp() {
		m_d2dContext.Reset();
		m_swapChain.Reset();
		m_d2dDevice.Reset();
		m_d3dContext.Reset();
		m_d3dDevice.Reset();
	}

	D2DWindow() 
		: matrix_{D2D1::Matrix3x2F::Identity()}
	{
	}

	~D2DWindow() {
		CleanUp();
	}
};
