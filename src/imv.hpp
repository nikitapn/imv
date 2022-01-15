#pragma once

#include "d2d1_window.h"

#include <bitset>
#include <vector>
#include <cassert>
#include <limits>

template<typename T>
class Interval {
	static_assert(std::is_signed_v<T>);
protected:
	T value_;
	T first_;
	T last_;
	T increment_decrement_;
public:
	Interval operator++() noexcept {
		if (value_ + increment_decrement_ <= last_) {
			value_ = value_ + increment_decrement_;
		}
		return *this;
	}

	Interval operator--() noexcept {
		if (value_ - increment_decrement_ >= first_) {
			value_ = value_ - increment_decrement_;
		}
		return *this;
	}

	T reset() { return value_ = first_; }
	T operator()() const noexcept { return value_; }

	void set_value(T value) noexcept {
		assert(value >= first_ && value <= last_);
		value_ = value;
	}

	Interval()
		: value_(std::numeric_limits<T>::min())
		, first_(std::numeric_limits<T>::min())
		, last_(std::numeric_limits<T>::max())
		, increment_decrement_(std::numeric_limits<T>::epsilon()) 
	{
	}

	Interval(T first, T last, T increment_decrement) 
		: value_(first)
		, first_(first)
		, last_(last)
		, increment_decrement_(increment_decrement)
	{
	}
};

template<typename T>
class CirculalInterval : public Interval<T> {
public:
	CirculalInterval operator++() noexcept {
		if (this->value_ + this->increment_decrement_ > this->last_) this->value_ = this->first_;
		else this->value_ = this->value_ + this->increment_decrement_;
		return *this;
	}

	CirculalInterval operator--() noexcept {
		if (this->value_ - this->increment_decrement_ < this->first_) this->value_ = this->last_;
		else this->value_ = this->value_ - this->increment_decrement_;
		return *this;
	}

	CirculalInterval()
	{
	}

	CirculalInterval(T first, T last, T increment_decrement) 
		: Interval<T>(first, last, increment_decrement)
	{
	}
};


