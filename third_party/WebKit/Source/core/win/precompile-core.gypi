# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this file to make core targets in your .gyp use the default precompiled
# header on Windows, when precompiled headers are turned on.

{
  'conditions': [
      ['OS=="win" and chromium_win_pch==1', {
          'target_defaults': {
              'msvs_precompiled_header': '<(DEPTH)/third_party/WebKit/Source/core/win/Precompile-core.h',
              'msvs_precompiled_source': '<(DEPTH)/third_party/WebKit/Source/core/win/Precompile-core.cpp',
              'sources': ['<(DEPTH)/third_party/WebKit/Source/core/win/Precompile-core.cpp'],
              'msvs_settings': {
                  # Compiling the large headers in Blink hits the
                  # default size limits in precompile header
                  # compilation in VS2013. Here we increase the limits
                  # to 120% of default.
                  'VCCLCompilerTool': {
                      'AdditionalOptions': [
                          '-Zm120',
                      ],
                  },
              },
          }
      }],
  ],
}
