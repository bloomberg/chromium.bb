// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_
#define WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/video/capture/video_capture.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/shared_impl/dir_contents.h"
#include "ui/gfx/size.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/glue/clipboard_client.h"
#include "webkit/quota/quota_types.h"

class GURL;
class SkBitmap;
class SkCanvas;
class TransportDIB;
struct PP_HostResolver_Private_Hint;
struct PP_NetAddress_Private;

namespace WebKit {
class WebGraphicsContext3D;
}

namespace base {
class MessageLoopProxy;
class Time;
}

namespace fileapi {
class FileSystemCallbackDispatcher;
}

namespace gfx {
class Point;
}

namespace gpu {
class CommandBuffer;
}

namespace ppapi {
class PepperFilePath;
class PPB_HostResolver_Shared;
class PPB_X509Certificate_Fields;
struct DeviceRefData;
struct HostPortPair;
struct Preferences;

namespace thunk {
class ResourceCreationAPI;
}

}  // namespace ppapi

namespace WebKit {
typedef SkCanvas WebCanvas;
class WebGamepads;
class WebPlugin;
struct WebCompositionUnderline;
struct WebCursorInfo;
}

namespace webkit_glue {
class ClipboardClient;
class P2PTransport;
class NetworkListObserver;
}  // namespace webkit_glue

namespace webkit {
namespace ppapi {

class FileIO;
class FullscreenContainer;
class PluginInstance;
class PluginModule;
class PPB_Broker_Impl;
class PPB_Flash_Menu_Impl;
class PPB_ImageData_Impl;
class PPB_TCPSocket_Private_Impl;
class PPB_UDPSocket_Private_Impl;

// Virtual interface that the browser implements to implement features for
// PPAPI plugins.
class PluginDelegate {
 public:
  // This interface is used for the PluginModule to tell the code in charge of
  // re-using modules which modules currently exist.
  //
  // It is different than the other interfaces, which are scoped to the
  // lifetime of the plugin instance. The implementor of this interface must
  // outlive all plugin modules, and is in practice a singleton
  // (PepperPluginRegistry). This requirement means we can't do the obvious
  // thing and just have a PluginDelegate call for this purpose (when the
  // module is being deleted, we know there are no more PluginInstances that
  // have PluginDelegates).
  class ModuleLifetime {
   public:
    // Notification that the given plugin object is no longer usable. It either
    // indicates the module was deleted, or that it has crashed.
    //
    // This can be called from the module's destructor, so you should not
    // dereference the given pointer.
    virtual void PluginModuleDead(PluginModule* dead_module) = 0;
  };

  // This class is implemented by the PluginDelegate implementation and is
  // designed to manage the lifetime and communication with the proxy's
  // HostDispatcher for out-of-process PPAPI plugins.
  //
  // The point of this is to avoid having a relationship from the PPAPI plugin
  // implementation to the ppapi proxy code. Otherwise, things like the IPC
  // system will be dependencies of the webkit directory, which we don't want.
  //
  // The PluginModule will scope the lifetime of this object to its own
  // lifetime, so the implementation can use this to manage the HostDispatcher
  // lifetime without introducing the dependency.
  class OutOfProcessProxy {
   public:
    virtual ~OutOfProcessProxy() {}

    // Implements GetInterface for the proxied plugin.
    virtual const void* GetProxiedInterface(const char* name) = 0;

    // Notification to the out-of-process layer that the given plugin instance
    // has been created. This will happen before the normal PPB_Instance method
    // calls so the out-of-process code can set up the tracking information for
    // the new instance.
    virtual void AddInstance(PP_Instance instance) = 0;

    // Like AddInstance but removes the given instance. This is called after
    // regular instance shutdown so the out-of-process code can clean up its
    // tracking information.
    virtual void RemoveInstance(PP_Instance instance) = 0;

    virtual base::ProcessId GetPeerProcessId() = 0;
  };

  // Represents an image. This is to allow the browser layer to supply a correct
  // image representation. In Chrome, this will be a TransportDIB.
  class PlatformImage2D {
   public:
    virtual ~PlatformImage2D() {}

    // Caller will own the returned pointer, returns NULL on failure.
    virtual SkCanvas* Map() = 0;

    // Returns the platform-specific shared memory handle of the data backing
    // this image. This is used by PPAPI proxying to send the image to the
    // out-of-process plugin. On success, the size in bytes will be placed into
    // |*bytes_count|. Returns 0 on failure.
    virtual intptr_t GetSharedMemoryHandle(uint32* byte_count) const = 0;

