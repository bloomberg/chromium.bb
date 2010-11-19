# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'cpp_sources' : [
      # Updated automatically by update-scons.py.
      # From ppapi.gyp:ppapi_cpp_objects:.*\.cc
      '<(DEPTH)/ppapi/cpp/core.cc',
      '<(DEPTH)/ppapi/cpp/graphics_2d.cc',
      '<(DEPTH)/ppapi/cpp/image_data.cc',
      '<(DEPTH)/ppapi/cpp/instance.cc',
      '<(DEPTH)/ppapi/cpp/module.cc',
      '<(DEPTH)/ppapi/cpp/paint_aggregator.cc',
      '<(DEPTH)/ppapi/cpp/paint_manager.cc',
      '<(DEPTH)/ppapi/cpp/rect.cc',
      '<(DEPTH)/ppapi/cpp/resource.cc',
      '<(DEPTH)/ppapi/cpp/url_loader.cc',
      '<(DEPTH)/ppapi/cpp/url_request_info.cc',
      '<(DEPTH)/ppapi/cpp/url_response_info.cc',
      '<(DEPTH)/ppapi/cpp/var.cc',
      '<(DEPTH)/ppapi/cpp/dev/audio_config_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/audio_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/buffer_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/directory_entry_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/directory_reader_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/file_chooser_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/file_io_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/file_ref_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/file_system_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/find_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/font_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/fullscreen_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/graphics_3d_client_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/graphics_3d_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/printing_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/scrollbar_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/selection_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/transport_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/url_util_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/video_decoder_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/widget_client_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/widget_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/zoom_dev.cc',
      '<(DEPTH)/ppapi/cpp/dev/scriptable_object_deprecated.cc',
      # End ppapi.gyp
      # Updated automatically by update-scons.py.
      # From ppapi.gyp:ppapi_cpp:.*\.cc
      '<(DEPTH)/ppapi/cpp/ppp_entrypoints.cc',
      # End ppapi.gyp
    ]
  },
  'targets': [
    {
      'target_name': 'ppapi_cpp',
      'type': 'static_library',
      'sources': [
        '<@(cpp_sources)',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
  ],
}
