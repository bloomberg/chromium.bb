// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBFILEUTILITIES_IMPL_H_
#define WEBFILEUTILITIES_IMPL_H_

#include "base/platform_file.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebFileUtilities.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WEBKIT_GLUE_EXPORT WebFileUtilitiesImpl :
    NON_EXPORTED_BASE(public WebKit::WebFileUtilities) {
 public:
  WebFileUtilitiesImpl();
  virtual ~WebFileUtilitiesImpl();

  // WebFileUtilities methods:
  virtual bool getFileInfo(
      const WebKit::WebString& path,
      WebKit::WebFileInfo& result);
  virtual WebKit::WebString directoryName(const WebKit::WebString& path);
  virtual WebKit::WebString baseName(const WebKit::WebString& path);
  virtual WebKit::WebURL filePathToURL(const WebKit::WebString& path);
  virtual base::PlatformFile openFile(const WebKit::WebString& path, int mode);
  virtual void closeFile(base::PlatformFile& handle);
  virtual int readFromFile(base::PlatformFile handle, char* data, int length);

  void set_sandbox_enabled(bool sandbox_enabled) {
    sandbox_enabled_ = sandbox_enabled;
  }

 protected:
  bool sandbox_enabled_;
};

}  // namespace webkit_glue

#endif  // WEBFILEUTILITIES_IMPL_H_
