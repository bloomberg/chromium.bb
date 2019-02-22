#!/bin/bash
# Copyright 2018 Google LLC
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

BASE_DIR=`cd $(dirname ${BASH_SOURCE[0]}) && pwd`
# This expects the environment variable EMSDK to be set
if [[ ! -d $EMSDK ]]; then
  echo "Be sure to set the EMSDK environment variable."
  exit 1
fi

BUILD_DIR=${BUILD_DIR:="out/canvaskit_wasm"}
mkdir -p $BUILD_DIR
# Navigate to SKIA_HOME from where this file is located.
pushd $BASE_DIR/../..

source $EMSDK/emsdk_env.sh
EMCC=`which emcc`
EMCXX=`which em++`

RELEASE_CONF="-Oz --closure 1 --llvm-lto 3 -DSK_RELEASE"
EXTRA_CFLAGS="\"-DSK_RELEASE\""
if [[ $@ == *debug* ]]; then
  echo "Building a Debug build"
  EXTRA_CFLAGS="\"-DSK_DEBUG\""
  RELEASE_CONF="-O0 --js-opts 0 -s SAFE_HEAP=1 -s ASSERTIONS=1 -s GL_ASSERTIONS=1 -g3 -DPATHKIT_TESTING -DSK_DEBUG"
fi

# Turn off exiting while we check for ninja (which may not be on PATH)
set +e
NINJA=`which ninja`
if [[ -z $NINJA ]]; then
  git clone "https://chromium.googlesource.com/chromium/tools/depot_tools.git" --depth 1 $BUILD_DIR/depot_tools
  NINJA=$BUILD_DIR/depot_tools/ninja
fi
# Re-enable error checking
set -e

echo "Compiling bitcode"

# Inspired by https://github.com/Zubnix/skia-wasm-port/blob/master/build_bindings.sh
./bin/gn gen ${BUILD_DIR} \
  --args="cc=\"${EMCC}\" \
  cxx=\"${EMCXX}\" \
  extra_cflags_cc=[\"-frtti\"] \
  extra_cflags=[\"-s\",\"USE_FREETYPE=1\",\"-s\",\"USE_LIBPNG=1\", \"-s\", \"WARN_UNALIGNED=1\",
    \"-DIS_WEBGL=1\", \"-DSKNX_NO_SIMD\",
    ${EXTRA_CFLAGS}
  ] \
  is_debug=false \
  is_official_build=true \
  is_component_build=false \
  target_cpu=\"wasm\" \
  \
  skia_use_angle = false \
  skia_use_dng_sdk=false \
  skia_use_egl=true \
  skia_use_expat=false \
  skia_use_fontconfig=false \
  skia_use_freetype=true \
  skia_use_icu=false \
  skia_use_libheif=false \
  skia_use_libjpeg_turbo=false \
  skia_use_libpng=true \
  skia_use_libwebp=false \
  skia_use_lua=false \
  skia_use_piex=false \
  skia_use_vulkan=false \
  skia_use_zlib=true \
  \
  skia_enable_ccpr=false \
  skia_enable_gpu=true \
  skia_enable_fontmgr_empty=false \
  skia_enable_pdf=false"

${NINJA} -C ${BUILD_DIR} libskia.a

export EMCC_CLOSURE_ARGS="--externs $BASE_DIR/externs.js "

echo "Generating final wasm"

# Skottie doesn't end up in libskia and is currently not its own library
# so we just hack in the .cpp files we need for now.
${EMCXX} \
    $RELEASE_CONF \
    -Iinclude/c \
    -Iinclude/codec \
    -Iinclude/config \
    -Iinclude/core \
    -Iinclude/effects \
    -Iinclude/gpu \
    -Iinclude/gpu/gl \
    -Iinclude/pathops \
    -Iinclude/private \
    -Iinclude/utils/ \
    -Imodules/skottie/include \
    -Imodules/sksg/include \
    -Isrc/core/ \
    -Isrc/utils/ \
    -Isrc/sfnt/ \
    -Itools/fonts \
    -Itools \
    -lEGL \
    -lGLESv2 \
    -std=c++14 \
    --bind \
    --pre-js $BASE_DIR/helper.js \
    --pre-js $BASE_DIR/interface.js \
    $BASE_DIR/canvaskit_bindings.cpp \
    $BUILD_DIR/libskia.a \
    modules/skottie/src/Skottie.cpp \
    modules/skottie/src/SkottieAdapter.cpp \
    modules/skottie/src/SkottieAnimator.cpp \
    modules/skottie/src/SkottieJson.cpp \
    modules/skottie/src/SkottieLayer.cpp \
    modules/skottie/src/SkottieLayerEffect.cpp \
    modules/skottie/src/SkottiePrecompLayer.cpp \
    modules/skottie/src/SkottieProperty.cpp \
    modules/skottie/src/SkottieShapeLayer.cpp \
    modules/skottie/src/SkottieTextLayer.cpp \
    modules/skottie/src/SkottieValue.cpp \
    modules/sksg/src/*.cpp \
    src/core/SkCubicMap.cpp \
    src/core/SkTime.cpp \
    src/pathops/SkOpBuilder.cpp \
    tools/fonts/SkTestFontMgr.cpp \
    tools/fonts/SkTestTypeface.cpp \
    src/utils/SkJSON.cpp \
    src/utils/SkParse.cpp \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORT_NAME="CanvasKitInit" \
    -s FORCE_FILESYSTEM=0 \
    -s MODULARIZE=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s STRICT=1 \
    -s TOTAL_MEMORY=32MB \
    -s USE_FREETYPE=1 \
    -s USE_LIBPNG=1 \
    -s WARN_UNALIGNED=1 \
    -s WASM=1 \
    -o $BUILD_DIR/canvaskit.js
