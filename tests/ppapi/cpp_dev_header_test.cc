// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a build test that includes the dev PPAPI C++ headers, to make
// sure that the ppapi header layout in the NaCl toolchain is correct.

#include "ppapi/cpp/dev/audio_config_dev.h"
#include "ppapi/cpp/dev/audio_dev.h"
// TODO(dspringer): Add this file back in when
// http://codereview.chromium.org/3455005/show lands.
// #include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/directory_reader_dev.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/dev/find_dev.h"
#include "ppapi/cpp/dev/font_dev.h"
#include "ppapi/cpp/dev/graphics_3d_dev.h"
#include "ppapi/cpp/dev/scrollbar_dev.h"
#include "ppapi/cpp/dev/url_loader_dev.h"
#include "ppapi/cpp/dev/url_request_info_dev.h"
#include "ppapi/cpp/dev/url_response_info_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/dev/widget_dev.h"
