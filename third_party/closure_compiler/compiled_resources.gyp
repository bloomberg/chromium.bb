# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Add your directory-specific .gyp file to this list for it to be continuously
# typechecked on the builder:
# http://build.chromium.org/p/chromium.fyi/builders/Closure%20Compilation%20Linux
#
# Also, see our guide to Closure compilation in chrome:
# https://code.google.com/p/chromium/wiki/ClosureCompilation
{
  'targets': [
    {
      'target_name': 'compile_all_resources',
      'type': 'none',
      'dependencies': [
        '../../chrome/browser/resources/bookmark_manager/js/compiled_resources.gyp:*',
        '../../chrome/browser/resources/chromeos/braille_ime/compiled_resources.gyp:*',
        '../../chrome/browser/resources/chromeos/compiled_resources.gyp:*',
        '../../chrome/browser/resources/chromeos/network_ui/compiled_resources.gyp:*',
        '../../chrome/browser/resources/downloads/compiled_resources.gyp:*',
        '../../chrome/browser/resources/extensions/compiled_resources.gyp:*',
        '../../chrome/browser/resources/help/compiled_resources.gyp:*',
        '../../chrome/browser/resources/history/compiled_resources.gyp:*',
        '../../chrome/browser/resources/options/compiled_resources.gyp:*',
        '../../chrome/browser/resources/media_router/compiled_resources.gyp:*',
        '../../chrome/browser/resources/md_downloads/compiled_resources.gyp:*',
        '../../chrome/browser/resources/md_extensions/compiled_resources.gyp:*',
        '../../chrome/browser/resources/ntp4/compiled_resources.gyp:*',
        '../../chrome/browser/resources/settings/compiled_resources.gyp:*',
        '../../chrome/browser/resources/uber/compiled_resources.gyp:*',
        '../../remoting/app_remoting_webapp_compile.gypi:*',
        '../../remoting/remoting_webapp_compile.gypi:*',
        '../../ui/file_manager/audio_player/js/compiled_resources.gyp:*',
        '../../ui/file_manager/file_manager/background/js/compiled_resources.gyp:*',
        '../../ui/file_manager/file_manager/foreground/js/compiled_resources.gyp:*',
        '../../ui/file_manager/gallery/js/compiled_resources.gyp:*',
        '../../ui/file_manager/image_loader/compiled_resources.gyp:*',
        '../../ui/file_manager/video_player/js/compiled_resources.gyp:*',
        '../../ui/webui/resources/cr_elements/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/chromeos/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/chromeos/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/compiled_resources.gyp:*',
        '../../ui/webui/resources/js/cr/ui/compiled_resources.gyp:*',
      ],
    },
  ]
}
