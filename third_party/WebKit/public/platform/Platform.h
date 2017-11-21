/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Platform_h
#define Platform_h

#ifdef WIN32
#include <windows.h>
#endif

#include <memory>

#include "BlameContext.h"
#include "UserMetricsAction.h"
#include "WebAudioDevice.h"
#include "WebCommon.h"
#include "WebData.h"
#include "WebDataConsumerHandle.h"
#include "WebGamepadListener.h"
#include "WebGestureDevice.h"
#include "WebLocalizedString.h"
#include "WebPlatformEventType.h"
#include "WebSize.h"
#include "WebSpeechSynthesizer.h"
#include "WebStorageQuotaCallbacks.h"
#include "WebStorageQuotaType.h"
#include "WebString.h"
#include "WebURLError.h"
#include "WebURLLoader.h"
#include "WebURLLoaderFactory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {
class Gamepads;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace service_manager {
class Connector;
}

namespace v8 {
class Context;
template <class T>
class Local;
}

namespace blink {

class InterfaceProvider;
class WebAudioBus;
class WebAudioLatencyHint;
class WebBlobRegistry;
class WebCanvasCaptureHandler;
class WebClipboard;
class WebCompositorSupport;
class WebCookieJar;
class WebCrypto;
class WebDatabaseObserver;
class WebPlatformEventListener;
class WebFallbackThemeEngine;
class WebFileSystem;
class WebFileUtilities;
class WebGestureCurve;
class WebGraphicsContext3DProvider;
class WebIDBFactory;
class WebImageCaptureFrameGrabber;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMediaCapabilitiesClient;
class WebMediaPlayer;
class WebMediaRecorderHandler;
class WebMediaStream;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebMediaStreamTrack;
class WebNotificationManager;
class WebPluginListBuilder;
class WebPrescientNetworking;
class WebPublicSuffixList;
class WebPushProvider;
class WebRTCCertificateGenerator;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebSandboxSupport;
class WebScrollbarBehavior;
class WebSecurityOrigin;
class WebServiceWorkerCacheStorage;
class WebSocketHandshakeThrottle;
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
class WebStorageNamespace;
class WebSyncProvider;
struct WebFloatPoint;
class WebTaskRunner;
class WebThemeEngine;
class WebThread;
class WebTrialTokenValidator;
class WebURLLoaderMockFactory;
class WebURLResponse;
class WebURLResponse;
struct WebSize;

class BLINK_PLATFORM_EXPORT Platform {
 public:
// HTML5 Database ------------------------------------------------------

#ifdef WIN32
  typedef HANDLE FileHandle;
#else
  typedef int FileHandle;
#endif

  // Initialize platform and wtf. If you need to initialize the entire Blink,
  // you should use blink::Initialize.
  static void Initialize(Platform*);
  static Platform* Current();

  // Used to switch the current platform only for testing.
  // You should not pass in a Platform object that is not fully instantiated.
  static void SetCurrentPlatformForTesting(Platform*);

  // May return null.
  virtual WebCookieJar* CookieJar() { return nullptr; }

  // Must return non-null.
  virtual WebClipboard* Clipboard() { return nullptr; }

  // Must return non-null.
  virtual WebFileUtilities* GetFileUtilities() { return nullptr; }

  // May return null if sandbox support is not necessary
  virtual WebSandboxSupport* GetSandboxSupport() { return nullptr; }

  // May return null on some platforms.
  virtual WebThemeEngine* ThemeEngine() { return nullptr; }

  virtual WebFallbackThemeEngine* FallbackThemeEngine() { return nullptr; }

  // May return null.
  virtual std::unique_ptr<WebSpeechSynthesizer> CreateSpeechSynthesizer(
      WebSpeechSynthesizerClient*) {
    return nullptr;
  }

  // Audio --------------------------------------------------------------

  virtual double AudioHardwareSampleRate() { return 0; }
  virtual size_t AudioHardwareBufferSize() { return 0; }
  virtual unsigned AudioHardwareOutputChannels() { return 0; }

