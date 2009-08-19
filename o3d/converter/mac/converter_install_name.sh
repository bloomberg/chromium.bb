#!/bin/sh


/usr/bin/install_name_tool -change \
  @executable_path/../Library/Frameworks/Cg.framework/Cg \
  @executable_path/Cg.framework/Cg \
  "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"
