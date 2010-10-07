// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a build test that includes the dev PPAPI C++ headers, to make
// sure that the ppapi header layout in the NaCl toolchain is correct.  The
// headers listed in this file are maintained by the script
// ../../src/shared/ppapi/update-scons.py.  Update $SOURCE_ROOT/ppapi and run
// the script to update all of the source files listed here.

// update-scons.py reads this file and finds the inclusion marker.  It
// replaces all of the #includes with the corresponding list of files from
// the ppapi.gyp file.
//
// The inclusion marker format is:
//   // From ppapi.gyp:TARGET:REGEXP
//
// For example, if this exists in this file:
//   // From ppapi.gyp:ppapi_cpp_objects:cpp/dev/[^/]*\.h
//
// then the script will remove all of the #include lines.  It will then find
// the 'ppapi_cpp_objects' target in the ppapi.gyp file.  It will find all
// 'sources' for that target that match the regular expression
// 'cpp/dev/[^/]*\.h' and will insert lines to #include each of those files.

// From ppapi.gyp:ppapi_cpp_objects:cpp/dev/[^/]*\.h
#include "ppapi/cpp/dev/audio_config_dev.h"
#include "ppapi/cpp/dev/audio_dev.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/directory_reader_dev.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/dev/find_dev.h"
#include "ppapi/cpp/dev/font_dev.h"
#include "ppapi/cpp/dev/fullscreen_dev.h"
#include "ppapi/cpp/dev/graphics_3d_dev.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/dev/scrollbar_dev.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/cpp/dev/url_loader_dev.h"
#include "ppapi/cpp/dev/url_request_info_dev.h"
#include "ppapi/cpp/dev/url_response_info_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/dev/widget_dev.h"
#include "ppapi/cpp/dev/zoom_dev.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
