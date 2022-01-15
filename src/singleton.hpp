#ifndef SINGLETON_HPP_
#define SINGLETON_HPP_

#include <memory>
#include <mutex>

template<typename T>
class singleton {
	inline static std::once_flag flag_;
	inline static std::unique_ptr<T> instance_;
public:
	static T& get_instance() {
		std::call_once(flag_, []() {
			instance_.reset(new T());
			});
		return *instance_.get();
	}
};

#endif // SINGLETON_HPP_