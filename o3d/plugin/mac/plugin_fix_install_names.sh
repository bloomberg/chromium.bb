#!/bin/sh

# Make plugin look in its private Frameworks directory for these frameworks.


# Cg.framework
/usr/bin/install_name_tool -change \
  @executable_path/../Library/Frameworks/Cg.framework/Cg \
  @loader_path/../Frameworks/Cg.framework/Cg \
  "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"
