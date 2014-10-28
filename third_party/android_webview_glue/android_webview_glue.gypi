# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'system_webview_apk',
      'variables': {
        'apk_name': 'SystemWebView',
      },
      'includes': [ 'android_webview_glue_common.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/android_webview_assets',
          'files': [],
          'conditions': [
            ['icu_use_data_file_flag==1', {
              'files': [
                '<(PRODUCT_DIR)/icudtl.dat',
              ],
            }],
            ['v8_use_external_startup_data==1', {
              'files': [
                '<(PRODUCT_DIR)/natives_blob.bin',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
          ],
        },
      ],
    },
  ],
}

