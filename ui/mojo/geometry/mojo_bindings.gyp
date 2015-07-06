# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //ui/mojo/geometry
      'target_name': 'mojo_geometry_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'geometry.mojom',
        ],
      },
      'includes': [ '../../../third_party/mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'dependencies': [
        'mojo_geometry_bindings_mojom',
        '../../../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
  ],
}
