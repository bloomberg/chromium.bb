// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLATFORM_SUPPORT_IMPL_H_
#define WEBKIT_PLATFORM_SUPPORT_IMPL_H_

#include "base/compiler_specific.h"
#include "base/platform_file.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webkit_glue_export.h"

#if defined(OS_WIN)
#include "webkit/glue/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "webkit/glue/webthemeengine_impl_mac.h"
#elif defined(OS_POSIX)
#include "webkit/glue/webthemeengine_impl_linux.h"
#endif


class MessageLoop;

namespace webkit {
struct WebPluginInfo;
}

namespace WebKit {
class WebSocketStreamHandle;
}

namespace webkit_glue {

class WebSocketStreamHandleDelegate;
class WebSocketStreamHandleBridge;

class WEBKIT_GLUE_EXPORT WebKitPlatformSupportImpl :
    NON_EXPORTED_BASE(public WebKit::WebKitPlatformSupport) {
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
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* category_name);
  virtual int addTraceEvent(
      char phase,
      const unsigned char* category_enabled,
      const char* name,
      unsigned long long id,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const unsigned long long* arg_values,
      int threshold_begin_id,
      long long threshold,
      unsigned char flags);
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
  virtual WebKit::WebThread* currentThread();


  // Embedder functions. The following are not implemented by the glue layer and
  // need to be specialized by the embedder.

  // Gets a localized string given a message id.  Returns an empty string if the
  // message id is not found.
  virtual string16 GetLocalizedString(int message_id) = 0;

  // Returns the raw data for a resource.  This resource must have been
  // specified as BINDATA in the relevant .rc file.
  virtual base::StringPiece GetDataResource(int resource_id) = 0;

  // Returns the raw data for an image resource with a scale factor as close as
  // is available to |scale_factor|.  This resource must have been specified as
  // BINDATA in the relevant .rc file.
  virtual base::StringPiece GetImageResource(int resource_id,
                                             float scale_factor) = 0;

  // Returns the list of plugins.
  virtual void GetPlugins(bool refresh,
                          std::vector<webkit::WebPluginInfo>* plugins) = 0;
  // Creates a ResourceLoaderBridge.
  virtual ResourceLoaderBridge* CreateResourceLoader(
      const ResourceLoaderBridge::RequestInfo& request_info) = 0;
  // Creates a WebSocketStreamHandleBridge.
  virtual WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle* handle,
      WebSocketStreamHandleDelegate* delegate) = 0;

  void SuspendSharedTimer();
  void ResumeSharedTimer();
  virtual void OnStartSharedTimer(base::TimeDelta delay) {}

  virtual void didStartWorkerRunLoop(
      const WebKit::WebWorkerRunLoop& runLoop) OVERRIDE;
  virtual void didStopWorkerRunLoop(
      const WebKit::WebWorkerRunLoop& runLoop) OVERRIDE;

 private:
  void DoTimeout() {
    if (shared_timer_func_ && !shared_timer_suspended_)
      shared_timer_func_();
  }
  static void DestroyCurrentThread(void*);

  MessageLoop* main_loop_;
  base::OneShotTimer<WebKitPlatformSupportImpl> shared_timer_;
  void (*shared_timer_func_)();
  double shared_timer_fire_time_;
  int shared_timer_suspended_;  // counter
  WebThemeEngineImpl theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_PLATFORM_SUPPORT_IMPL_H_
