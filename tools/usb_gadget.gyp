# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'usb_gadget_files': [
      'usb_gadget/__init__.py',
      'usb_gadget/__main__.py',
      'usb_gadget/default_gadget.py',
      'usb_gadget/gadget.py',
      'usb_gadget/hid_constants.py',
      'usb_gadget/hid_descriptors.py',
      'usb_gadget/hid_gadget.py',
      'usb_gadget/keyboard_gadget.py',
      'usb_gadget/linux_gadgetfs.py',
      'usb_gadget/mouse_gadget.py',
      'usb_gadget/server.py',
      'usb_gadget/usb_constants.py',
      'usb_gadget/usb_descriptors.py',
    ],
    'usb_gadget_package': '<(PRODUCT_DIR)/usb_gadget.zip',
    'usb_gadget_package_hash': '<(PRODUCT_DIR)/usb_gadget.zip.md5',
  },
  'targets': [
    {
      'target_name': 'usb_gadget',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Building USB Gadget ZIP bundle',
          'inputs': [
            'usb_gadget/package.py',
            '<@(usb_gadget_files)',
          ],
          'outputs': [
            '<(usb_gadget_package)',
            '<(usb_gadget_package_hash)',
          ],
          'action': [
            'python', 'usb_gadget/package.py',
            '--zip-file', '<(usb_gadget_package)',
            '--hash-file', '<(usb_gadget_package_hash)',
            '<@(usb_gadget_files)',
          ]
        }
      ]
    }
  ]
}
