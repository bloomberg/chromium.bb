# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'es_util',
      'type': 'static_library',
      'dependencies': [
        '../../gpu/gpu.gyp:gles2_c_lib_nocheck',
      ],
      'include_dirs': [
        'Common/Include',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'Common/Include',
        ],
      },
      'sources': [
        'Common/Include/esUtil.h',
        'Common/Source/esShader.c',
        'Common/Source/esShapes.c',
        'Common/Source/esTransform.c',
        'Common/Source/esUtil.c',
      ],
    },
    {
      'target_name': 'hello_triangle',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_2/Hello_Triangle/Hello_Triangle.c',
        'Chapter_2/Hello_Triangle/Hello_Triangle.h',
      ],
    },
    {
      'target_name': 'mip_map_2d',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_9/MipMap2D/MipMap2D.c',
        'Chapter_9/MipMap2D/MipMap2D.h',
      ],
    },
    {
      'target_name': 'simple_texture_2d',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_9/Simple_Texture2D/Simple_Texture2D.c',
        'Chapter_9/Simple_Texture2D/Simple_Texture2D.h',
      ],
    },
    {
      'target_name': 'simple_texture_cubemap',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.c',
        'Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.h',
      ],
    },
    {
      'target_name': 'simple_vertex_shader',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_8/Simple_VertexShader/Simple_VertexShader.c',
        'Chapter_8/Simple_VertexShader/Simple_VertexShader.h',
      ],
    },
    {
      'target_name': 'stencil_test',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_11/Stencil_Test/Stencil_Test.c',
        'Chapter_11/Stencil_Test/Stencil_Test.h',
      ],
    },
    {
      'target_name': 'texture_wrap',
      'type': 'static_library',
      'dependencies': [
        'es_util',
      ],
      'sources': [
        'Chapter_9/TextureWrap/TextureWrap.c',
        'Chapter_9/TextureWrap/TextureWrap.h',
      ],
    },
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
