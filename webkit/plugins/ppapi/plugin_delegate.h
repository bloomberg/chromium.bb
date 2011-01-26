// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_
#define WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "gfx/size.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/plugins/ppapi/dir_contents.h"

class AudioMessageFilter;
class GURL;

namespace base {
class MessageLoopProxy;
class Time;
}

namespace fileapi {
class FileSystemCallbackDispatcher;
}

namespace gfx {
class Rect;
}

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}
}

namespace skia {
class PlatformCanvas;
}

namespace WebKit {
class WebFileChooserCompletion;
struct WebFileChooserParams;
}

struct PP_Flash_NetAddress;
struct PP_VideoCompressedDataBuffer_Dev;
struct PP_VideoDecoderConfig_Dev;
struct PP_VideoUncompressedDataBuffer_Dev;

class TransportDIB;

namespace webkit {
namespace ppapi {

class FileIO;
class FullscreenContainer;
class PluginInstance;
class PluginModule;
class PPB_Flash_NetConnector_Impl;

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
    // Notification that the given plugin object has been deleted. This is
    // called from the module's destructor, so you should not dereference the
    // given pointer.
    virtual void PluginModuleDestroyed(PluginModule* destroyed_module) = 0;
  };

  // This class is implemented by the PluginDelegate implementation and is
  // designed to manage the lifetime and communicatin with the proxy's
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
    virtual bool Init() = 0;

    // Present the rendered frame to the compositor.
    virtual bool SwapBuffers() = 0;

    // Get the last EGL error.
    virtual unsigned GetError() = 0;

    // Set an optional callback that will be invoked when the side effects of
    // a SwapBuffers call become visible to the compositor. Takes ownership
    // of the callback.
    virtual void SetSwapBuffersCallback(Callback0::Type* callback) = 0;

    // If the plugin instance is backed by an OpenGL, return its ID in the
    // compositors namespace. Otherwise return 0. Returns 0 by default.
    virtual unsigned GetBackingTextureId() = 0;

    // This call will return the address of the GLES2 implementation for this
    // context that is constructed in Initialize() and is valid until this
    // context is destroyed.
    virtual gpu::gles2::GLES2Implementation* GetGLES2Implementation() = 0;
  };

  class PlatformAudio {
   public:
    class Client {
     protected:
      virtual ~Client() {}

     public:
      // Called when the stream is created.
      virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                                 size_t shared_memory_size,
                                 base::SyncSocket::Handle socket) = 0;
    };

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

  class PlatformVideoDecoder {
   public:
    virtual ~PlatformVideoDecoder() {}

    // Returns false on failure.
    virtual bool Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer) = 0;
    virtual int32_t Flush(PP_CompletionCallback& callback) = 0;
    virtual bool ReturnUncompressedDataBuffer(
        PP_VideoUncompressedDataBuffer_Dev& buffer) = 0;
  };

  // Indicates that the given instance has been created.
  virtual void InstanceCreated(PluginInstance* instance) = 0;

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  virtual void InstanceDeleted(PluginInstance* instance) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformImage2D* CreateImage2D(int width, int height) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformContext3D* CreateContext3D() = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      const PP_VideoDecoderConfig_Dev& decoder_config) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformAudio* CreateAudio(uint32_t sample_rate,
                                     uint32_t sample_count,
                                     PlatformAudio::Client* client) = 0;

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
  typedef Callback2<base::PlatformFileError, base::PlatformFile
                    >::Type AsyncOpenFileCallback;
  virtual bool AsyncOpenFile(const FilePath& path,
                             int flags,
                             AsyncOpenFileCallback* callback) = 0;
  virtual bool OpenFileSystem(
      const GURL& url,
      fileapi::FileSystemType type,
      long long size,
      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool MakeDirectory(
      const FilePath& path,
      bool recursive,
      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Query(const FilePath& path,
                     fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Touch(const FilePath& path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Delete(const FilePath& path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool Rename(const FilePath& file_path,
                      const FilePath& new_file_path,
                      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;
  virtual bool ReadDirectory(
      const FilePath& directory_path,
      fileapi::FileSystemCallbackDispatcher* dispatcher) = 0;

  virtual base::PlatformFileError OpenModuleLocalFile(
      const std::string& module_name,
      const FilePath& path,
      int flags,
      base::PlatformFile* file) = 0;
  virtual base::PlatformFileError RenameModuleLocalFile(
      const std::string& module_name,
      const FilePath& path_from,
      const FilePath& path_to) = 0;
  virtual base::PlatformFileError DeleteModuleLocalFileOrDir(
      const std::string& module_name,
      const FilePath& path,
      bool recursive) = 0;
  virtual base::PlatformFileError CreateModuleLocalDir(
      const std::string& module_name,
      const FilePath& path) = 0;
  virtual base::PlatformFileError QueryModuleLocalFile(
      const std::string& module_name,
      const FilePath& path,
      base::PlatformFileInfo* info) = 0;
  virtual base::PlatformFileError GetModuleLocalDirContents(
      const std::string& module_name,
      const FilePath& path,
      DirContents* contents) = 0;

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
      const struct PP_Flash_NetAddress* addr) = 0;

  // Create a fullscreen container for a plugin instance. This effectively
  // switches the plugin to fullscreen.
  virtual FullscreenContainer* CreateFullscreenContainer(
      PluginInstance* instance) = 0;

  // Returns a string with the name of the default 8-bit char encoding.
  virtual std::string GetDefaultEncoding() = 0;

  // Sets the mininum and maximium zoom factors.
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

  // Tells the browser that the PDF has an unsupported feature.
  virtual void HasUnsupportedFeature() = 0;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PLUGIN_DELEGATE_H_
