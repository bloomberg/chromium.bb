# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': {
    # TODO(yaar) Include upstream (webkit.org) features.gypi here, so
    # that this file inherits upstream feature_defines.
  },
  'variables': {
    'feature_defines': [
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_DATABASE=1',
      'ENABLE_DATAGRID=0',
      'ENABLE_OFFLINE_WEB_APPLICATIONS=1',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DOM_STORAGE=1',
      'ENABLE_JAVASCRIPT_DEBUGGER=0',
      'ENABLE_JSC_MULTIPLE_THREADS=0',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_NOTIFICATIONS=0',
      'ENABLE_XSLT=1',
      'ENABLE_XPATH=1',
      'ENABLE_SHARED_WORKERS=0',
      'ENABLE_SVG=1',
      'ENABLE_SVG_ANIMATION=1',
      'ENABLE_SVG_AS_IMAGE=1',
      'ENABLE_SVG_USE=1',
      'ENABLE_SVG_FOREIGN_OBJECT=1',
      'ENABLE_SVG_FONTS=1',
      'ENABLE_VIDEO=1',
      'ENABLE_WORKERS=1',
    ],
  },
}
