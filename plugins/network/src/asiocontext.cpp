#include "asiocontext.h"

#include <boost/asio.hpp>

namespace pch {

AsioContext::AsioContext()
	: _work(boost::asio::make_work_guard(_io)),
	  _thread([this]() {
		_io.run();
	})
{
}

AsioContext::~AsioContext()
{
	_work.reset();
	if (_thread.joinable()) {
		_thread.join();
	}
}

AsioContextManager& AsioContextManager::instance()
{
	static AsioContextManager mgr;
	return mgr;
}

std::shared_ptr<AsioContext> AsioContextManager::get(const char* name)
{
	const std::string key = (name == nullptr || name[0] == '\0') ? "default" : name;
	std::lock_guard<std::mutex> lk(_mutex);
	auto it = _contexts.find(key);
	if (it != _contexts.end()) {
		return it->second;
	}
	auto ctx = std::make_shared<AsioContext>();
	_contexts.emplace(key, ctx);
	return ctx;
}

}