  // Creates a device for audio I/O.
  // Pass in (number_of_input_channels > 0) if live/local audio input is
  // desired.
  virtual std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      unsigned number_of_input_channels,
      unsigned number_of_channels,
      const WebAudioLatencyHint& latency_hint,
      WebAudioDevice::RenderCallback*,
      const WebString& device_id,
      const WebSecurityOrigin&) {
    return nullptr;
  }

  // MIDI ----------------------------------------------------------------

  // Creates a platform dependent WebMIDIAccessor. MIDIAccessor under platform
  // creates and owns it.
  virtual std::unique_ptr<WebMIDIAccessor> CreateMIDIAccessor(
      WebMIDIAccessorClient*);

  // Blob ----------------------------------------------------------------

  // Must return non-null.
  virtual WebBlobRegistry* GetBlobRegistry() { return nullptr; }

  // Database ------------------------------------------------------------

  // Opens a database file.
  virtual FileHandle DatabaseOpenFile(const WebString& vfs_file_name,
                                      int desired_flags) {
    return FileHandle();
  }

  // Deletes a database file and returns the error code.
  virtual int DatabaseDeleteFile(const WebString& vfs_file_name,
                                 bool sync_dir) {
    return 0;
  }

  // Returns the attributes of the given database file.
  virtual long DatabaseGetFileAttributes(const WebString& vfs_file_name) {
    return 0;
  }

  // Returns the size of the given database file.
  virtual long long DatabaseGetFileSize(const WebString& vfs_file_name) {
    return 0;
  }

  // Returns the space available for the given origin.
  virtual long long DatabaseGetSpaceAvailableForOrigin(
      const WebSecurityOrigin& origin) {
    return 0;
  }

  // Set the size of the given database file.
  virtual bool DatabaseSetFileSize(const WebString& vfs_file_name,
                                   long long size) {
    return false;
  }

  // Return a filename-friendly identifier for an origin.
  virtual WebString DatabaseCreateOriginIdentifier(
      const WebSecurityOrigin& origin) {
    return WebString();
  }

  // DOM Storage --------------------------------------------------

  // Return a LocalStorage namespace
  virtual std::unique_ptr<WebStorageNamespace> CreateLocalStorageNamespace();

  // Return a SessionStorage namespace
  virtual std::unique_ptr<WebStorageNamespace> CreateSessionStorageNamespace(
      int64_t namespace_id);

  // FileSystem ----------------------------------------------------------

  // Must return non-null.
  virtual WebFileSystem* FileSystem() { return nullptr; }

  // Return a filename-friendly identifier for an origin.
  virtual WebString FileSystemCreateOriginIdentifier(
      const WebSecurityOrigin& origin) {
    return WebString();
  }

  // IDN conversion ------------------------------------------------------

  virtual WebString ConvertIDNToUnicode(const WebString& host) { return host; }

  // IndexedDB ----------------------------------------------------------

  // Must return non-null.
  virtual WebIDBFactory* IdbFactory() { return nullptr; }

  // Cache Storage ----------------------------------------------------------

  virtual std::unique_ptr<WebServiceWorkerCacheStorage> CreateCacheStorage(
      const WebSecurityOrigin&);

  // Gamepad -------------------------------------------------------------

  virtual void SampleGamepads(device::Gamepads& into) {}

  // History -------------------------------------------------------------

