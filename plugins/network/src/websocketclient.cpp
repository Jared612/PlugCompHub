#include "websocketclient.h"

namespace pch {

WebSocketClient::~WebSocketClient()
{
	close();
}

void WebSocketClient::setMessageCallback(MessageCallback cb) {
	std::lock_guard<std::mutex> lk(_mutex);
	_onMessage = std::move(cb);
}

void WebSocketClient::setCloseCallback(CloseCallback cb) {
	std::lock_guard<std::mutex> lk(_mutex);
	_onClose = std::move(cb);
}

void WebSocketClient::connect(const char* /*asioContextName*/, const std::string& host, const std::string& port,
                               const std::string& target, ConnectCallback cb)
{
	std::lock_guard<std::mutex> lk(_mutex);
	if (_running) return;

	std::string url = "ws://" + host + ":" + port + target;
	_running = true;
	_thread = std::thread(&WebSocketClient::workerLoop, this, url, std::move(cb));
}

void WebSocketClient::workerLoop(const std::string& url, ConnectCallback cb)
{
	_ws.reset(new httplib::ws::WebSocketClient(url));

	if (_ws->connect()) {
		if (cb) cb(true, "");
	} else {
		if (cb) cb(false, "connection failed");
		_running = false;
		return;
	}

	// 读取循环
	while (_running && _ws->is_open()) {
		std::string msg;
		auto result = _ws->read(msg);
		if (result == httplib::ws::ReadResult::Text || result == httplib::ws::ReadResult::Binary) {
			MessageCallback mc;
			{
				std::lock_guard<std::mutex> lk(_mutex);
				mc = _onMessage;
			}
			if (mc) mc(msg);
		} else {
			break;
		}
	}

	_running = false;

	CloseCallback cc;
	{
		std::lock_guard<std::mutex> lk(_mutex);
		cc = _onClose;
	}
	if (cc) cc();
}

void WebSocketClient::send(const std::string& msg)
{
	std::lock_guard<std::mutex> lk(_mutex);
	if (_ws && _ws->is_open()) {
		_ws->send(msg);
	}
}

void WebSocketClient::close()
{
	std::lock_guard<std::mutex> lk(_mutex);
	if (!_running) return;

	_running = false;
	if (_ws) {
		try { _ws->close(); } catch (...) {}
	}
	if (_thread.joinable()) _thread.join();
}

}
