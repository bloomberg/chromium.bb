#!/bin/sh

# Copy stripped frameworks inside the plugin.

ditto --arch i386 \
  "${PROJECT_DIR}/../../breakpad/src/client/mac/build/Release/Breakpad.framework" \
  "${BUILT_PRODUCTS_DIR}/O3D.plugin/Contents/Frameworks/Breakpad.framework"

ditto --arch i386 \
  "${PROJECT_DIR}/../../third_party/cg/files/mac/Cg.framework" \
  "${BUILT_PRODUCTS_DIR}/O3D.plugin/Contents/Frameworks/Cg.framework"

