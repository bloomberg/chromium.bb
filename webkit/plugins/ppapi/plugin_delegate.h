// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_
#define WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/message_loop_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/video/capture/video_capture.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ui/gfx/size.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/plugins/ppapi/dir_contents.h"
#include "webkit/quota/quota_types.h"

class GURL;
struct PP_NetAddress_Private;
class SkBitmap;
class TransportDIB;

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
struct Preferences;
}

namespace skia {
class PlatformCanvas;
}

namespace WebKit {
class WebFileChooserCompletion;
struct WebCursorInfo;
struct WebFileChooserParams;
}

namespace webkit_glue {
class P2PTransport;
}  // namespace webkit_glue

namespace webkit {
namespace ppapi {

class FileIO;
class FullscreenContainer;
class PepperFilePath;
class PluginInstance;
class PluginModule;
class PPB_Broker_Impl;
class PPB_Flash_Menu_Impl;
class PPB_Flash_NetConnector_Impl;
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
  };

  // Represents an image. This is to allow the browser layer to supply a correct
  // image representation. In Chrome, this will be a TransportDIB.
  class PlatformImage2D {
   public:
    virtual ~PlatformImage2D() {}

    // Caller will own the returned pointer, returns NULL on failure.
    virtual skia::PlatformCanvas* Map() = 0;

    // Returns the platform-specific shared memory handle of the data backing
    // this image. This is used by PPAPI proxying to send the image to the
    // out-of-process plugin. On success, the size in bytes will be placed into
    // |*bytes_count|. Returns 0 on failure.
    virtual intptr_t GetSharedMemoryHandle(uint32* byte_count) const = 0;

    virtual TransportDIB* GetTransportDIB() const = 0;
  };

  class PlatformContext3D {
   public:
    virtual ~PlatformContext3D() {}

    // Initialize the context.
    virtual bool Init(const int32* attrib_list) = 0;

    // If the plugin instance is backed by an OpenGL, return its ID in the
    // compositors namespace. Otherwise return 0. Returns 0 by default.
    virtual unsigned GetBackingTextureId() = 0;

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

    // Run the callback once the channel has been flushed.
    virtual bool Echo(const base::Callback<void()>& callback) = 0;
  };

  // The (interface for the) client used by |PlatformAudio| and
  // |PlatformAudioInput|.
  class PlatformAudioCommonClient {
   protected:
    virtual ~PlatformAudioCommonClient() {}

