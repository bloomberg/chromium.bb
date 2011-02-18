/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
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


// Pack file.

#include "base/file_path.h"
#include "core/cross/pack.h"
#include "core/cross/bitmap.h"
#include "core/cross/draw_context.h"
#include "import/cross/archive_request.h"
#include "import/cross/raw_data.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "core/cross/file_request.h"
#include "core/cross/image_utils.h"
#include "core/cross/render_node.h"
#include "core/cross/iclass_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"
#include "utils/cross/file_path_utils.h"

namespace o3d {

O3D_DEFN_CLASS(Pack, NamedObject)

Pack::Pack(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      class_manager_(service_locator->GetService<IClassManager>()),
      object_manager_(service_locator->GetService<ObjectManager>()),
      renderer_(service_locator->GetService<Renderer>()) {
}

Pack::~Pack() {
  DLOG(INFO) << owned_objects_.size()
             << " objects still in Pack at time of deletion";
}

bool Pack::Destroy() {
  return object_manager_->DestroyPack(this);
}

bool Pack::RemoveObject(ObjectBase* object) {
  return UnregisterObject(object);
}

Transform* Pack::root() const {
  return root_.Get();
}

void Pack::set_root(Transform *root) {
  root_ = Transform::Ref(root);
}

ObjectBase* Pack::CreateObject(const String& type_name) {
  ObjectBase::Ref new_object(class_manager_->CreateObject(type_name));
  if (new_object.Get() != NULL) {
    RegisterObject(new_object);
    return new_object.Get();
  }
  return NULL;
}

ObjectBase* Pack::CreateObjectByClass(const ObjectBase::Class* type) {
  ObjectBase::Ref new_object(class_manager_->CreateObjectByClass(type));
  if (new_object.Get() != NULL) {
    RegisterObject(new_object);
    return new_object.Get();
  }
  return NULL;
}

// Creates a new FileRequest object.
FileRequest *Pack::CreateFileRequest(const String& type) {
  FileRequest *request = FileRequest::Create(service_locator(),
                                             this,
                                             FileRequest::TypeFromString(type));
  if (request) {
    RegisterObject(request);
  }
  return request;
}

// Creates a new ArchiveRequest object.
ArchiveRequest *Pack::CreateArchiveRequest() {
  ArchiveRequest *request = ArchiveRequest::Create(service_locator(),
                                                   this);
  if (request) {
    RegisterObject(request);
  }
  return request;
}

// Creates a Texture object from a file in the current render context format.
Texture* Pack::CreateTextureFromFile(const String& uri,
                                     const FilePath& filepath,
                                     image::ImageFileType file_type,
                                     bool generate_mipmaps) {
  String filename = FilePathToUTF8(filepath);

  DLOG(INFO) << "CreateTextureFromFile(uri='" << uri
             << "', filename='" << filename << "')";

  // TODO(gman): Add support for volume texture when we have code to load them.
  BitmapRefArray bitmaps;
  if (!Bitmap::LoadFromFile(service_locator(), filepath, file_type, &bitmaps)) {
    O3D_ERROR(service_locator())
        << "Failed to load bitmap file \"" << uri << "\"";
    return NULL;
  }

  return CreateTextureFromBitmaps(bitmaps, uri, generate_mipmaps);
}

// Creates a Texture object from a file in the current render context format.
// This version takes a String |filename| argument instead of the preferred
// FilePath argument.  The use of this method should be phased out
Texture* Pack::CreateTextureFromFile(const String& uri,
                                     const String& filename,
                                     image::ImageFileType file_type,
                                     bool generate_mipmaps) {
  FilePath filepath = UTF8ToFilePath(filename);
  return CreateTextureFromFile(uri,
                               filepath,
                               file_type,
                               generate_mipmaps);
}

// Creates a Texture object from a bitmap in the current render context format.
Texture* Pack::CreateTextureFromBitmaps(
    const BitmapRefArray& bitmaps, const String& uri, bool generate_mipmaps) {
  if (bitmaps.empty()) {
    return NULL;
  }
  unsigned width = bitmaps[0]->width();
  unsigned height = bitmaps[0]->height();

  BitmapRefArray temp_bitmaps;
  for (BitmapRefArray::size_type ii = 0; ii < bitmaps.size(); ++ii) {
    temp_bitmaps.push_back(bitmaps[ii]);
    Bitmap* bitmap = temp_bitmaps[ii].Get();
    if (bitmap->width() > static_cast<unsigned int>(Texture::kMaxDimension) ||
        bitmap->height() > static_cast<unsigned int>(Texture::kMaxDimension)) {
      O3D_ERROR(service_locator())
          << "Bitmap (uri='" << uri
          << "', size="  << bitmap->width() << "x" << bitmap->height()
          << ", mips=" << bitmap->num_mipmaps()<< ") is larger than the "
          << "maximum texture size which is (" << Texture::kMaxDimension
          << "x" << Texture::kMaxDimension << ")";
      return NULL;
    }
    if (bitmap->width() != width || bitmap->height() != height) {
      O3D_ERROR(service_locator()) << "Bitmaps are not all the same dimensions";
      return NULL;
    }
    if (generate_mipmaps && image::CanMakeMips(bitmap->format())) {
      unsigned total_mips = image::ComputeMipMapCount(
        bitmap->width(), bitmap->height());
      // If we don't already have mips and we could use them then make them.
      if (bitmap->num_mipmaps() == 1 && total_mips > 1) {
        // Create a new Bitmap with mips
        Bitmap::Ref new_bitmap(new Bitmap(service_locator()));
        new_bitmap->Allocate(bitmap->format(),
                             bitmap->width(),
                             bitmap->height(),
                             total_mips,
                             bitmap->semantic());
        new_bitmap->SetRect(0, 0, 0, bitmap->width(), bitmap->height(),
                            bitmap->GetMipData(0), bitmap->GetMipPitch(0));
        new_bitmap->GenerateMips(0, total_mips - 1);
        temp_bitmaps[ii] = new_bitmap;
      }
    }
  }

  // Figure out what kind of texture to make.
  // TODO(gman): Refactor to check semantics to distinguish between CUBE and
  // VOLUME textures.
  Texture::Ref texture;
  if (temp_bitmaps.size() == 1) {
    Bitmap* bitmap = temp_bitmaps[0].Get();
    Texture2D::Ref texture_2d = renderer_->CreateTexture2D(
        bitmap->width(),
        bitmap->height(),
        bitmap->format(),
        bitmap->num_mipmaps(),
        false);
    if (!texture_2d.IsNull()) {
      texture = Texture::Ref(texture_2d.Get());
      texture_2d->SetFromBitmap(*bitmap);
    }
  } else if (temp_bitmaps.size() == 6) {
    Bitmap* bitmap = temp_bitmaps[0].Get();
    TextureCUBE::Ref texture_cube = renderer_->CreateTextureCUBE(
        bitmap->width(),
        bitmap->format(),
        bitmap->num_mipmaps(),
        false);
    if (!texture_cube.IsNull()) {
      texture = Texture::Ref(texture_cube.Get());
      for (unsigned ii = 0; ii < 6; ++ii) {
        texture_cube->SetFromBitmap(static_cast<TextureCUBE::CubeFace>(ii),
                                    *temp_bitmaps[ii].Get());
      }
    }
  }

  if (!texture.IsNull()) {
    // TODO(gman): remove this. Maybe set the name? or make uri an offical
    //    Texture param.
    ParamString* param = texture->CreateParam<ParamString>(
        O3D_STRING_CONSTANT("uri"));
    DCHECK(param != NULL);
    param->set_value(uri);

    RegisterObject(texture);
  } else {
    O3D_ERROR(service_locator())
        << "Unable to create texture (uri='" << uri
        << "', size="  << width << "x" << height
        << ", mips=" << temp_bitmaps[0]->num_mipmaps() << ")";
  }

  return texture.Get();
}

// Creates a Texture object from RawData and allocates
// the necessary resources for it.
Texture* Pack::CreateTextureFromRawData(RawData *raw_data,
                                        bool generate_mips) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  const String uri = raw_data->uri();

  DLOG(INFO) << "CreateTextureFromRawData(uri='" << uri << "')";

  BitmapRefArray bitmap_refs;
  if (!Bitmap::LoadFromRawData(raw_data, image::UNKNOWN, &bitmap_refs)) {
    O3D_ERROR(service_locator())
        << "Failed to load bitmap from raw data \"" << uri << "\"";
    return NULL;
  }

  return CreateTextureFromBitmaps(bitmap_refs, uri, generate_mips);
}

// Create a bitmap object.
Bitmap* Pack::CreateBitmap(int width, int height, Texture::Format format) {
  DCHECK(image::CheckImageDimensions(width, height));

  Bitmap::Ref bitmap(new Bitmap(service_locator()));
  if (bitmap.IsNull()) {
    O3D_ERROR(service_locator())
        << "Failed to create bitmap object.";
    return NULL;
  }
  bitmap->Allocate(format, width, height, 1, Bitmap::IMAGE);
  if (!bitmap->image_data()) {
    O3D_ERROR(service_locator())
        << "Failed to allocate memory for bitmap.";
    return NULL;
  }
  RegisterObject(bitmap);
  return bitmap.Get();
}

// Create a new bitmap object from rawdata.
std::vector<Bitmap*> Pack::CreateBitmapsFromRawData(RawData* raw_data) {
  BitmapRefArray bitmap_refs;
  if (!Bitmap::LoadFromRawData(raw_data, image::UNKNOWN, &bitmap_refs)) {
    O3D_ERROR(service_locator())
        << "Failed to load bitmap from raw data.";
  }
  std::vector<Bitmap*> bitmaps(bitmap_refs.size(), NULL);
  for (BitmapRefArray::size_type ii = 0; ii < bitmap_refs.size(); ++ii) {
    RegisterObject(bitmap_refs[ii]);
    bitmaps[ii] = bitmap_refs[ii].Get();
  }
  return bitmaps;
}

// Creates a new RawData from a data URL.
RawData* Pack::CreateRawDataFromDataURL(const String& data_url) {
  RawData::Ref raw_data = RawData::CreateFromDataURL(service_locator(),
                                                     data_url);

  if (!raw_data.IsNull()) {
    RegisterObject(raw_data);
  }
  return raw_data.Get();
}

// Creates a Texture2D object and allocates the necessary resources for it.
Texture2D* Pack::CreateTexture2D(int width,
                                 int height,
                                 Texture::Format format,
                                 int levels,
                                 bool enable_render_surfaces) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (width > Texture::kMaxDimension ||
      height > Texture::kMaxDimension) {
    O3D_ERROR(service_locator())
        << "Maximum texture size is (" << Texture::kMaxDimension << "x"
        << Texture::kMaxDimension << ")";
    return NULL;
  }

