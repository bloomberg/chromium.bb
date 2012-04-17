# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libusb',
      'type': '<(library)',
      'sources': [
        'libusb/core.c',
        'libusb/descriptor.c',
        'libusb/io.c',
        'libusb/sync.c',
      ],
      'include_dirs': [
        '.',
        'libusb',
        'libusb/os',
      ],
      'conditions': [
        [ 'OS == "linux"', {
          'sources': [
            'libusb/os/linux_usbfs.c',
            'libusb/os/threads_posix.c',
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
        [ 'OS == "win"', {
          'sources': [
            'libusb/os/poll_windows.c',
            'libusb/os/threads_windows.c',
            'libusb/os/windows_usb.c',
          ],
          'include_dirs!': [
            '.',
          ],
          'include_dirs': [
            'msvc',
          ],
        }],
        [ 'OS == "mac"', {
          'sources': [
            'libusb/os/darwin_usb.c',
            'libusb/os/threads_posix.c',
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
    },
  ],
}
