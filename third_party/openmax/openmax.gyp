# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'openmax_type%': 'stub',
  },
  'targets': [
    {
      # OpenMAX IL level of API.
      'target_name': 'il',
      'sources': [
        'il/OMX_Audio.h',
        'il/OMX_Component.h',
        'il/OMX_ContentPipe.h',
        'il/OMX_Core.h',
        'il/OMX_Image.h',
        'il/OMX_Index.h',
        'il/OMX_IVCommon.h',
        'il/OMX_Other.h',
        'il/OMX_Types.h',
        'il/OMX_Video.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'il',
        ],
      },
      'conditions': [
        ['openmax_type=="stub"', {
          'type': '<(library)',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'sources': [
            'omx_stub.cc',
          ],
          'include_dirs': [
            'il',
          ],
          'defines': [
            '__OMX_EXPORTS',
          ],
          'direct_dependent_settings': {
            'defines': [
              '__OMX_EXPORTS',
            ],
          },
        }],
        ['openmax_type=="bellagio"', {
          'type': 'none',
          'direct_dependent_settings': {
            'link_settings': {
              'libraries': [
                '-lomxil-bellagio',
              ],
            },
          },
        }],
        ['openmax_type=="omxcore"', {
          'type': 'none',
          'direct_dependent_settings': {
            'link_settings': {
              'libraries': [
                '-lOmxCore',
              ],
            },
          },
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