   public:
    // Called when the stream is created.
    virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                               size_t shared_memory_size,
                               base::SyncSocket::Handle socket) = 0;
  };

  class PlatformAudio {
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
    virtual ~PlatformAudio() {}
  };

  class PlatformAudioInput {
   public:
    // Starts the playback. Returns false on error or if called before the
    // stream is created or after the stream is closed.
    virtual bool StartCapture() = 0;

    // Stops the capture. Returns false on error or if called before the stream
    // is created or after the stream is closed.
    virtual bool StopCapture() = 0;

    // Closes the stream. Make sure to call this before the object is
    // destructed.
    virtual void ShutDown() = 0;

   protected:
    virtual ~PlatformAudioInput() {}
  };

  // Interface for PlatformVideoDecoder is directly inherited from general media
  // VideoDecodeAccelerator interface.
  class PlatformVideoDecoder : public media::VideoDecodeAccelerator {
   protected:
    virtual ~PlatformVideoDecoder() {}
  };

  class PlatformVideoCapture : public media::VideoCapture {
   public:
    virtual ~PlatformVideoCapture() {}
  };

  // Provides access to the ppapi broker.
  class PpapiBroker {
   public:
    virtual void Connect(webkit::ppapi::PPB_Broker_Impl* client) = 0;

    // Decrements the references to the broker.
    // When there are no more references, this renderer's dispatcher is
    // destroyed, allowing the broker to shutdown if appropriate.
    // Callers should not reference this object after calling Disconnect.
    virtual void Disconnect(webkit::ppapi::PPB_Broker_Impl* client) = 0;

   protected:
    virtual ~PpapiBroker() {}
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

  // Returns a pointer (ownership not transferred) to the bitmap to paint the
  // sad plugin screen with. Returns NULL on failure.
  virtual SkBitmap* GetSadPluginBitmap() = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformImage2D* CreateImage2D(int width, int height) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformContext3D* CreateContext3D() = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformVideoCapture* CreateVideoCapture(
      media::VideoCapture::EventHandler* handler) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id) = 0;

  // The caller is responsible for calling Shutdown() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  virtual PlatformAudio* CreateAudio(uint32_t sample_rate,
                                     uint32_t sample_count,
                                     PlatformAudioCommonClient* client) = 0;

  // The caller is responsible for calling Shutdown() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  virtual PlatformAudioInput* CreateAudioInput(uint32_t sample_rate,
      uint32_t sample_count,
      PlatformAudioCommonClient* client) = 0;

  // A pointer is returned immediately, but it is not ready to be used until
  // BrokerConnected has been called.
  // The caller is responsible for calling Release() on the returned pointer
  // to clean up the corresponding resources allocated during this call.
  virtual PpapiBroker* ConnectToPpapiBroker(
      webkit::ppapi::PPB_Broker_Impl* client) = 0;

  // Notifies that the number of find results has changed.
  virtual void NumberOfFindResultsChanged(int identifier,
                                          int total,
                                          bool final_result) = 0;

  // Notifies that the index of the currently selected item has been updated.
  virtual void SelectedFindResultChanged(int identifier, int index) = 0;

  // Runs a file chooser.
  virtual bool RunFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion) = 0;

  // Sends an async IPC to open a file.
  typedef base::Callback<void (base::PlatformFileError, base::PassPlatformFile)>
      AsyncOpenFileCallback;
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) = 0;
  virtual bool AsyncOpenFileSystemURL(
      const GURL& path,
      int flags,
      const AsyncOpenFileCallback& callback) = 0;

  virtual bool OpenFileSystem(
      const GURL& url,
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

  virtual base::PlatformFileError OpenFile(const PepperFilePath& path,
                                           int flags,
                                           base::PlatformFile* file) = 0;
  virtual base::PlatformFileError RenameFile(const PepperFilePath& from_path,
                                             const PepperFilePath& to_path) = 0;
  virtual base::PlatformFileError DeleteFileOrDir(const PepperFilePath& path,
                                                  bool recursive) = 0;
  virtual base::PlatformFileError CreateDir(const PepperFilePath& path) = 0;
  virtual base::PlatformFileError QueryFile(const PepperFilePath& path,
                                            base::PlatformFileInfo* info) = 0;
  virtual base::PlatformFileError GetDirContents(const PepperFilePath& path,
                                                 DirContents* contents) = 0;

  // Synchronously returns the platform file path for a filesystem URL.
  virtual void SyncGetFileSystemPlatformPath(const GURL& url,
                                             FilePath* platform_path) = 0;

  // Returns a MessageLoopProxy instance associated with the message loop
  // of the file thread in this renderer.
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() = 0;

  virtual int32_t ConnectTcp(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const char* host,
      uint16_t port) = 0;
  virtual int32_t ConnectTcpAddress(
      webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
      const PP_NetAddress_Private* addr) = 0;

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
  virtual void TCPSocketSSLHandshake(uint32 socket_id,
                                     const std::string& server_name,
                                     uint16_t server_port) = 0;
  virtual void TCPSocketRead(uint32 socket_id, int32_t bytes_to_read) = 0;
  virtual void TCPSocketWrite(uint32 socket_id, const std::string& buffer) = 0;
  virtual void TCPSocketDisconnect(uint32 socket_id) = 0;

  // For PPB_UDPSocket_Private.
  virtual uint32 UDPSocketCreate() = 0;
  virtual void UDPSocketBind(PPB_UDPSocket_Private_Impl* socket,
                             uint32 socket_id,
                             const PP_NetAddress_Private& addr) = 0;
  virtual void UDPSocketRecvFrom(uint32 socket_id, int32_t num_bytes) = 0;
  virtual void UDPSocketSendTo(uint32 socket_id,
                               const std::string& buffer,
                               const PP_NetAddress_Private& addr) = 0;
  virtual void UDPSocketClose(uint32 socket_id) = 0;

  // Show the given context menu at the given position (in the plugin's
  // coordinates).
  virtual int32_t ShowContextMenu(
      PluginInstance* instance,
      webkit::ppapi::PPB_Flash_Menu_Impl* menu,
      const gfx::Point& position) = 0;

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

  // Retrieves the proxy information for the given URL in PAC format. On error,
  // this will return an empty string.
  virtual std::string ResolveProxy(const GURL& url) = 0;

  // Tell the browser when resource loading starts/ends.
  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Sets restrictions on how the content can be used (i.e. no print/copy).
  virtual void SetContentRestriction(int restrictions) = 0;

  // Tells the browser to bring up SaveAs dialog to save specified URL.
  virtual void SaveURLAs(const GURL& url) = 0;

  // Creates P2PTransport object.
  virtual webkit_glue::P2PTransport* CreateP2PTransport() = 0;

  virtual double GetLocalTimeZoneOffset(base::Time t) = 0;

  // TODO(viettrungluu): Generalize this for use with other plugins if it proves
  // necessary.
  virtual std::string GetFlashCommandLineArgs() = 0;

  // Create an anonymous shared memory segment of size |size| bytes, and return
  // a pointer to it, or NULL on error.  Caller owns the returned pointer.
  virtual base::SharedMemory* CreateAnonymousSharedMemory(uint32_t size) = 0;

  // Returns the current preferences.
  virtual ::ppapi::Preferences GetPreferences() = 0;

  // Locks the mouse for |instance|. It will call
  // PluginInstance::OnLockMouseACK() to notify the instance when the operation
  // is completed. The call to OnLockMouseACK() may be synchronous (i.e., it may
  // be called when LockMouse() is still on the stack).
  virtual void LockMouse(PluginInstance* instance) = 0;

  // Unlocks the mouse if |instance| currently owns the mouse lock. Whenever an
  // plugin instance has lost the mouse lock, it will be notified by
  // PluginInstance::OnMouseLockLost(). Please note that UnlockMouse() is not
  // the only cause of losing mouse lock. For example, a user may press the Esc
  // key to quit the mouse lock mode, which also results in an OnMouseLockLost()
  // call to the current mouse lock owner.
  virtual void UnlockMouse(PluginInstance* instance) = 0;

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  virtual void DidChangeCursor(PluginInstance* instance,
                               const WebKit::WebCursorInfo& cursor) = 0;

  // Notifies that |instance| has received a mouse event.
  virtual void DidReceiveMouseEvent(PluginInstance* instance) = 0;

  // Determines if the browser entered fullscreen mode.
  virtual bool IsInFullscreenMode() = 0;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_
