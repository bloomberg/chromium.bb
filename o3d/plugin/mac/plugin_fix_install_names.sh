#!/bin/sh

# Make plugin look in its private Frameworks directory for these frameworks.

# First take care of Breakpad.framework
/usr/bin/install_name_tool -change \
  @executable_path/../Frameworks/Breakpad.framework/Versions/A/Breakpad \
  @loader_path/../Frameworks/Breakpad.framework/Versions/A/Breakpad \
  "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"

# Cg.framework
/usr/bin/install_name_tool -change \
  @executable_path/../Library/Frameworks/Cg.framework/Cg \
  @loader_path/../Frameworks/Cg.framework/Cg \
  "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"
