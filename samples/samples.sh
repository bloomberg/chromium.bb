#!/bin/sh

# chrome2samples and samples2chrome source this file to obtain the list of
# sample files.

GYPS="
    base/base.gyp
    googleurl/build/googleurl.gyp
    net/net.gyp
    sdch/sdch.gyp
    skia/skia.gyp
    testing/gtest.gyp
    third_party/bzip2/bzip2.gyp
    third_party/icu38/icu38.gyp
    third_party/libevent/libevent.gyp
    third_party/libjpeg/libjpeg.gyp
    third_party/libpng/libpng.gyp
    third_party/modp_b64/modp_b64.gyp
    third_party/zlib/zlib.gyp
    "