class ImvWindow 
	: public D2DWindow<ImvWindow> 
{
	enum { DIRTY_BOUNDS, DIRTY_IMAGE, DIRTY_ROTATION };

	wrl::ComPtr<IWICFormatConverter> m_image;
	wrl::ComPtr<IWICBitmapFlipRotator> m_rotator;
	wrl::ComPtr<ID2D1Bitmap1> m_bitmap;
	wrl::ComPtr<ID2D1SolidColorBrush> m_brush;
	
	D2D1_RECT_F m_imageRect;
	std::bitset<3> m_dirty;
	CPoint drag_old_point_;

	static constexpr float scale_table[] {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 7.0f, 10.0f};

	CirculalInterval<std::int64_t> current_img_idx_;
	CirculalInterval<std::int64_t> rotation_idx_{0, 3, 1};
	Interval<std::int64_t> scale_idx_{0, sizeof(scale_table) / sizeof(float) - 1, 1};

	CCursorHandle cursor_arrow_;
	CCursorHandle cursor_sizeall_;
	CMenu menu_;
	bool fit_to_window_ = false;
	std::vector<std::pair<std::string, std::int64_t>> images_;
public:
	using Base = D2DWindow<ImvWindow>;

	DECLARE_WND_CLASS2("imv", ImvWindow);

	BEGIN_MSG_MAP(c)
		MESSAGE_HANDLER(WM_CREATE, OnCreate);
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown);
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnAppExit)
		COMMAND_ID_HANDLER(ID_ROTATE_CLOCKWISE, OnRotateClockwise)
		COMMAND_ID_HANDLER(ID_ROTATE_ANTICLOCKWISE, OnRotateAntiClockwise)
		CHAIN_MSG_MAP(Base)
	END_MSG_MAP()

		LRESULT OnAppExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
			PostMessage(WM_QUIT);
			return 0;
		}

		LRESULT OnRotateClockwise(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
			RotateClockwise();
			Invalidate();
			return 0;
		}

		LRESULT OnRotateAntiClockwise(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
			RotateAntiClockwise();
			Invalidate();
			return 0;
		}

	void UpdateImage() {
		wrl::ComPtr<IWICBitmapDecoder> decoder;

		HRESULT hr = GR::get_instance().wicFactory->CreateDecoderFromFilename(
			wide(images_[current_img_idx_()].first).c_str(),                      // Image to be decoded
			NULL,                            // Do not prefer a particular vendor
			GENERIC_READ,                    // Desired read access to the file
			WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
			decoder.GetAddressOf()                        // Pointer to the decoder
		);

		if (FAILED(hr)) {
			ResetImage();
			return;
		}

		wrl::ComPtr<IWICBitmapFrameDecode> source;
		
		// Retrieve the first frame of the image from the decoder
		HR(decoder->GetFrame(0, source.GetAddressOf()));
		HR(GR::get_instance().wicFactory->CreateFormatConverter(m_image.ReleaseAndGetAddressOf()));
		
		HR(m_image->Initialize(
			source.Get(), 
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone, 
			nullptr,
			0.0f,
			WICBitmapPaletteTypeMedianCut)
		);

		rotation_idx_.set_value(images_[current_img_idx_()].second);

		m_dirty.set(DIRTY_IMAGE);
	}

	void ResetImage() {
		m_bitmap.Reset();
		m_image.Reset();
		Invalidate();
	}

	void UpdateBitmap() {
		if (!m_dirty.any()) return;

		IWICBitmapSource* image = m_image.Get();

		if (rotation_idx_() != 0) {
			constexpr WICBitmapTransformOptions tab[] = {
				WICBitmapTransformRotate0, WICBitmapTransformRotate90,
				WICBitmapTransformRotate180, WICBitmapTransformRotate270
			};
			// Create the flip/rotator.
			HR(GR::get_instance().wicFactory->CreateBitmapFlipRotator(m_rotator.ReleaseAndGetAddressOf()));
			HR(m_rotator->Initialize(m_image.Get(), tab[rotation_idx_()]));
			image = m_rotator.Get();
		} 

		HR(m_d2dContext->CreateBitmapFromWicBitmap(image, m_bitmap.ReleaseAndGetAddressOf()));
		
		auto targetSize = m_d2dContext->GetSize();
		auto imageSize = m_bitmap->GetSize();
		
		if (fit_to_window_ || imageSize.width > targetSize.width || imageSize.height > targetSize.height) {
			float imageAspectRatio = imageSize.width / imageSize.height;
			float targetAspectRatio = targetSize.width / targetSize.height;

			if (imageAspectRatio < targetAspectRatio) {
				m_imageRect.top = 0.0f;
				m_imageRect.bottom = targetSize.height;

				auto width = imageAspectRatio * targetSize.height;
				m_imageRect.left = (targetSize.width - width) / 2.0f;
				m_imageRect.right = width + m_imageRect.left;
			} else {
				m_imageRect.left = 0.0f;
				m_imageRect.right = targetSize.width;

				auto heigth = targetSize.width / imageAspectRatio;
				m_imageRect.top = (targetSize.height - heigth) / 2.0f;
				m_imageRect.bottom = heigth + m_imageRect.top;
			}
		} else {
			m_imageRect.left = (targetSize.width - imageSize.width) / 2.0f;
			m_imageRect.right = m_imageRect.left + imageSize.width;

			m_imageRect.top = (targetSize.height - imageSize.height) / 2.0f;
			m_imageRect.bottom = m_imageRect.top + imageSize.height;
		}

		m_dirty.reset();
	}

	void OnBoundsChanged() { 
		m_dirty.set(DIRTY_BOUNDS);
	}

	void CreateResources() { // override
		//using D2D1::ColorF;
		//m_d2dContext->CreateSolidColorBrush(ColorF(ColorF::LightSlateGray), m_brush.GetAddressOf());
	}

	void Draw() {
		using D2D1::ColorF;
		
		if (m_dirty.test(DIRTY_IMAGE)) {
			this->SetWindowText(images_[current_img_idx_()].first.c_str());
		}

		UpdateBitmap();

		if (!m_bitmap) {
			m_d2dContext->Clear(ColorF(ColorF::LightGray));
		} else {
			m_d2dContext->Clear(ColorF(ColorF::LightGray));
			m_d2dContext->DrawBitmap(m_bitmap.Get(), m_imageRect,
				1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		}

		//auto targetSize = m_d2dContext->GetSize();
		//auto targetRect = D2D1::RectF(0, 0, targetSize.width, targetSize.height);
		//m_d2dContext->DrawRectangle(targetRect, m_brush.Get());
	}

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		menu_.LoadMenuA(MAKEINTRESOURCE(IDR_CONTEXTMENU));

		/*
		if (m_image) {
			CRect rc;
			m_image->GetSize((UINT*)&rc.right, (UINT*)&rc.bottom);
			if (rc.bottom < 1024 && rc.top < 1024) {
				AdjustWindowRect(rc, GetWndStyle(0), false);
				SetWindowPos(NULL, rc, SW_SHOWDEFAULT);
			}
		}
		*/

		CenterWindow();

		return bHandled = 0;
	}

	void RotateClockwise() {
		++rotation_idx_;
			images_[current_img_idx_()].second = rotation_idx_();
			m_dirty.set(DIRTY_ROTATION);
	}

	void RotateAntiClockwise() {
		--rotation_idx_;
			images_[current_img_idx_()].second = rotation_idx_();
			m_dirty.set(DIRTY_ROTATION);
	}

	LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		switch (wParam) {
		case VK_UP:
			scale_idx_.set_value(0);
			matrix_ = D2D1::Matrix3x2F::Identity();
			m_d2dContext->SetTransform(matrix_);
			break;
		case VK_DOWN:
			fit_to_window_ = !fit_to_window_;
			m_dirty.set(DIRTY_BOUNDS);
			break;
		case VK_LEFT:
			--current_img_idx_;
			UpdateImage();
			break;
		case VK_RIGHT:
			++current_img_idx_;
			UpdateImage();
			break;
		case VK_NEXT:
			RotateClockwise();
			break;
		case VK_PRIOR:
			RotateAntiClockwise();
			break;
		case VK_ESCAPE:
			PostMessage(WM_QUIT);
			break;
		default:
			return 0;
			break;
		}
		
		Invalidate();

		return 0;
	}

	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

		if (zDelta > 0) {
			++scale_idx_;
		} else {
			--scale_idx_;
		}

		if (scale_idx_() != 0) {
			auto const scale = scale_table[scale_idx_()];

			auto center = GetCenterPosition();
			auto screen_center = GetCenterPositionScreen();

			auto pt = center * scale;
			matrix_ = D2D1::Matrix3x2F
			{
				scale, 0,
				0, scale,
				-(pt.x - screen_center.x), -(pt.y - screen_center.y)
			};
		} else {
			matrix_ = D2D1::Matrix3x2F::Identity();
		}

		m_d2dContext->SetTransform(matrix_);
		Invalidate();
		return 0;
	}

	LRESULT OnRButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		CPoint point(lParam);
		ClientToScreen(&point);
		menu_.GetSubMenu(0).TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, point.x, point.y, m_hWnd);
		return 0;
	}

	LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		if (scale_idx_() == 0) return 0;

		drag_old_point_ = CPoint(lParam);
		SetCursor(cursor_sizeall_);
		SetCapture();
		return 0;
	}

	LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		if (scale_idx_() == 0) return 0;

		UINT flags = static_cast<UINT>(wParam);
		CPoint point(lParam);
		
		if (flags & MK_LBUTTON) {
			if (drag_old_point_ == static_cast<POINT>(point)) return 0;
			
			auto translation = D2D1::Matrix3x2F::Translation(point.x - drag_old_point_.x, point.y - drag_old_point_.y);
			matrix_ = matrix_ * translation;

			drag_old_point_ = point;

			m_d2dContext->SetTransform(matrix_);
			Invalidate();
		}
		
		return 0;
	}

	LRESULT OnLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		if (scale_idx_() == 0) return 0;

		ReleaseCapture();
		SetCursor(cursor_arrow_);
		return 0;
	}

	ImvWindow(const fs::path image_path) {
		fs::directory_iterator m_dir_iterator;
		auto directory = image_path;
		directory.remove_filename();
		
		m_dir_iterator = fs::directory_iterator(directory);

		int64_t img_idx = -1;

		for (auto& it : m_dir_iterator) {
			if (it.is_regular_file()) {
				auto path = it.path();
				if (path.has_extension()) {
					auto ext = path.extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
	
					bool founded_image_file = true;
					
					if (ext == ".jpg" || ext == ".bmp" || ext == ".cr2" || ext == ".gif" || ext == ".png" || ext == ".ico" || ext == ".webp") {
						images_.emplace_back(path.string(), 0);
					} else {
						founded_image_file = false;
					}

					if (img_idx == -1 && founded_image_file && image_path == path) {
						img_idx = images_.size() - 1;
					}
				}
			}
		}
		
		if (img_idx == -1) throw;

		current_img_idx_ = CirculalInterval<std::int64_t>(0, images_.size() - 1, 1);
		current_img_idx_.set_value(img_idx);

		cursor_arrow_.LoadSysCursor(IDC_ARROW);
		cursor_sizeall_.LoadSysCursor(IDC_SIZEALL);

		UpdateImage();
	}

	~ImvWindow() {
		m_bitmap.Reset();
		m_image.Reset();
	}
};