    virtual TransportDIB* GetTransportDIB() const = 0;
  };

  class PlatformGraphics2D {
   public:
    virtual bool ReadImageData(PP_Resource image, const PP_Point* top_left) = 0;

    // Assciates this device with the given plugin instance. You can pass NULL
    // to clear the existing device. Returns true on success. In this case, a
    // repaint of the page will also be scheduled. Failure means that the device
    // is already bound to a different instance, and nothing will happen.
    virtual bool BindToInstance(PluginInstance* new_instance) = 0;

    // Paints the current backing store to the web page.
    virtual void Paint(WebKit::WebCanvas* canvas,
                       const gfx::Rect& plugin_rect,
                       const gfx::Rect& paint_rect) = 0;

    // Notifications about the view's progress painting.  See PluginInstance.
    // These messages are used to send Flush callbacks to the plugin.
    virtual void ViewWillInitiatePaint() = 0;
    virtual void ViewInitiatedPaint() = 0;
    virtual void ViewFlushedPaint() = 0;

    virtual bool IsAlwaysOpaque() const = 0;
    virtual void SetScale(float scale) = 0;
    virtual float GetScale() const = 0;
    virtual PPB_ImageData_Impl* ImageData() = 0;
  };

  class PlatformContext3D {
   public:
    virtual ~PlatformContext3D() {}

    // Initialize the context.
    virtual bool Init(const int32* attrib_list,
                      PlatformContext3D* share_context) = 0;

    // If the plugin instance is backed by an OpenGL, return its ID in the
    // compositors namespace. Otherwise return 0. Returns 0 by default.
    virtual unsigned GetBackingTextureId() = 0;

    // Returns the parent context that allocated the backing texture ID.
    virtual WebKit::WebGraphicsContext3D* GetParentContext() = 0;

    // Returns true if the backing texture is always opaque.
    virtual bool IsOpaque() = 0;

    // This call will return the address of the command buffer for this context
    // that is constructed in Initialize() and is valid until this context is
    // destroyed.
    virtual ::gpu::CommandBuffer* GetCommandBuffer() = 0;

    // If the command buffer is routed in the GPU channel, return the route id.
    // Otherwise return 0.
    virtual int GetCommandBufferRouteId() = 0;

    // Set an optional callback that will be invoked when the context is lost
    // (e.g. gpu process crash). Takes ownership of the callback.
    virtual void SetContextLostCallback(
        const base::Callback<void()>& callback) = 0;

    // Set an optional callback that will be invoked when the GPU process
    // sends a console message.
    typedef base::Callback<void(const std::string&, int)>
        ConsoleMessageCallback;
    virtual void SetOnConsoleMessageCallback(
        const ConsoleMessageCallback& callback) = 0;

    // Run the callback once the channel has been flushed.
    virtual bool Echo(const base::Callback<void()>& callback) = 0;
  };

  // The base class of clients used by |PlatformAudioOutput| and
  // |PlatformAudioInput|.
  class PlatformAudioClientBase {
   protected:
    virtual ~PlatformAudioClientBase() {}

   public:
    // Called when the stream is created.
    virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                               size_t shared_memory_size,
                               base::SyncSocket::Handle socket) = 0;
  };

  class PlatformAudioOutputClient : public PlatformAudioClientBase {
   protected:
    virtual ~PlatformAudioOutputClient() {}
  };

  class PlatformAudioOutput {
   public:
    // Starts the playback. Returns false on error or if called before the
    // stream is created or after the stream is closed.
    virtual bool StartPlayback() = 0;

    // Stops the playback. Returns false on error or if called before the stream
    // is created or after the stream is closed.
    virtual bool StopPlayback() = 0;

    // Closes the stream. Make sure to call this before the object is
    // destructed.
    virtual void ShutDown() = 0;

   protected:
    virtual ~PlatformAudioOutput() {}
  };

  class PlatformAudioInputClient : public PlatformAudioClientBase {
   public:
    virtual void StreamCreationFailed() = 0;

   protected:
    virtual ~PlatformAudioInputClient() {}
  };

  class PlatformAudioInput {
   public:
    virtual void StartCapture() = 0;
    virtual void StopCapture() = 0;

    // Closes the stream. Make sure to call this before the object is
    // destructed.
    virtual void ShutDown() = 0;

   protected:
    virtual ~PlatformAudioInput() {}
  };

