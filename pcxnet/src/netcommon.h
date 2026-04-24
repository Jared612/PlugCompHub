#pragma once

#include "ipcxasiocontext.h"
#include "pcx.h"

#include <boost/asio.hpp>

namespace pcx {

inline boost::asio::io_context* resolveIoContext(const char* asioContextName)
{
	if (asioContextName == nullptr || asioContextName[0] == '\0') {
		return nullptr;
	}

	auto* ctxObj = static_cast<IPcxAsioContext*>(pcx::api::FindObject(asioContextName));
	if (ctxObj == nullptr) {
		return nullptr;
	}

	return static_cast<boost::asio::io_context*>(ctxObj->getIoContext());
}

}
