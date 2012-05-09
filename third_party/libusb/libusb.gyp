# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_libusb%': 0,
    'required_libusb_version': '1.0.0',
  },
  'targets': [
    {
      'target_name': 'libusb',
      'conditions': [
        ['OS == "linux" and use_system_libusb', {
          'type': 'none',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_LIBUSB',
            ],
            'cflags': [
              '<!@(pkg-config --atleast-version=<(required_libusb_version) libusb-1.0)',
              '<!@(pkg-config --cflags libusb-1.0)',
            ],
            'link_settings': {
              'ldflags': [
                '<!@(pkg-config --libs-only-L --libs-only-other libusb-1.0)',
              ],
              'libraries': [
                '<!@(pkg-config --libs-only-l libusb-1.0)',
              ],
            },
          },
        }, {
          'type': '<(library)',
          'sources': [
            'src/libusb/core.c',
            'src/libusb/descriptor.c',
            'src/libusb/io.c',
            'src/libusb/sync.c',
          ],
          'include_dirs': [
            'src',
            'src/libusb',
            'src/libusb/os',
          ],
          'conditions': [
            [ 'OS == "linux"', {
              'sources': [
                'src/libusb/os/linux_usbfs.c',
                'src/libusb/os/threads_posix.c',
              ],
              'defines': [
                'DEFAULT_VISIBILITY=',
                'HAVE_POLL_H=1',
                'HAVE_SYS_TIME_H=1',
                'OS_LINUX=1',
                'POLL_NFDS_TYPE=nfds_t',
                'THREADS_POSIX=1',
                '_GNU_SOURCE=1',
              ],
            }],
            ['OS == "win"', {
              'sources': [
                'src/libusb/os/poll_windows.c',
                'src/libusb/os/threads_windows.c',
                'src/libusb/os/windows_usb.c',
              ],
              'include_dirs!': [
                'src',
              ],
              'include_dirs': [
                'src/msvc',
              ],
            }],
            ['OS == "mac"', {
              'sources': [
                'src/libusb/os/darwin_usb.c',
                'src/libusb/os/threads_posix.c',
              ],
              'defines': [
                'DEFAULT_VISIBILITY=',
                'HAVE_POLL_H=1',
                'HAVE_SYS_TIME_H=1',
                'OS_DARWIN=1',
                'POLL_NFDS_TYPE=nfds_t',
                'THREADS_POSIX=1',
                '_GNU_SOURCE=1',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
