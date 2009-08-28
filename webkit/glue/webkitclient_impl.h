// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_CLIENT_IMPL_H_
#define WEBKIT_CLIENT_IMPL_H_

#include "base/platform_file.h"
#include "base/timer.h"
#include "webkit/api/public/WebKitClient.h"
#if defined(OS_WIN)
#include "webkit/glue/webthemeengine_impl_win.h"
#endif

class MessageLoop;

namespace webkit_glue {

class WebKitClientImpl : public WebKit::WebKitClient {
 public:
  WebKitClientImpl();

  // WebKitClient methods (partial implementation):
  virtual WebKit::WebThemeEngine* themeEngine();
  virtual WebKit::WebURLLoader* createURLLoader();
  virtual void getPluginList(bool refresh, WebKit::WebPluginListBuilder*);
  virtual void decrementStatsCounter(const char* name);
  virtual void incrementStatsCounter(const char* name);
  virtual void traceEventBegin(const char* name, void* id, const char* extra);
  virtual void traceEventEnd(const char* name, void* id, const char* extra);
  virtual WebKit::WebData loadResource(const char* name);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name, int numeric_value);
  virtual double currentTime();
  virtual void setSharedTimerFiredFunction(void (*func)());
  virtual void setSharedTimerFireTime(double fireTime);
  virtual void stopSharedTimer();
  virtual void callOnMainThread(void (*func)());
  virtual void suddenTerminationChanged(bool enabled) { }

  virtual base::PlatformFile databaseOpenFile(
      const WebKit::WebString& file_name, int desired_flags,
      base::PlatformFile* dir_handle);
  virtual int databaseDeleteFile(const WebKit::WebString& file_name,
      bool sync_dir);
  virtual long databaseGetFileAttributes(const WebKit::WebString& file_name);
  virtual long long databaseGetFileSize(const WebKit::WebString& file_name);

  virtual bool fileExists(const WebKit::WebString& path);
  virtual bool deleteFile(const WebKit::WebString& path);
  virtual bool deleteEmptyDirectory(const WebKit::WebString& path);
  virtual bool getFileSize(const WebKit::WebString& path, long long& result);
  virtual bool getFileModificationTime(const WebKit::WebString& path,
                                       time_t& result);
  virtual WebKit::WebString directoryName(const WebKit::WebString& path);
  virtual WebKit::WebString pathByAppendingComponent(
      const WebKit::WebString& path, const WebKit::WebString& component);
  virtual bool makeAllDirectories(const WebKit::WebString& path);

  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebApplicationCacheHostClient*);

 private:
  void DoTimeout() {
    if (shared_timer_func_)
      shared_timer_func_();
  }

  MessageLoop* main_loop_;
  base::OneShotTimer<WebKitClientImpl> shared_timer_;
  void (*shared_timer_func_)();

#if defined(OS_WIN)
  WebThemeEngineImpl theme_engine_;
#endif
};

}  // namespace webkit_glue

#endif  // WEBKIT_CLIENT_IMPL_H_
