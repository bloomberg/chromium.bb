#!/bin/sh

# Copy stripped frameworks inside the plugin.

ditto --arch i386 \
  "${PROJECT_DIR}/../../third_party/cg/files/mac/Cg.framework" \
  "${BUILT_PRODUCTS_DIR}/O3D.plugin/Contents/Frameworks/Cg.framework"

ditto --arch i386 \
  "${BUILT_PRODUCTS_DIR}/crash_report_sender.app" \
  "${BUILT_PRODUCTS_DIR}/O3D.plugin/Contents/Resources/crash_report_sender.app"