  if (enable_render_surfaces) {
    if (image::ComputePOTSize(width) != static_cast<unsigned int>(width) ||
        image::ComputePOTSize(height) != static_cast<unsigned int>(height)) {
      O3D_ERROR(service_locator()) <<
          "Textures with RenderSurfaces enabled must have power-of-two "
          "dimensions.";
      return NULL;
    }
  }

  Texture2D::Ref texture = renderer_->CreateTexture2D(
      width,
      height,
      format,
      (levels == 0) ? image::ComputeMipMapCount(width, height) : levels,
      enable_render_surfaces);
  if (!texture.IsNull()) {
    RegisterObject(texture);
  }

  return texture.Get();
}

// Creates a TextureCUBE object and allocates the necessary resources for it.
TextureCUBE* Pack::CreateTextureCUBE(int edge_length,
                                     Texture::Format format,
                                     int levels,
                                     bool enable_render_surfaces) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (edge_length > Texture::kMaxDimension) {
    O3D_ERROR(service_locator())
        << "Maximum edge_length is " << Texture::kMaxDimension;
    return NULL;
  }


  if (enable_render_surfaces) {
    if (image::ComputePOTSize(edge_length) !=
        static_cast<unsigned int>(edge_length)) {
      O3D_ERROR(service_locator()) <<
          "Textures with RenderSurfaces enabled must have power-of-two "
          "dimensions.";
      return NULL;
    }
  }

  TextureCUBE::Ref texture = renderer_->CreateTextureCUBE(
      edge_length,
      format,
      (levels == 0) ? image::ComputeMipMapCount(edge_length,
                                                edge_length) : levels,
      enable_render_surfaces);
  if (!texture.IsNull()) {
    RegisterObject(texture);
  }

  return texture.Get();
}