  // Interface for PlatformVideoDecoder is directly inherited from general media
  // VideoDecodeAccelerator interface.
  class PlatformVideoDecoder : public media::VideoDecodeAccelerator {
   public:
    virtual ~PlatformVideoDecoder() {}
  };

  class PlatformVideoCaptureEventHandler
      : public media::VideoCapture::EventHandler {
   public:
    virtual ~PlatformVideoCaptureEventHandler() {}

    virtual void OnInitialized(media::VideoCapture* capture,
                               bool succeeded) = 0;
  };

  class PlatformVideoCapture : public media::VideoCapture,
                               public base::RefCounted<PlatformVideoCapture> {
   public:
    // Detaches the event handler and stops sending notifications to it.
    virtual void DetachEventHandler() = 0;

   protected:
    virtual ~PlatformVideoCapture() {}

   private:
    friend class base::RefCounted<PlatformVideoCapture>;
  };

  // Provides access to the ppapi broker.
  class Broker {
   public:
    // Decrements the references to the broker.
    // When there are no more references, this renderer's dispatcher is
    // destroyed, allowing the broker to shutdown if appropriate.
    // Callers should not reference this object after calling Disconnect().
    virtual void Disconnect(webkit::ppapi::PPB_Broker_Impl* client) = 0;

   protected:
    virtual ~Broker() {}
  };

  // Notification that the given plugin is focused or unfocused.
  virtual void PluginFocusChanged(webkit::ppapi::PluginInstance* instance,
                                  bool focused) = 0;
  // Notification that the text input status of the given plugin is changed.
  virtual void PluginTextInputTypeChanged(
      webkit::ppapi::PluginInstance* instance) = 0;
  // Notification that the caret position in the given plugin is changed.
  virtual void PluginCaretPositionChanged(
      webkit::ppapi::PluginInstance* instance) = 0;
  // Notification that the plugin requested to cancel the current composition.
  virtual void PluginRequestedCancelComposition(
      webkit::ppapi::PluginInstance* instance) = 0;
  // Notification that the text selection in the given plugin is changed.
  virtual void PluginSelectionChanged(
      webkit::ppapi::PluginInstance* instance) = 0;
  // Requests simulating IME events for testing purpose.
  virtual void SimulateImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) = 0;
  virtual void SimulateImeConfirmComposition(const string16& text) = 0;

  // Notification that the given plugin has crashed. When a plugin crashes, all
  // instances associated with that plugin will notify that they've crashed via
  // this function.
  virtual void PluginCrashed(PluginInstance* instance) = 0;

  // Indicates that the given instance has been created.
  virtual void InstanceCreated(PluginInstance* instance) = 0;

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  virtual void InstanceDeleted(PluginInstance* instance) = 0;

  // Creates the resource creation API for the given instance.
  virtual scoped_ptr< ::ppapi::thunk::ResourceCreationAPI>
      CreateResourceCreationAPI(PluginInstance* instance) = 0;

  // Returns a pointer (ownership not transferred) to the bitmap to paint the
  // sad plugin screen with. Returns NULL on failure.
  virtual SkBitmap* GetSadPluginBitmap() = 0;

