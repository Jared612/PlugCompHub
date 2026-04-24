#include "pcxwebsocketclient.h"

#include "netcommon.h"

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <memory>

namespace pcx {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

PcxWebSocketClient::~PcxWebSocketClient()
{
	close();
}

void PcxWebSocketClient::setMessageCallback(MessageCallback cb)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_onMessage = std::move(cb);
}

void PcxWebSocketClient::setCloseCallback(CloseCallback cb)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_onClose = std::move(cb);
}

void PcxWebSocketClient::connect(const char* asioContextName, const std::string& host, const std::string& port,
	const std::string& target, ConnectCallback cb)
{
	if (_running) {
		if (cb) {
			cb(true, "");
		}
		return;
	}

	if (_worker.joinable()) {
		_worker.join();
	}

	_ioContext = resolveIoContext(asioContextName);
	if (_ioContext == nullptr) {
		if (cb) {
			cb(false, "asio context not found");
		}
		return;
	}

	_running = true;
	_worker = std::thread([this, host, port, target, cb = std::move(cb)]() mutable {
		try {
			tcp::resolver resolver(*_ioContext);
			auto const results = resolver.resolve(host, port);

			auto ws = std::make_shared<websocket::stream<tcp::socket>>(*_ioContext);
			net::connect(ws->next_layer(), results.begin(), results.end());
			ws->handshake(host, target);

			{
				std::lock_guard<std::mutex> lk(_ioMutex);
				_ws = ws;
			}

			if (cb) {
				cb(true, "");
			}

			while (_running) {
				beast::flat_buffer buffer;
				{
					std::lock_guard<std::mutex> lk(_ioMutex);
					if (!_ws) {
						break;
					}
					_ws->read(buffer);
				}

				auto msg = beast::buffers_to_string(buffer.data());
				MessageCallback onMessage;
				{
					std::lock_guard<std::mutex> lk(_mutex);
					onMessage = _onMessage;
				}
				if (onMessage) {
					onMessage(msg);
				}
			}
		}
		catch (const std::exception& ex) {
			if (cb) {
				cb(false, ex.what());
			}
		}

		{
			std::lock_guard<std::mutex> lk(_ioMutex);
			_ws.reset();
		}
		_running = false;

		CloseCallback onClose;
		{
			std::lock_guard<std::mutex> lk(_mutex);
			onClose = _onClose;
		}
		if (onClose) {
			onClose();
		}
	});
}

void PcxWebSocketClient::send(const std::string& msg)
{
	std::lock_guard<std::mutex> lk(_ioMutex);
	if (!_ws || !_running) {
		return;
	}
	try {
		_ws->binary(false);
		_ws->write(net::buffer(msg));
	}
	catch (...) {
	}
}

void PcxWebSocketClient::close()
{
	_running = false;

	{
		std::lock_guard<std::mutex> lk(_ioMutex);
		if (_ws) {
			boost::system::error_code ec;
			_ws->close(websocket::close_code::normal, ec);
			_ws.reset();
		}
	}

	if (_worker.joinable() && std::this_thread::get_id() != _worker.get_id()) {
		_worker.join();
	}
}

}
