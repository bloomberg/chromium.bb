# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is used to build parts of dbus-c++ in Chromium on Linux.
# TODO(thestig) Add support for system dbus-c++ in the future if it becomes
# compatible with the CrOS fork.
{
  'targets': [
    {
      'target_name': 'dbus_cplusplus',
      'type': 'shared_library',
      'dependencies': [
        '../../build/linux/system.gyp:dbus',
        '../../build/linux/system.gyp:glib',
      ],
      'sources': [
        'source/src/connection.cpp',
        'source/src/connection_p.h',
        'source/src/debug.cpp',
        'source/src/dispatcher.cpp',
        'source/src/dispatcher_p.h',
        'source/src/error.cpp',
        'source/src/eventloop-integration.cpp',
        'source/src/eventloop.cpp',
        'source/src/glib-integration.cpp',
        'source/src/interface.cpp',
        'source/src/internalerror.h',
        'source/src/introspection.cpp',
        'source/src/message.cpp',
        'source/src/message_p.h',
        'source/src/object.cpp',
        'source/src/pendingcall.cpp',
        'source/src/pendingcall_p.h',
        'source/src/property.cpp',
        'source/src/server.cpp',
        'source/src/server_p.h',
        'source/src/types.cpp',
      ],
      'cflags!': [
        '-fno-exceptions',
      ],
      'defines': [
        'DBUS_HAS_RECURSIVE_MUTEX',
        'DBUS_HAS_THREADS_INIT_DEFAULT',
        'GCC_HASCLASSVISIBILITY',
      ],
      'direct_dependent_settings': {
        'cflags!': [
          '-fno-exceptions',
        ],
        'defines': [
          'DBUS_HAS_RECURSIVE_MUTEX',
          'DBUS_HAS_THREADS_INIT_DEFAULT',
          'GCC_HASCLASSVISIBILITY',
        ],
        'include_dirs': [
          'source/include',
        ],
      },
      'include_dirs': [
        'source/include',
      ],
    },
  ],
}
