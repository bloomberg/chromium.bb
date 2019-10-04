/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include <blpwtk2_config.h>

#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webviewclientimpl.h>
#include <blpwtk2_webviewclient.h>
#include <blpwtk2_findonpage.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>

#include <base/compiler_specific.h>
#include <ipc/ipc_sender.h>
#include <ui/gfx/geometry/rect.h>

#include <cstdint>
#include <string>
#include <vector>
#include <blpwtk2_gpudataloggerImpl.h>
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include <iostream>


#include <blpwtk2_webviewclientimpl.h>
#include <blpwtk2_webviewclientdelegate.h>

#include <blpwtk2_string.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_contextmenuparams.h>

#include <ipc/ipc_message_macros.h>
#include <windows.h>

namespace blpwtk2 {


// static
scoped_refptr<GpuDataLogger> GpuDataLogger::Create() {
  return new GpuDataLoggerImpl();
}

GpuDataLoggerImpl::GpuDataLoggerImpl() {}

GpuDataLoggerImpl::~GpuDataLoggerImpl() {}

// Called for any observers whenever there is a log message added to the GPU data.
void GpuDataLoggerImpl::OnAddLogMessage(int level, const std::string& header, const std::string& message) {
	switch (level) {
		case logging::LOG_INFO:
			LOG(INFO) << "gpu log: [" << header << "] " << message;
			break;
		case logging::LOG_WARNING:
			LOG(WARNING) << "gpu log: [" << header << "] " << message;
			break;
		case logging::LOG_ERROR:
			LOG(ERROR) << "gpu log: [" << header << "] " << message;
			break;
		case logging::LOG_FATAL:
			LOG(FATAL) << "gpu log: [" << header << "] " << message;
			break;
		default:
			VLOG(level) << "gpu log: [" << header << "] " << message;
			break;
	}
}

}  // namespace blpwtk2

