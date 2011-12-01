// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test simply includes all the C++ headers to ensure they compile with a
// C++ compiler.  If it compiles, it passes.
//
#ifndef PPAPI_TESTS_ALL_CPP_INCLUDES_H_
#define PPAPI_TESTS_ALL_CPP_INCLUDES_H_

#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/directory_reader_dev.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/dev/find_dev.h"
#include "ppapi/cpp/dev/font_dev.h"
#include "ppapi/cpp/dev/fullscreen_dev.h"
#include "ppapi/cpp/dev/ime_input_event_dev.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/dev/scrollbar_dev.h"
#include "ppapi/cpp/dev/selection_dev.h"
#include "ppapi/cpp/dev/text_input_dev.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/dev/websocket_dev.h"
#include "ppapi/cpp/dev/widget_client_dev.h"
#include "ppapi/cpp/dev/widget_dev.h"
#include "ppapi/cpp/dev/zoom_dev.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/non_thread_safe_ref_count.h"
#include "ppapi/cpp/paint_aggregator.h"
#include "ppapi/cpp/paint_manager.h"
#include "ppapi/cpp/private/flash_fullscreen.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"

#endif  // PPAPI_TESTS_ALL_CPP_INCLUDES_H_