RenderDepthStencilSurface* Pack::CreateDepthStencilSurface(int width,
                                                           int height) {
  if (!renderer_) {
    O3D_ERROR(service_locator()) << "No Render Device Available";
    return NULL;
  }

  if (width > Texture::kMaxDimension ||
      height > Texture::kMaxDimension) {
    O3D_ERROR(service_locator())
        << "Maximum texture size is (" << Texture::kMaxDimension << "x"
        << Texture::kMaxDimension << ")";
    return NULL;
  }

  if (image::ComputePOTSize(width) != static_cast<unsigned int>(width) ||
      image::ComputePOTSize(height) != static_cast<unsigned int>(height)) {
    O3D_ERROR(service_locator()) <<
        "Depth-stencil RenderSurfaces must have power-of-two dimensions.";
    return NULL;
  }

  RenderDepthStencilSurface::Ref surface =
      renderer_->CreateDepthStencilSurface(width, height);

  if (!surface.IsNull()) {
    RegisterObject(surface);
  }
  return surface.Get();
}

ObjectBase* Pack::GetObjectBaseById(Id id,
                                    const ObjectBase::Class* class_type) {
  ObjectBase::Ref object(object_manager_->GetObjectBaseById(id, class_type));
  if (!object.IsNull() &&
      owned_objects_.find(object) == owned_objects_.end()) {
    object.Reset();
  }
  return object.Get();
}

