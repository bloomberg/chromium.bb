// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a build test that includes the basic PPAPI C++ headers, to make
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
//   // From ppapi.gyp:ppapi_cpp_objects:cpp/[^/]*\.h
//
// then the script will remove all of the #include lines.  It will then find
// the 'ppapi_cpp_objects' target in the ppapi.gyp file.  It will find all
// 'sources' for that target that match the regular expression 'cpp/[^/]*\.h'
// and will insert lines to #include each of those files.

// From ppapi.gyp:ppapi_cpp_objects:cpp/[^/]*\.h
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/paint_aggregator.h"
#include "ppapi/cpp/paint_manager.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"