  // Creates a replacement plug-in that is shown when the plug-in at |file_path|
  // couldn't be loaded.
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      const FilePath& file_path) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformImage2D* CreateImage2D(int width, int height) = 0;

  // Returns the internal PlatformGraphics2D implementation.
  virtual PlatformGraphics2D* GetGraphics2D(PluginInstance* instance,
                                            PP_Resource graphics_2d) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformContext3D* CreateContext3D() = 0;

  // Set that the context will now present to the delegate.
  virtual void ReparentContext(PlatformContext3D*) = 0;

  // If |device_id| is empty, the default video capture device will be used. The
  // user can start using the returned object to capture video right away.
  // Otherwise, the specified device will be used. The user needs to wait till
  // |handler| gets an OnInitialized() notification to start using the returned
  // object.
  virtual PlatformVideoCapture* CreateVideoCapture(
      const std::string& device_id,
      PlatformVideoCaptureEventHandler* handler) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id) = 0;

  // Get audio hardware output sample rate.
  virtual uint32_t GetAudioHardwareOutputSampleRate() = 0;

  // Get audio hardware output buffer size.
  virtual uint32_t GetAudioHardwareOutputBufferSize() = 0;

  // The caller is responsible for calling Shutdown() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  virtual PlatformAudioOutput* CreateAudioOutput(
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioOutputClient* client) = 0;

  // If |device_id| is empty, the default audio input device will be used.
  // The caller is responsible for calling Shutdown() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  virtual PlatformAudioInput* CreateAudioInput(
      const std::string& device_id,
      uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioInputClient* client) = 0;

  // A pointer is returned immediately, but it is not ready to be used until
  // BrokerConnected has been called.
  // The caller is responsible for calling Disconnect() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  virtual Broker* ConnectToBroker(webkit::ppapi::PPB_Broker_Impl* client) = 0;

  // Notifies that the number of find results has changed.
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result) = 0;

  // Notifies that the index of the currently selected item has been updated.
  virtual void SelectedFindResultChanged(int identifier, int index) = 0;

  // Sends an async IPC to open a local file.
  typedef base::Callback<void (base::PlatformFileError, base::PassPlatformFile)>
      AsyncOpenFileCallback;
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) = 0;

  // Sends an async IPC to open a file through filesystem API.
  // When a file is successfully opened, |callback| is invoked with
  // PLATFORM_FILE_OK, the opened file handle, and a callback function for
  // notifying that the file is closed. When the users of this function
  // finished using the file, they must close the file handle and then must call
  // the supplied callback function.
  typedef base::Callback<void (base::PlatformFileError)>
      NotifyCloseFileCallback;
  typedef base::Callback<
      void (base::PlatformFileError,
            base::PassPlatformFile,
            const NotifyCloseFileCallback&)> AsyncOpenFileSystemURLCallback;
  virtual bool AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileSystemURLCallback& callback) = 0;

  virtual bool OpenFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      long long size,
      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool MakeDirectory(
      const GURL& path,
      bool recursive,
      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Query(const GURL& path,
                     fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Touch(const GURL& path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool SetLength(const GURL& path,
                         int64_t length,
                         fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Delete(const GURL& path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Rename(const GURL& file_path,
                      const GURL& new_file_path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool ReadDirectory(
      const GURL& directory_path,
      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;

  // For quota handlings for FileIO API.
  typedef base::Callback<void (int64)> AvailableSpaceCallback;
  virtual void QueryAvailableSpace(const GURL& origin,
                                   quota::StorageType type,
                                   const AvailableSpaceCallback& callback) = 0;
  virtual void WillUpdateFile(const GURL& file_path) = 0;
  virtual void DidUpdateFile(const GURL& file_path, int64_t delta) = 0;

  // Synchronously returns the platform file path for a filesystem URL.
  virtual void SyncGetFileSystemPlatformPath(const GURL& url,
                                             FilePath* platform_path) = 0;

  // Returns a MessageLoopProxy instance associated with the message loop
  // of the file thread in this renderer.
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() = 0;

  // For PPB_TCPSocket_Private.
  virtual uint32 TCPSocketCreate() = 0;
  virtual void TCPSocketConnect(PPB_TCPSocket_Private_Impl* socket,
                                uint32 socket_id,
                                const std::string& host,
                                uint16_t port) = 0;
  virtual void TCPSocketConnectWithNetAddress(
      PPB_TCPSocket_Private_Impl* socket,
      uint32 socket_id,
      const PP_NetAddress_Private& addr) = 0;
  virtual void TCPSocketSSLHandshake(
      uint32 socket_id,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs) = 0;
  virtual void TCPSocketRead(uint32 socket_id, int32_t bytes_to_read) = 0;
  virtual void TCPSocketWrite(uint32 socket_id, const std::string& buffer) = 0;
  virtual void TCPSocketDisconnect(uint32 socket_id) = 0;
  virtual void RegisterTCPSocket(PPB_TCPSocket_Private_Impl* socket,
                                 uint32 socket_id) = 0;

  // For PPB_UDPSocket_Private.
  virtual uint32 UDPSocketCreate() = 0;
  virtual void UDPSocketSetBoolSocketFeature(PPB_UDPSocket_Private_Impl* socket,
                                             uint32 socket_id,
                                             int32_t name,
                                             bool value) = 0;
  virtual void UDPSocketBind(PPB_UDPSocket_Private_Impl* socket,
                             uint32 socket_id,
                             const PP_NetAddress_Private& addr) = 0;
  virtual void UDPSocketRecvFrom(uint32 socket_id, int32_t num_bytes) = 0;
  virtual void UDPSocketSendTo(uint32 socket_id,
                               const std::string& buffer,
                               const PP_NetAddress_Private& addr) = 0;
  virtual void UDPSocketClose(uint32 socket_id) = 0;

  // For PPB_TCPServerSocket_Private.
  virtual void TCPServerSocketListen(PP_Resource socket_resource,
                                     const PP_NetAddress_Private& addr,
                                     int32_t backlog) = 0;
  virtual void TCPServerSocketAccept(uint32 server_socket_id) = 0;
  virtual void TCPServerSocketStopListening(
      PP_Resource socket_resource,
      uint32 socket_id) = 0;

  // For PPB_HostResolver_Private.
  virtual void RegisterHostResolver(
      ::ppapi::PPB_HostResolver_Shared* host_resolver,
      uint32 host_resolver_id) = 0;
  virtual void HostResolverResolve(
      uint32 host_resolver_id,
      const ::ppapi::HostPortPair& host_port,
      const PP_HostResolver_Private_Hint* hint) = 0;
  virtual void UnregisterHostResolver(uint32 host_resolver_id) = 0;

  // Add/remove a network list observer.
  virtual bool AddNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) = 0;
  virtual void RemoveNetworkListObserver(
      webkit_glue::NetworkListObserver* observer) = 0;

  // For PPB_X509Certificate_Private.
  virtual bool X509CertificateParseDER(
      const std::vector<char>& der,
      ::ppapi::PPB_X509Certificate_Fields* fields) = 0;

  // Create a fullscreen container for a plugin instance. This effectively
  // switches the plugin to fullscreen.
  virtual FullscreenContainer* CreateFullscreenContainer(
      PluginInstance* instance) = 0;

  // Gets the size of the screen. The fullscreen window will be created at that
  // size.
  virtual gfx::Size GetScreenSize() = 0;

  // Returns a string with the name of the default 8-bit char encoding.
  virtual std::string GetDefaultEncoding() = 0;

  // Sets the minimum and maximum zoom factors.
  virtual void ZoomLimitsChanged(double minimum_factor,
                                 double maximum_factor) = 0;

  // Tell the browser when resource loading starts/ends.
  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Sets restrictions on how the content can be used (i.e. no print/copy).
  virtual void SetContentRestriction(int restrictions) = 0;

  // Tells the browser to bring up SaveAs dialog to save specified URL.
  virtual void SaveURLAs(const GURL& url) = 0;

  // Create an anonymous shared memory segment of size |size| bytes, and return
  // a pointer to it, or NULL on error.  Caller owns the returned pointer.
  virtual base::SharedMemory* CreateAnonymousSharedMemory(uint32_t size) = 0;

  // Returns the current preferences.
  virtual ::ppapi::Preferences GetPreferences() = 0;

  // Locks the mouse for |instance|. If false is returned, the lock is not
  // possible. If true is returned then the lock is pending. Success or
  // failure will be delivered asynchronously via
  // PluginInstance::OnLockMouseACK().
  virtual bool LockMouse(PluginInstance* instance) = 0;

  // Unlocks the mouse if |instance| currently owns the mouse lock. Whenever an
  // plugin instance has lost the mouse lock, it will be notified by
  // PluginInstance::OnMouseLockLost(). Please note that UnlockMouse() is not
  // the only cause of losing mouse lock. For example, a user may press the Esc
  // key to quit the mouse lock mode, which also results in an OnMouseLockLost()
  // call to the current mouse lock owner.
  virtual void UnlockMouse(PluginInstance* instance) = 0;

  // Returns true iff |instance| currently owns the mouse lock.
  virtual bool IsMouseLocked(PluginInstance* instance) = 0;

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  virtual void DidChangeCursor(PluginInstance* instance,
                               const WebKit::WebCursorInfo& cursor) = 0;

  // Notifies that |instance| has received a mouse event.
  virtual void DidReceiveMouseEvent(PluginInstance* instance) = 0;

  // Determines if the browser entered fullscreen mode.
  virtual bool IsInFullscreenMode() = 0;

  // Retrieve current gamepad data.
  virtual void SampleGamepads(WebKit::WebGamepads* data) = 0;

  // Returns true if the containing page is visible.
  virtual bool IsPageVisible() const = 0;

  typedef base::Callback<
      void (int /* request_id */,
            bool /* succeeded */,
            const std::vector< ::ppapi::DeviceRefData>& /* devices */)>
      EnumerateDevicesCallback;

  // Enumerates devices of the specified type. The request ID passed into the
  // callback will be the same as the return value.
  virtual int EnumerateDevices(PP_DeviceType_Dev type,
                               const EnumerateDevicesCallback& callback) = 0;
  // Stop enumerating devices of the specified |request_id|. The |request_id|
  // is the return value of EnumerateDevicesCallback.
  virtual void StopEnumerateDevices(int request_id) = 0;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_
