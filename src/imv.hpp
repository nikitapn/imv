#pragma once

#include <bitset>
#include <vector>
#include <cassert>
#include <limits>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

#include <boost/asio/strand.hpp>

#include "thread_pool.hpp"
#include "d2d1_window.h"


using tp = thread_pool_3;

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
	bool operator==(const Interval& other) const noexcept { return value_ == other.value_; }

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

	CirculalInterval operator-(size_t ix) noexcept {
		auto tmp = *this;
		while (ix--) --tmp;
		return tmp;
	}

	CirculalInterval operator+(size_t ix) noexcept {
		auto tmp = *this;
		while (ix--) ++tmp;
		return tmp;
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
	static constexpr float scale_table[]{1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 7.0f, 10.0f};
	CPoint drag_old_point_;
	CirculalInterval<std::int64_t> current_img_idx_;
	Interval<std::int64_t> scale_idx_{0, sizeof(scale_table) / sizeof(float) - 1, 1};
	CCursorHandle cursor_arrow_;
	CCursorHandle cursor_sizeall_;
	CMenu menu_;
	bool fit_to_window_ = false;
	bool image_changed_ = true;

	class Image {
		ImvWindow& window_;
		std::string image_path_;
		D2D1_RECT_F rect_;
		wrl::ComPtr<IWICFormatConverter> image_;
		wrl::ComPtr<IWICBitmapFlipRotator> rotator_;
		wrl::ComPtr<ID2D1Bitmap1> bitmap_;
		enum class ImageStatus { FAILED_TO_LOAD, LOADING, LOADED_DI, LOADED_DD } status_ = ImageStatus::LOADING;
		boost::asio::io_context::strand strand_;

		IWICBitmapSource* get_image() {
			if (rotator_ && rotation_idx() != 0) [[unlikely]] {
				return rotator_.Get();
			} else {
				return image_.Get();
			}
		}
	public:
		CirculalInterval<std::int64_t> rotation_idx{0, 3, 1};

		ID2D1Bitmap1* bitmap() { return bitmap_.Get(); }
		const D2D1_RECT_F& rect() const noexcept { return rect_; }
		std::string_view image_path() const noexcept { return image_path_; }
		boost::asio::io_context::strand& strand() { return strand_; }

		bool is_loaded() noexcept {
			ImageStatus status = status_;
			if (status != ImageStatus::LOADED_DD) {
				std::unique_lock<std::mutex> lk(window_.mutex_);
				window_.cv_.wait(lk, [this]() { return status_ >= ImageStatus::LOADED_DI; });
				if (status_ == ImageStatus::LOADED_DI) {
					window_.cv_.wait(lk, [this]() { return status_ >= ImageStatus::LOADED_DD; });
				}
				status = status_;
			}
			return (status == ImageStatus::LOADED_DD);
		}

		void load_di() {
			wrl::ComPtr<IWICBitmapDecoder> decoder;

			HRESULT hr = GR::get_instance().wicFactory->CreateDecoderFromFilename(
				wide(image_path_).c_str(),       // Image to be decoded
				NULL,                            // Do not prefer a particular vendor
				GENERIC_READ,                    // Desired read access to the file
				WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
				decoder.GetAddressOf()           // Pointer to the decoder
			);

			if (FAILED(hr)) { 
				std::lock_guard<std::mutex> lk(window_.mutex_);
				status_ = ImageStatus::FAILED_TO_LOAD;
				window_.cv_.notify_all();
				return;
			}

			wrl::ComPtr<IWICBitmapFrameDecode> source;

			// Retrieve the first frame of the image from the decoder
			HR(decoder->GetFrame(0, source.GetAddressOf()));
			HR(GR::get_instance().wicFactory->CreateFormatConverter(image_.ReleaseAndGetAddressOf()));

			HR(image_->Initialize(
				source.Get(),
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone,
				nullptr,
				0.0f,
				WICBitmapPaletteTypeMedianCut)
			);

			{
				std::lock_guard<std::mutex> lk(window_.mutex_);
				status_ = ImageStatus::LOADED_DI;
			}
			window_.cv_.notify_all();

			if (rotation_idx() != 0) {
				constexpr WICBitmapTransformOptions tab[] = {
					WICBitmapTransformRotate0, WICBitmapTransformRotate90,
					WICBitmapTransformRotate180, WICBitmapTransformRotate270
				};
				// Create the flip/rotator.
				HR(GR::get_instance().wicFactory->CreateBitmapFlipRotator(rotator_.ReleaseAndGetAddressOf()));
				HR(rotator_->Initialize(image_.Get(), tab[rotation_idx()]));
			}
		}

		void load_dd(bool recreate_bitmap = false) {
			if (status_ < ImageStatus::LOADED_DI) {
				std::unique_lock<std::mutex> lk(window_.mutex_);
				window_.cv_.wait(lk, [this]() { return status_ >= ImageStatus::LOADED_DI; });
			}

			if (!bitmap_ || recreate_bitmap) {
				HR(window_.d2d1_context()->CreateBitmapFromWicBitmap(get_image(), bitmap_.ReleaseAndGetAddressOf()));
			}

			auto targetSize = window_.d2d1_context()->GetSize();
			auto imageSize = bitmap_->GetSize();

			if (window_.is_fit_to_window() || imageSize.width > targetSize.width || imageSize.height > targetSize.height) {
				float imageAspectRatio = imageSize.width / imageSize.height;
				float targetAspectRatio = targetSize.width / targetSize.height;

				if (imageAspectRatio < targetAspectRatio) {
					rect_.top = 0.0f;
					rect_.bottom = targetSize.height;

					auto width = imageAspectRatio * targetSize.height;
					rect_.left = (targetSize.width - width) / 2.0f;
					rect_.right = width + rect_.left;
				} else {
					rect_.left = 0.0f;
					rect_.right = targetSize.width;

					auto heigth = targetSize.width / imageAspectRatio;
					rect_.top = (targetSize.height - heigth) / 2.0f;
					rect_.bottom = heigth + rect_.top;
				}
			} else {
				rect_.left = (targetSize.width - imageSize.width) / 2.0f;
				rect_.right = rect_.left + imageSize.width;

				rect_.top = (targetSize.height - imageSize.height) / 2.0f;
				rect_.bottom = rect_.top + imageSize.height;
			}

			{
				std::lock_guard<std::mutex> lk(window_.mutex_);
				status_ = ImageStatus::LOADED_DD;
			}
			window_.cv_.notify_all();
		}

		void load_d2d_resources(bool recreate_bitmap) {
			load_di();
			load_dd(recreate_bitmap);
		}

		void free_d2d_resources() {
			// release IWICFormatConverter also
			// otherwise bitmap isn't going to be freed 
			rotator_.Reset();
			image_.Reset();
			bitmap_.Reset();
			status_ = ImageStatus::LOADING;
		}

		Image(ImvWindow& window, std::string&& path)
			: window_{window}
			, image_path_{path}
			, strand_{tp::get_instance().ctx()}
		{
		}
	};

	std::mutex mutex_;
	std::condition_variable cv_;
	std::vector<Image> images_;
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
		rotate_clockwise();
		Invalidate();
		return 0;
	}

	LRESULT OnRotateAntiClockwise(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
		rotate_anti_clockwise();
		Invalidate();
		return 0;
	}

	Image& current_image() {
		return images_[current_img_idx_()];
	}

	void CreateResources() { // override
		//using D2D1::ColorF;
		//m_d2dContext->CreateSolidColorBrush(ColorF(ColorF::LightSlateGray), m_brush.GetAddressOf());
	}

	bool is_fit_to_window() { return fit_to_window_; }

	void Draw() {
		using D2D1::ColorF;

		auto& current = current_image();

		if (image_changed_) {
			this->SetWindowText(current.image_path().data());
			image_changed_ = false;
		}

		if (current.is_loaded()) {
			m_d2dContext->Clear(ColorF(ColorF::LightGray));
			m_d2dContext->DrawBitmap(current.bitmap(), current.rect(),
				1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		} else {
			m_d2dContext->Clear(ColorF(ColorF::LightGray));
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

	void rotate_clockwise() {
		++current_image().rotation_idx;
		current_image().load_d2d_resources(true);
	}

	void rotate_anti_clockwise() {
		--current_image().rotation_idx;
		current_image().load_d2d_resources(true);
	}

	void next_image() {
		auto prev = current_img_idx_ - 1;
		++current_img_idx_;
		image_changed_ = true;

		if (current_img_idx_ == prev || current_img_idx_ + 1 == prev || prev == current_img_idx_ - 1) return;

		async<false>(images_[prev()].strand(), &Image::free_d2d_resources, &images_[prev()]);
		async<false>(images_[(current_img_idx_ + 1)()].strand(), &Image::load_d2d_resources, &images_[(current_img_idx_ + 1)()], false);
	}

	void prev_image() {
		auto next = current_img_idx_ + 1;
		--current_img_idx_;
		image_changed_ = true;

		if (current_img_idx_ == next || current_img_idx_ - 1 == next || next == current_img_idx_ + 1) return;
	
		async<false>(images_[next()].strand(), &Image::free_d2d_resources, &images_[next()]);
		async<false>(images_[(current_img_idx_ - 1)()].strand(), &Image::load_d2d_resources, &images_[(current_img_idx_ - 1)()], false);
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
			load_dd();
			break;
		case VK_LEFT:
			prev_image();
			break;
		case VK_RIGHT:
			next_image();
			break;
		case VK_NEXT:
			rotate_clockwise();
			break;
		case VK_PRIOR:
			rotate_anti_clockwise();
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

	void OnBoundsChanged() {
		load_dd(false);
	}

	template<typename Func, typename... Args> 
	void update_cached(Func&& fn, Args&&... args) {
		auto prev = current_img_idx_ - 1;
		auto next = current_img_idx_ + 1;

		if (next != prev) { // more than 3
			async<false>(images_[prev()].strand(), fn, &images_[prev()], std::forward<Args>(args)...);
			async<false>(images_[current_img_idx_()].strand(), fn, &images_[current_img_idx_()], std::forward<Args>(args)...);
			async<false>(images_[next()].strand(), fn, &images_[next()], std::forward<Args>(args)...);
		} else if (current_img_idx_ != next) { // 2 images
			async<false>(images_[current_img_idx_()].strand(), fn, &images_[current_img_idx_()], std::forward<Args>(args)...);
			async<false>(images_[next()].strand(), fn, &images_[next()], std::forward<Args>(args)...);
		} else { // 1 image
			std::invoke(fn, &images_[current_img_idx_()], std::forward<Args>(args)...);
		}
	}

	void load_di() {
		update_cached(std::mem_fn(&Image::load_di));
	}

	void load_dd(bool recreate_bitmap = false) {
		update_cached(std::mem_fn(&Image::load_dd), recreate_bitmap);
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
						images_.emplace_back(*this, path.string());
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

		load_di();
	}
};