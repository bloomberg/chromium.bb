// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLATFORM_SUPPORT_IMPL_H_
#define WEBKIT_PLATFORM_SUPPORT_IMPL_H_

#include "base/platform_file.h"
#include "base/timer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitPlatformSupport.h"
#if defined(OS_WIN)
#include "webkit/glue/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "webkit/glue/webthemeengine_impl_mac.h"
#elif defined(OS_POSIX)
#include "webkit/glue/webthemeengine_impl_linux.h"
#endif


class MessageLoop;

namespace webkit_glue {

class WebKitPlatformSupportImpl : public WebKit::WebKitPlatformSupport {
 public:
  WebKitPlatformSupportImpl();
  virtual ~WebKitPlatformSupportImpl();

  // WebKitPlatformSupport methods (partial implementation):
  virtual WebKit::WebThemeEngine* themeEngine();

  virtual base::PlatformFile databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const WebKit::WebString& origin_identifier);
  virtual WebKit::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index, const WebKit::WebString& challenge,
      const WebKit::WebURL& url);
  virtual size_t memoryUsageMB();
  virtual size_t actualMemoryUsageMB();
  virtual WebKit::WebURLLoader* createURLLoader();
  virtual WebKit::WebSocketStreamHandle* createSocketStreamHandle();
  virtual WebKit::WebString userAgent(const WebKit::WebURL& url);
  virtual void getPluginList(bool refresh, WebKit::WebPluginListBuilder*);
  virtual void decrementStatsCounter(const char* name);
  virtual void incrementStatsCounter(const char* name);
  virtual void histogramCustomCounts(
    const char* name, int sample, int min, int max, int bucket_count);
  virtual void histogramEnumeration(
    const char* name, int sample, int boundary_value);
  virtual void traceEventBegin(const char* name, void* id, const char* extra);
  virtual void traceEventEnd(const char* name, void* id, const char* extra);
  virtual WebKit::WebData loadResource(const char* name);
  virtual bool loadAudioResource(
      WebKit::WebAudioBus* destination_bus, const char* audio_file_data,
      size_t data_size, double sample_rate);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name, int numeric_value);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name, const WebKit::WebString& value);
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value1, const WebKit::WebString& value2);
  virtual void suddenTerminationChanged(bool enabled) { }
  virtual double currentTime();
  virtual double monotonicallyIncreasingTime();
  virtual void cryptographicallyRandomValues(
      unsigned char* buffer, size_t length);
  virtual void setSharedTimerFiredFunction(void (*func)());
  virtual void setSharedTimerFireInterval(double interval_seconds);
  virtual void stopSharedTimer();
  virtual void callOnMainThread(void (*func)(void*), void* context);
  virtual WebKit::WebThread* createThread(const char* name);

  void SuspendSharedTimer();
  void ResumeSharedTimer();

 private:
  void DoTimeout() {
    if (shared_timer_func_ && !shared_timer_suspended_)
      shared_timer_func_();
  }

  MessageLoop* main_loop_;
  base::OneShotTimer<WebKitPlatformSupportImpl> shared_timer_;
  void (*shared_timer_func_)();
  double shared_timer_fire_time_;
  int shared_timer_suspended_;  // counter
  WebThemeEngineImpl theme_engine_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_PLATFORM_SUPPORT_IMPL_H_
