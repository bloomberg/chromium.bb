# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Default identity. Can be overridden to allow making custom
    # plugins based on O3D, but they will not work with regular O3D apps.
    'plugin_name%': 'O3D Plugin',
    'plugin_npapi_filename%': 'npo3dautoplugin',
    'plugin_npapi_mimetype%': 'application/vnd.o3d.auto',
    'plugin_activex_hostcontrol_clsid%': '9666A772-407E-4F90-BC37-982E8160EB2D',
    'plugin_activex_typelib_clsid%': 'D4F6E31C-E952-48FE-9833-6AE308BD79C6',
    'plugin_activex_hostcontrol_name%': 'o3d_host',
    'plugin_activex_typelib_name%': 'npapi_host2',
    'plugin_installdir_csidl%': 'CSIDL_APPDATA',
    'plugin_vendor_directory%': 'Google',
    'plugin_product_directory%': 'O3D',
    'plugin_extras_directory%': 'O3DExtras',
    # You must set this to 1 if changing any of the above.
    'plugin_rebranded%': '0',
  }
}
