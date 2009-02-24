{
  'variables': {
    'depth': '..',
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:*',
        '../chrome/chrome.gyp:*',
        '../googleurl/build/googleurl.gyp:*',
        '../media/media.gyp:*',
        '../net/net.gyp:*',
        '../sdch/sdch.gyp:*',
        '../skia/skia.gyp:*',
        '../testing/gtest.gyp:*',
        '../third_party/bzip2/bzip2.gyp:*',
        '../third_party/icu38/icu38.gyp:*',
        '../third_party/libevent/libevent.gyp:*',
        '../third_party/libjpeg/libjpeg.gyp:*',
        '../third_party/libpng/libpng.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/libxslt/libxslt.gyp:*',
        '../third_party/modp_b64/modp_b64.gyp:*',
        '../third_party/npapi/npapi.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
        '../webkit/tools/test_shell/test_shell.gyp:*',
        '../webkit/webkit.gyp:*',
        '../v8/v8.gyp:*',
      ],
    },
  ],
}
