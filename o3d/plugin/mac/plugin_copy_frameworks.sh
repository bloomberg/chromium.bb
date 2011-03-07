#!/bin/sh

# Copy stripped frameworks inside the plugin.

PLUGIN_NPAPI_FILENAME=$1

ditto --arch i386 \
  "${PROJECT_DIR}/../../third_party/cg/files/mac/Cg.framework" \
  "${BUILT_PRODUCTS_DIR}/${PLUGIN_NPAPI_FILENAME}.plugin/Contents/Frameworks/Cg.framework"

# Delete the .svn directories
find "${BUILT_PRODUCTS_DIR}/${PLUGIN_NPAPI_FILENAME}.plugin/Contents/Frameworks/Cg.framework" -type d -name .svn -print0 | xargs -0 rm -r

ditto --arch i386 \
  "${BUILT_PRODUCTS_DIR}/crash_report_sender.app" \
  "${BUILT_PRODUCTS_DIR}/${PLUGIN_NPAPI_FILENAME}.plugin/Contents/Resources/crash_report_sender.app"

ditto --arch i386 \
  "${BUILT_PRODUCTS_DIR}/crash_inspector" \
  "${BUILT_PRODUCTS_DIR}/${PLUGIN_NPAPI_FILENAME}.plugin/Contents/Resources/crash_inspector"
