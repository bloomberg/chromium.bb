# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # OpenMAX IL level of API.
      'target_name': 'il',
      'type': 'none',
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
        'link_settings': {
          'libraries': [
            '-lOmxCore',
            # We need dl for dlopen() and friends.
            '-ldl',
          ],
        },
      },
    },
 ],
}