ObjectBaseArray Pack::GetObjects(const String& name,
                                 const String& class_type_name) const {
  ObjectBaseArray objects;
  ObjectSet::const_iterator end(owned_objects_.end());
  for (ObjectSet::const_iterator iter(owned_objects_.begin());
       iter != end;
       ++iter) {
    ObjectBase* object = iter->Get();
    if (object->IsAClassName(class_type_name)) {
      if (object->IsA(NamedObjectBase::GetApparentClass())) {
        if (name.compare(down_cast<NamedObjectBase*>(object)->name()) == 0) {
          objects.push_back(object);
        }
      }
    }
  }
  return objects;
}

ObjectBaseArray Pack::GetObjectsByClassName(
    const String& class_type_name) const {
  ObjectBaseArray objects;
  ObjectSet::const_iterator end(owned_objects_.end());
  for (ObjectSet::const_iterator iter(owned_objects_.begin());
       iter != end;
       ++iter) {
    if (iter->Get()->IsAClassName(class_type_name)) {
      objects.push_back(iter->Get());
    }
  }
  return objects;
}

void Pack::RegisterObject(ObjectBase *object) {
  ObjectBase::Ref temp(object);
  DLOG_ASSERT(owned_objects_.find(temp) == owned_objects_.end())
      << "attempt to register duplicate object in pack.";
  owned_objects_.insert(temp);
}

bool Pack::UnregisterObject(ObjectBase *object) {
  ObjectSet::iterator find(owned_objects_.find(ObjectBase::Ref(object)));
  if (find == owned_objects_.end())
    return false;

  owned_objects_.erase(find);
  return true;
}
}  // namespace o3d
