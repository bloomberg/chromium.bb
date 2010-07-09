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
class DeviceContext2D;
class DirectoryReader;
class FileChooser;
class FileIO;
class FileRef;
class Font;
class ImageData;
class PluginModule;
class Scrollbar;
class URLLoader;
class URLRequestInfo;
class URLResponseInfo;
class Widget;

class Resource : public base::RefCountedThreadSafe<Resource> {
 public:
  explicit Resource(PluginModule* module);
  virtual ~Resource();

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> GetAs(PP_Resource res) {
    scoped_refptr<Resource> resource = ResourceTracker::Get()->GetResource(res);
    return resource ? resource->Cast<T>() : NULL;
  }

  PP_Resource GetResource() const;

  PluginModule* module() const { return module_; }

  // Cast the resource into a specified type. This will return NULL if the
  // resource does not match the specified type. Specializations of this
  // template call into As* functions.
  template <typename T> T* Cast() { return NULL; }

 private:
  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type. Used by the Cast()
  // function.
  virtual Buffer* AsBuffer() { return NULL; }
  virtual DeviceContext2D* AsDeviceContext2D() { return NULL; }
  virtual DirectoryReader* AsDirectoryReader() { return NULL; }
  virtual FileChooser* AsFileChooser() { return NULL; }
  virtual FileIO* AsFileIO() { return NULL; }
  virtual FileRef* AsFileRef() { return NULL; }
  virtual Font* AsFont() { return NULL; }
  virtual ImageData* AsImageData() { return NULL; }
  virtual Scrollbar* AsScrollbar() { return NULL; }
  virtual URLLoader* AsURLLoader() { return NULL; }
  virtual URLRequestInfo* AsURLRequestInfo() { return NULL; }
  virtual URLResponseInfo* AsURLResponseInfo() { return NULL; }
  virtual Widget* AsWidget() { return NULL; }

  PluginModule* module_;  // Non-owning pointer to our module.

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

// Cast() specializations.
#define DEFINE_RESOURCE_CAST(Type)                   \
  template <> inline Type* Resource::Cast<Type>() {  \
      return As##Type();                             \
  }

DEFINE_RESOURCE_CAST(Buffer)
DEFINE_RESOURCE_CAST(DeviceContext2D)
DEFINE_RESOURCE_CAST(DirectoryReader)
DEFINE_RESOURCE_CAST(FileChooser)
DEFINE_RESOURCE_CAST(FileIO)
DEFINE_RESOURCE_CAST(FileRef)
DEFINE_RESOURCE_CAST(Font)
DEFINE_RESOURCE_CAST(ImageData)
DEFINE_RESOURCE_CAST(Scrollbar)
DEFINE_RESOURCE_CAST(URLLoader)
DEFINE_RESOURCE_CAST(URLRequestInfo)
DEFINE_RESOURCE_CAST(URLResponseInfo)
DEFINE_RESOURCE_CAST(Widget)

#undef DEFINE_RESOURCE_CAST
}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_