  // Returns the hash for the given canonicalized URL for use in visited
  // link coloring.
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) {
    return 0;
  }

  // Returns whether the given link hash is in the user's history. The
  // hash must have been generated by calling VisitedLinkHash().
  virtual bool IsLinkVisited(unsigned long long link_hash) { return false; }

  // Same as above, but always returns actual value, without any caches.
  virtual size_t ActualMemoryUsageMB() { return 0; }

  // Return the number of of processors of the current machine.
  virtual size_t NumberOfProcessors() { return 0; }

  static const size_t kNoDecodedImageByteLimit = static_cast<size_t>(-1);

  // Returns the maximum amount of memory a decoded image should be allowed.
  // See comments on ImageDecoder::max_decoded_bytes_.
  virtual size_t MaxDecodedImageBytes() { return kNoDecodedImageByteLimit; }

  // Returns true if this is a low-end device.
  // This is the same as base::SysInfo::IsLowEndDevice.
  virtual bool IsLowEndDevice() { return false; }

  // Process -------------------------------------------------------------

  // Returns a unique identifier for a process. This may not necessarily be
  // the process's process ID.
  virtual uint32_t GetUniqueIdForProcess() { return 0; }

  // Returns a unique FrameSinkID for the current renderer process
  virtual viz::FrameSinkId GenerateFrameSinkId() { return viz::FrameSinkId(); }

  // Network -------------------------------------------------------------

  // Returns the platform's default URLLoaderFactory. It is expected that the
  // returned value is stored and to be used for all the CreateURLLoader
  // requests for the same loading context.
  // TODO(kinuko): See if we can deprecate this too.
  virtual std::unique_ptr<WebURLLoaderFactory> CreateDefaultURLLoaderFactory() {
    return nullptr;
  }

  // Returns a WebDataConsumerHandle for given a mojo data pipe endpoint.
  virtual std::unique_ptr<WebDataConsumerHandle> CreateDataConsumerHandle(
      mojo::ScopedDataPipeConsumerHandle handle) {
    return nullptr;
  }

  // May return null.
  virtual WebPrescientNetworking* PrescientNetworking() { return nullptr; }

  // Returns the User-Agent string.
  virtual WebString UserAgent() { return WebString(); }

  // A suggestion to cache this metadata in association with this URL.
  virtual void CacheMetadata(const WebURL&,
                             base::Time response_time,
                             const char* data,
                             size_t data_size) {}

  // A suggestion to cache this metadata in association with this URL which
  // resource is in CacheStorage.
  virtual void CacheMetadataInCacheStorage(
      const WebURL&,
      base::Time response_time,
      const char* data,
      size_t data_size,
      const blink::WebSecurityOrigin& cache_storage_origin,
      const WebString& cache_storage_cache_name) {}

  // Plugins -------------------------------------------------------------

  // If refresh is true, then cached information should not be used to
  // satisfy this call. mainFrameOrigin is used by the browser process to
  // filter plugins from the plugin list based on content settings.
  virtual void GetPluginList(bool refresh,
                             const WebSecurityOrigin& main_frame_origin,
                             WebPluginListBuilder*) {}

  // Public Suffix List --------------------------------------------------

  // May return null on some platforms.
  virtual WebPublicSuffixList* PublicSuffixList() { return nullptr; }

  // Resources -----------------------------------------------------------

  // Returns a localized string resource (with substitution parameters).
  virtual WebString QueryLocalizedString(WebLocalizedString::Name) {
    return WebString();
  }
  virtual WebString QueryLocalizedString(WebLocalizedString::Name,
                                         const WebString& parameter) {
    return WebString();
  }
  virtual WebString QueryLocalizedString(WebLocalizedString::Name,
                                         const WebString& parameter1,
                                         const WebString& parameter2) {
    return WebString();
  }

  virtual bool IsRendererSideResourceSchedulerEnabled() const { return false; }

  // Threads -------------------------------------------------------

  // Creates an embedder-defined thread.
  virtual std::unique_ptr<WebThread> CreateThread(const char* name);

  // Creates a WebAudio-specific thread with the elevated priority. Do NOT use
  // for any other purpose.
  virtual std::unique_ptr<WebThread> CreateWebAudioThread();

  // Returns an interface to the current thread. This is owned by the
  // embedder.
  virtual WebThread* CurrentThread() { return nullptr; }

  // Returns a blame context for attributing top-level work which does not
  // belong to a particular frame scope.
  virtual BlameContext* GetTopLevelBlameContext() { return nullptr; }

  // Resources -----------------------------------------------------------

  // Returns a blob of data corresponding to the named resource.
  virtual WebData GetDataResource(const char* name) { return WebData(); }

  // Decodes the in-memory audio file data and returns the linear PCM audio data
  // in the |destination_bus|.
  // Returns true on success.
  virtual bool DecodeAudioFileData(WebAudioBus* destination_bus,
                                   const char* audio_file_data,
                                   size_t data_size) {
    return false;
  }

  // Scrollbar ----------------------------------------------------------

  // Must return non-null.
  virtual WebScrollbarBehavior* ScrollbarBehavior() { return nullptr; }

  // Process lifetime management -----------------------------------------

  // Disable/Enable sudden termination on a process level. When possible, it
  // is preferable to disable sudden termination on a per-frame level via
  // WebFrameClient::SuddenTerminationDisablerChanged.
  // This method should only be called on the main thread.
  virtual void SuddenTerminationChanged(bool enabled) {}

  // Increase/decrease the process refcount. The process won't shut itself
  // down until this refcount reaches 0. The browser might still shut down the
  // renderer through fast shutdown. See SuddenTerminationChanged to disable
  // that.
  // These methods should only be called on the main thread.
  virtual void AddRefProcess() {}
  virtual void ReleaseRefProcess() {}

  // System --------------------------------------------------------------

  // Returns a value such as "en-US".
  virtual WebString DefaultLocale() { return WebString(); }

  // Returns an interface to the main thread. Can be null if blink was
  // initialized on a thread without a message loop.
  WebThread* MainThread() const;

  // Returns an interface to the compositor thread. This can be null if the
  // renderer was created with threaded rendering desabled.
  virtual WebThread* CompositorThread() const { return 0; }

  // Returns an interface to the file task runner.
  WebTaskRunner* FileTaskRunner() const;
  scoped_refptr<base::SingleThreadTaskRunner> BaseFileTaskRunner() const;

  // Returns an interface to the IO task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const {
    return nullptr;
  }

  // Testing -------------------------------------------------------------

  // Gets a pointer to URLLoaderMockFactory for testing. Will not be available
  // in production builds.
  // TODO(kinuko,toyoshim): Deprecate this one. (crbug.com/751425)
  virtual WebURLLoaderMockFactory* GetURLLoaderMockFactory() { return nullptr; }

  // Record to a RAPPOR privacy-preserving metric, see:
  // https://www.chromium.org/developers/design-documents/rappor.
  // RecordRappor records a sample string, while RecordRapporURL records the
  // eTLD+1 of a url.
  virtual void RecordRappor(const char* metric, const WebString& sample) {}
  virtual void RecordRapporURL(const char* metric, const blink::WebURL& url) {}

  // Record a UMA sequence action.  The UserMetricsAction construction must
  // be on a single line for extract_actions.py to find it.  Please see
  // that script for more details.  Intended use is:
  // RecordAction(UserMetricsAction("MyAction"))
  virtual void RecordAction(const UserMetricsAction&) {}

  typedef uint64_t WebMemoryAllocatorDumpGuid;

  // GPU ----------------------------------------------------------------
  //
  struct ContextAttributes {
    bool fail_if_major_performance_caveat = false;
    unsigned web_gl_version = 0;
    // Offscreen contexts usually share a surface for the default frame buffer
    // since they aren't rendering to it. Setting any of the following
    // attributes causes creation of a custom surface owned by the context.
    bool support_alpha = false;
    bool support_depth = false;
    bool support_antialias = false;
    bool support_stencil = false;
  };
  struct GraphicsInfo {
    unsigned vendor_id = 0;
    unsigned device_id = 0;
    unsigned process_crash_count = 0;
    unsigned reset_notification_strategy = 0;
    bool sandboxed = false;
    bool amd_switchable = false;
    bool optimus = false;
    WebString vendor_info;
    WebString renderer_info;
    WebString driver_version;
    WebString error_message;
  };
  // Returns a newly allocated and initialized offscreen context provider,
  // backed by an independent context. Returns null if the context cannot be
  // created or initialized.
  // Passing an existing provider to |share_context| will create the new context
  // in the same share group as the one passed.
  virtual std::unique_ptr<WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext3DProvider(
      const ContextAttributes&,
      const WebURL& top_document_url,
      WebGraphicsContext3DProvider* share_context,
      GraphicsInfo*);

  // Returns a newly allocated and initialized offscreen context provider,
  // backed by the process-wide shared main thread context. Returns null if
  // the context cannot be created or initialized.
  virtual std::unique_ptr<WebGraphicsContext3DProvider>
  CreateSharedOffscreenGraphicsContext3DProvider();

  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() {
    return nullptr;
  }

  virtual std::unique_ptr<viz::SharedBitmap> AllocateSharedBitmap(
      const WebSize& size) {
    return nullptr;
  }

  virtual bool IsThreadedCompositingEnabled() { return false; }
  virtual bool IsThreadedAnimationEnabled() { return true; }

  virtual WebCompositorSupport* CompositorSupport() { return nullptr; }

  // Creates a new fling animation curve instance for device |device_source|
  // with |velocity| and already scrolled |cumulative_scroll| pixels.
  virtual std::unique_ptr<WebGestureCurve> CreateFlingAnimationCurve(
      WebGestureDevice device_source,
      const WebFloatPoint& velocity,
      const WebSize& cumulative_scroll);

  // Whether the compositor is using gpu and expects gpu resources as inputs,
  // or software based resources.
  // NOTE: This function should not be called from core/ and modules/, but
  // called by platform/graphics/ is fine.
  virtual bool IsGpuCompositingDisabled() { return true; }

  // WebRTC ----------------------------------------------------------

  // Creates a WebRTCPeerConnectionHandler for RTCPeerConnection.
  // May return null if WebRTC functionality is not avaliable or if it's out of
  // resources.
  virtual std::unique_ptr<WebRTCPeerConnectionHandler>
  CreateRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient*);

  // Creates a WebMediaRecorderHandler to record MediaStreams.
  // May return null if the functionality is not available or out of resources.
  virtual std::unique_ptr<WebMediaRecorderHandler> CreateMediaRecorderHandler();

  // May return null if WebRTC functionality is not available or out of
  // resources.
  virtual std::unique_ptr<WebRTCCertificateGenerator>
  CreateRTCCertificateGenerator();

  // May return null if WebRTC functionality is not available or out of
  // resources.
  virtual std::unique_ptr<WebMediaStreamCenter> CreateMediaStreamCenter(
      WebMediaStreamCenterClient*);

  // Creates a WebCanvasCaptureHandler to capture Canvas output.
  virtual std::unique_ptr<WebCanvasCaptureHandler>
  CreateCanvasCaptureHandler(const WebSize&, double, WebMediaStreamTrack*);

  // Fills in the WebMediaStream to capture from the WebMediaPlayer identified
  // by the second parameter.
  virtual void CreateHTMLVideoElementCapturer(WebMediaStream*,
                                              WebMediaPlayer*) {}
  virtual void CreateHTMLAudioElementCapturer(WebMediaStream*,
                                              WebMediaPlayer*) {}

  // Creates a WebImageCaptureFrameGrabber to take a snapshot of a Video Tracks.
  // May return null if the functionality is not available.
  virtual std::unique_ptr<WebImageCaptureFrameGrabber>
  CreateImageCaptureFrameGrabber();

  // WebSocket ----------------------------------------------------------

  // If this method returns non-null the returned object will be used to
  // determine if/when a new WebSocket connection can be exposed to Javascript.
  virtual std::unique_ptr<WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle();

  // WebWorker ----------------------------------------------------------

  virtual void DidStartWorkerThread() {}
  virtual void WillStopWorkerThread() {}
  virtual void WorkerContextCreated(const v8::Local<v8::Context>& worker) {}
  virtual bool AllowScriptExtensionForServiceWorker(const WebURL& script_url) {
    return false;
  }

  // WebCrypto ----------------------------------------------------------

  virtual WebCrypto* Crypto() { return nullptr; }

  // Mojo ---------------------------------------------------------------

  virtual service_manager::Connector* GetConnector();

  virtual InterfaceProvider* GetInterfaceProvider();

  virtual const char* GetBrowserServiceName() const { return ""; }

  // Platform events -----------------------------------------------------
  // Device Orientation, Device Motion, Battery, Gamepad.

  // Request the platform to start listening to the events of the specified
  // type and notify the given listener (if not null) when there is an update.
  virtual void StartListening(WebPlatformEventType type,
                              WebPlatformEventListener* listener) {}

  // Request the platform to stop listening to the specified event and no
  // longer notify the listener, if any.
  virtual void StopListening(WebPlatformEventType type) {}

  // This method converts from the supplied DOM code enum to the
  // embedder's DOM code value for the key pressed. |dom_code| values are
  // based on the value defined in
  // ui/events/keycodes/dom4/keycode_converter_data.h.
  // Returns null string, if DOM code value is not found.
  virtual WebString DomCodeStringFromEnum(int dom_code) { return WebString(); }

  // This method converts from the suppled DOM code value to the
  // embedder's DOM code enum for the key pressed. |code_string| is defined in
  // ui/events/keycodes/dom4/keycode_converter_data.h.
  // Returns 0, if DOM code enum is not found.
  virtual int DomEnumFromCodeString(const WebString& code_string) { return 0; }

  // This method converts from the supplied DOM |key| enum to the
  // corresponding DOM |key| string value for the key pressed. |dom_key| values
  // are based on the value defined in ui/events/keycodes/dom3/dom_key_data.h.
  // Returns empty string, if DOM key value is not found.
  virtual WebString DomKeyStringFromEnum(int dom_key) { return WebString(); }

  // This method converts from the suppled DOM |key| value to the
  // embedder's DOM |key| enum for the key pressed. |key_string| is defined in
  // ui/events/keycodes/dom3/dom_key_data.h.
  // Returns 0 if DOM key enum is not found.
  virtual int DomKeyEnumFromString(const WebString& key_string) { return 0; }

  // This method returns whether the specified |dom_key| is a modifier key.
  // |dom_key| values are based on the value defined in
  // ui/events/keycodes/dom3/dom_key_data.h.
  virtual bool IsDomKeyForModifier(int dom_key) { return false; }

  // Quota -----------------------------------------------------------

  // Queries the storage partition's storage usage and quota information.
  // WebStorageQuotaCallbacks::DidQueryStorageUsageAndQuota will be called
  // with the current usage and quota information for the partition. When
  // an error occurs WebStorageQuotaCallbacks::DidFail is called with an
  // error code.
  virtual void QueryStorageUsageAndQuota(const WebURL& storage_partition,
                                         WebStorageQuotaType,
                                         WebStorageQuotaCallbacks) {}

  // WebDatabase --------------------------------------------------------

  virtual WebDatabaseObserver* DatabaseObserver() { return nullptr; }

  // Web Notifications --------------------------------------------------

  virtual WebNotificationManager* GetWebNotificationManager() {
    return nullptr;
  }

  // Push API------------------------------------------------------------

  virtual WebPushProvider* PushProvider() { return nullptr; }

  // Background Sync API-------------------------------------------------

  virtual WebSyncProvider* BackgroundSyncProvider() { return nullptr; }

  // Origin Trials ------------------------------------------------------

  // TODO(crbug.com/738505): Remove the Web layer and return a
  // blink::TrialTokenValidator directly.
  virtual std::unique_ptr<WebTrialTokenValidator> CreateTrialTokenValidator();

  // Media Capabilities --------------------------------------------------

  virtual WebMediaCapabilitiesClient* MediaCapabilitiesClient() {
    return nullptr;
  }

  // Memory ------------------------------------------------------------

  // Requests purging memory. The platform may or may not purge memory,
  // depending on memory pressure.
  virtual void RequestPurgeMemory() {}

  // V8 Context Snapshot --------------------------------------------------

  // This method returns true only when
  // tools/v8_context_snapshot/v8_context_snapshot_generator is running (which
  // runs during Chromium's build step).
  virtual bool IsTakingV8ContextSnapshot() { return false; }

 protected:
  Platform();
  virtual ~Platform();

  WebThread* main_thread_;
  std::unique_ptr<WebThread> file_thread_;
};

}  // namespace blink

#endif
