// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

class Buffer;
class Audio;
class AudioConfig;
class DirectoryReader;
class FileChooser;
class FileIO;
class FileRef;
class Font;
class Graphics2D;
class Graphics3D;
class ImageData;
class ObjectVar;
class PluginModule;
class PrivateFontFile;
class Scrollbar;
class StringVar;
class Transport;
class URLLoader;
class URLRequestInfo;
class URLResponseInfo;
class Var;
class VideoDecoder;
class Widget;

class Resource : public base::RefCountedThreadSafe<Resource> {
 public:
  explicit Resource(PluginModule* module)  : resource_id_(0), module_(module) {}
  virtual ~Resource() {}

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> GetAs(PP_Resource res) {
    scoped_refptr<Resource> resource = ResourceTracker::Get()->GetResource(res);
    return resource ? resource->Cast<T>() : NULL;
  }

  PluginModule* module() const { return module_; }

  // Cast the resource into a specified type. This will return NULL if the
  // resource does not match the specified type. Specializations of this
  // template call into As* functions.
  template <typename T> T* Cast() { return NULL; }

  // Returns an resource id of this object. If the object doesn't have a
  // resource id, new one is created with plugin refcount of 1. If it does,
  // the refcount is incremented. Use this when you need to return a new
  // reference to the plugin.
  PP_Resource GetReference();

  // When you need to ensure that a resource has a reference, but you do not
  // want to increase the refcount (for example, if you need to call a plugin
  // callback function with a reference), you can use this class. For example:
  //
  // plugin_callback(.., ScopedResourceId(resource).id, ...);
  class ScopedResourceId {
   public:
    explicit ScopedResourceId(Resource* resource)
        : id(resource->GetReference()) {}
    ~ScopedResourceId() {
      ResourceTracker::Get()->UnrefResource(id);
    }
    const PP_Resource id;
  };

 private:
  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type. Used by the Cast()
  // function.
  virtual Audio* AsAudio() { return NULL; }
  virtual AudioConfig* AsAudioConfig() { return NULL; }
  virtual Buffer* AsBuffer() { return NULL; }
  virtual DirectoryReader* AsDirectoryReader() { return NULL; }
  virtual FileChooser* AsFileChooser() { return NULL; }
  virtual FileIO* AsFileIO() { return NULL; }
  virtual FileRef* AsFileRef() { return NULL; }
  virtual Font* AsFont() { return NULL; }
  virtual Graphics2D* AsGraphics2D() { return NULL; }
  virtual Graphics3D* AsGraphics3D() { return NULL; }
  virtual ImageData* AsImageData() { return NULL; }
  virtual ObjectVar* AsObjectVar() { return NULL; }
  virtual PrivateFontFile* AsPrivateFontFile() { return NULL; }
  virtual Scrollbar* AsScrollbar() { return NULL; }
  virtual StringVar* AsStringVar() { return NULL; }
  virtual Transport* AsTransport() { return NULL; }
  virtual URLLoader* AsURLLoader() { return NULL; }
  virtual URLRequestInfo* AsURLRequestInfo() { return NULL; }
  virtual URLResponseInfo* AsURLResponseInfo() { return NULL; }
  virtual Var* AsVar() { return NULL; }
  virtual VideoDecoder* AsVideoDecoder() { return NULL; }
  virtual Widget* AsWidget() { return NULL; }

 private:
  // If referenced by a plugin, holds the id of this resource object. Do not
  // access this member directly, because it is possible that the plugin holds
  // no references to the object, and therefore the resource_id_ is zero. Use
  // either GetReference() to obtain a new resource_id and increase the
  // refcount, or TemporaryReference when you do not want to increase the
  // refcount.
  PP_Resource resource_id_;

  // Non-owning pointer to our module.
  PluginModule* module_;

  // Called by the resource tracker when the last plugin reference has been
  // dropped.
  friend class ResourceTracker;
  void StoppedTracking();

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

// Cast() specializations.
#define DEFINE_RESOURCE_CAST(Type)                   \
  template <> inline Type* Resource::Cast<Type>() {  \
      return As##Type();                             \
  }

DEFINE_RESOURCE_CAST(Audio)
DEFINE_RESOURCE_CAST(AudioConfig)
DEFINE_RESOURCE_CAST(Buffer)
DEFINE_RESOURCE_CAST(DirectoryReader)
DEFINE_RESOURCE_CAST(FileChooser)
DEFINE_RESOURCE_CAST(FileIO)
DEFINE_RESOURCE_CAST(FileRef)
DEFINE_RESOURCE_CAST(Font)
DEFINE_RESOURCE_CAST(Graphics2D)
DEFINE_RESOURCE_CAST(Graphics3D)
DEFINE_RESOURCE_CAST(ImageData)
DEFINE_RESOURCE_CAST(ObjectVar)
DEFINE_RESOURCE_CAST(PrivateFontFile)
DEFINE_RESOURCE_CAST(Scrollbar)
DEFINE_RESOURCE_CAST(StringVar);
DEFINE_RESOURCE_CAST(Transport)
DEFINE_RESOURCE_CAST(URLLoader)
DEFINE_RESOURCE_CAST(URLRequestInfo)
DEFINE_RESOURCE_CAST(URLResponseInfo)
DEFINE_RESOURCE_CAST(Var)
DEFINE_RESOURCE_CAST(VideoDecoder)
DEFINE_RESOURCE_CAST(Widget)

#undef DEFINE_RESOURCE_CAST
}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_
