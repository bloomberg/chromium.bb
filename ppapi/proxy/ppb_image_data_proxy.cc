// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_image_data_proxy.h"

#include <string.h>  // For memcpy

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

#if !defined(OS_NACL)
#include "skia/ext/platform_canvas.h"
#include "ui/surface/transport_dib.h"
#endif

using ppapi::thunk::PPB_ImageData_API;

namespace ppapi {
namespace proxy {

namespace {

// How ImageData re-use works
// --------------------------
//
// When a plugin does ReplaceContents, it transfers the ImageData to the system
// for use as the backing store for the instance. When animating plugins (like
// video) re-creating image datas for each frame and mapping the memory has a
// high overhead. So we try to re-use these when possible.
//
// 1. Plugin does ReplaceContents and Flush and the proxy queues up an
//    asynchronous request to the renderer.
// 2. Plugin frees its ImageData reference. If it doesn't do this we can't
//    re-use it.
// 3. When the last plugin ref of an ImageData is released, we don't actually
//    delete it. Instead we put it on a queue where we hold onto it in the
//    plugin process for a short period of time.
// 4. When the Flush for the Graphics2D.ReplaceContents is executed, the proxy
//    will request the old ImageData. This is the one that's being replaced by
//    the new contents so is being abandoned, and without our caching system it
//    would get deleted at this point.
// 5. The proxy in the renderer will send NotifyUnusedImageData back to the
//    plugin process. We check if the given resource is in the queue and mark
//    it as usable.
// 6. When the plugin requests a new image data, we check our queue and if there
//    is a usable ImageData of the right size and format, we'll return it
//    instead of making a new one. Since when you're doing full frame
//    animations, generally the size doesn't change so cache hits should be
//    high.
//
// Some notes:
//
//  - We only re-use image datas when the plugin does ReplaceContents on them.
//    Theoretically we could re-use them in other cases but the lifetime
//    becomes more difficult to manage. The plugin could have used an ImageData
//    in an arbitrary number of queued up PaintImageData calls which we would
//    have to check. By doing ReplaceContents, the plugin is promising that it's
//    done with the image, so this is a good signal.
//
//  - If a flush takes a long time or there are many released image datas
//    accumulating in our queue such that some are deleted, we will have
//    released our reference by the time the renderer notifies us of an unused
//    image data. In this case we just give up.
//
//  - We maintain a per-instance cache. Some pages have many instances of
//    Flash, for example, each of a different size. If they're all animating we
//    want each to get its own image data re-use.
//
//  - We generate new resource IDs when re-use happens to try to avoid weird
//    problems if the plugin messes up its refcounting.

// Keep a cache entry for this many seconds before expiring it. We get an entry
// back from the renderer after an ImageData is swapped out, so it means the
// plugin has to be painting at least two frames for this time interval to
// get caching.
static const int kMaxAgeSeconds = 2;

// ImageDataCacheEntry ---------------------------------------------------------

struct ImageDataCacheEntry {
  ImageDataCacheEntry() : added_time(), usable(false), image() {}
  ImageDataCacheEntry(ImageData* i)
      : added_time(base::TimeTicks::Now()),
        usable(false),
        image(i) {
  }

  base::TimeTicks added_time;

  // Set to true when the renderer tells us that it's OK to re-use this iamge.
  bool usable;

  scoped_refptr<ImageData> image;
};

// ImageDataInstanceCache ------------------------------------------------------

// Per-instance cache of image datas.
class ImageDataInstanceCache {
 public:
  ImageDataInstanceCache() : next_insertion_point_(0) {}

  // These functions have the same spec as the ones in ImageDataCache.
  scoped_refptr<ImageData> Get(int width, int height,
                               PP_ImageDataFormat format);
  void Add(ImageData* image_data);
  void ImageDataUsable(ImageData* image_data);

  // Expires old entries. Returns true if there are still entries in the list,
  // false if this instance cache is now empty.
  bool ExpireEntries();

 private:
  void IncrementInsertionPoint();

  // We'll store this many ImageDatas per instance.
  const static int kCacheSize = 2;

  ImageDataCacheEntry images_[kCacheSize];

  // Index into cache where the next item will go.
  int next_insertion_point_;
};

scoped_refptr<ImageData> ImageDataInstanceCache::Get(
    int width, int height,
    PP_ImageDataFormat format) {
  // Just do a brute-force search since the cache is so small.
  for (int i = 0; i < kCacheSize; i++) {
    if (!images_[i].usable)
      continue;
    const PP_ImageDataDesc& desc = images_[i].image->desc();
    if (desc.format == format &&
        desc.size.width == width && desc.size.height == height) {
      scoped_refptr<ImageData> ret(images_[i].image);
      images_[i] = ImageDataCacheEntry();

      // Since we just removed an item, this entry is the best place to insert
      // a subsequent one.
      next_insertion_point_ = i;
      return ret;
    }
  }
  return scoped_refptr<ImageData>();
}

void ImageDataInstanceCache::Add(ImageData* image_data) {
  images_[next_insertion_point_] = ImageDataCacheEntry(image_data);
  IncrementInsertionPoint();
}

void ImageDataInstanceCache::ImageDataUsable(ImageData* image_data) {
  for (int i = 0; i < kCacheSize; i++) {
    if (images_[i].image.get() == image_data) {
      images_[i].usable = true;

      // This test is important. The renderer doesn't guarantee how many image
      // datas it has or when it notifies us when one is usable. Its possible
      // to get into situations where it's always telling us the old one is
      // usable, and then the older one immediately gets expired. Therefore,
      // if the next insertion would overwrite this now-usable entry, make the
      // next insertion overwrite some other entry to avoid the replacement.
      if (next_insertion_point_ == i)
        IncrementInsertionPoint();
      return;
    }
  }
}

bool ImageDataInstanceCache::ExpireEntries() {
  base::TimeTicks threshold_time =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(kMaxAgeSeconds);

  bool has_entry = false;
  for (int i = 0; i < kCacheSize; i++) {
    if (images_[i].image.get()) {
      // Entry present.
      if (images_[i].added_time <= threshold_time) {
        // Found an entry to expire.
        images_[i] = ImageDataCacheEntry();
        next_insertion_point_ = i;
      } else {
        // Found an entry that we're keeping.
        has_entry = true;
      }
    }
  }
  return has_entry;
}

void ImageDataInstanceCache::IncrementInsertionPoint() {
  // Go to the next location, wrapping around to get LRU.
  next_insertion_point_++;
  if (next_insertion_point_ >= kCacheSize)
    next_insertion_point_ = 0;
}

// ImageDataCache --------------------------------------------------------------

class ImageDataCache {
 public:
  ImageDataCache() : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}
  ~ImageDataCache() {}

  static ImageDataCache* GetInstance();

  // Retrieves an image data from the cache of the specified size and format if
  // one exists. If one doesn't exist, this will return a null refptr.
  scoped_refptr<ImageData> Get(PP_Instance instance,
                               int width, int height,
                               PP_ImageDataFormat format);

  // Adds the given image data to the cache. There should be no plugin
  // references to it. This may delete an older item from the cache.
  void Add(ImageData* image_data);

  // Notification from the renderer that the given image data is usable.
  void ImageDataUsable(ImageData* image_data);

  void DidDeleteInstance(PP_Instance instance);

 private:
  friend struct LeakySingletonTraits<ImageDataCache>;

  // Timer callback to expire entries for the given instance.
  void OnTimer(PP_Instance instance);

  // This class does timer calls and we don't want to run these outside of the
  // scope of the object. Technically, since this class is a leaked static,
  // this will never happen and this factory is unnecessary. However, it's
  // probably better not to make assumptions about the lifetime of this class.
  base::WeakPtrFactory<ImageDataCache> weak_factory_;

  typedef std::map<PP_Instance, ImageDataInstanceCache> CacheMap;
  CacheMap cache_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataCache);
};

// static
ImageDataCache* ImageDataCache::GetInstance() {
  return Singleton<ImageDataCache,
                   LeakySingletonTraits<ImageDataCache> >::get();
}

scoped_refptr<ImageData> ImageDataCache::Get(PP_Instance instance,
                                             int width, int height,
                                             PP_ImageDataFormat format) {
  CacheMap::iterator found = cache_.find(instance);
  if (found == cache_.end())
    return scoped_refptr<ImageData>();
  return found->second.Get(width, height, format);
}

void ImageDataCache::Add(ImageData* image_data) {
  cache_[image_data->pp_instance()].Add(image_data);

  // Schedule a timer to invalidate this entry.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      RunWhileLocked(base::Bind(&ImageDataCache::OnTimer,
                     weak_factory_.GetWeakPtr(),
                     image_data->pp_instance())),
      base::TimeDelta::FromSeconds(kMaxAgeSeconds));
}

void ImageDataCache::ImageDataUsable(ImageData* image_data) {
  CacheMap::iterator found = cache_.find(image_data->pp_instance());
  if (found != cache_.end())
    found->second.ImageDataUsable(image_data);
}

void ImageDataCache::DidDeleteInstance(PP_Instance instance) {
  cache_.erase(instance);
}

void ImageDataCache::OnTimer(PP_Instance instance) {
  CacheMap::iterator found = cache_.find(instance);
  if (found == cache_.end())
    return;
  if (!found->second.ExpireEntries()) {
    // There are no more entries for this instance, remove it from the cache.
    cache_.erase(found);
  }
}

}  // namespace

// ImageData -------------------------------------------------------------------

#if !defined(OS_NACL)
ImageData::ImageData(const HostResource& resource,
                     const PP_ImageDataDesc& desc,
                     ImageHandle handle)
    : Resource(OBJECT_IS_PROXY, resource),
      desc_(desc),
      used_in_replace_contents_(false) {
#if defined(OS_WIN)
  transport_dib_.reset(TransportDIB::CreateWithHandle(handle));
#else
  transport_dib_.reset(TransportDIB::Map(handle));
#endif  // defined(OS_WIN)
}
#else  // !defined(OS_NACL)

ImageData::ImageData(const HostResource& resource,
                     const PP_ImageDataDesc& desc,
                     const base::SharedMemoryHandle& handle)
    : Resource(OBJECT_IS_PROXY, resource),
      desc_(desc),
      shm_(handle, false /* read_only */),
      size_(desc.size.width * desc.size.height * 4),
      map_count_(0),
      used_in_replace_contents_(false) {
}
#endif  // else, !defined(OS_NACL)

ImageData::~ImageData() {
}

PPB_ImageData_API* ImageData::AsPPB_ImageData_API() {
  return this;
}

void ImageData::LastPluginRefWasDeleted() {
  // The plugin no longer needs this ImageData, add it to our cache if it's
  // been used in a ReplaceContents. These are the ImageDatas that the renderer
  // will send back ImageDataUsable messages for.
  if (used_in_replace_contents_)
    ImageDataCache::GetInstance()->Add(this);
}

void ImageData::InstanceWasDeleted() {
  ImageDataCache::GetInstance()->DidDeleteInstance(pp_instance());
}

PP_Bool ImageData::Describe(PP_ImageDataDesc* desc) {
  memcpy(desc, &desc_, sizeof(PP_ImageDataDesc));
  return PP_TRUE;
}

void* ImageData::Map() {
#if defined(OS_NACL)
  if (map_count_++ == 0)
    shm_.Map(size_);
  return shm_.memory();
#else
  if (!mapped_canvas_.get()) {
    mapped_canvas_.reset(transport_dib_->GetPlatformCanvas(desc_.size.width,
                                                           desc_.size.height));
    if (!mapped_canvas_.get())
      return NULL;
  }
  const SkBitmap& bitmap =
      skia::GetTopDevice(*mapped_canvas_)->accessBitmap(true);

  bitmap.lockPixels();
  return bitmap.getAddr(0, 0);
#endif
}

void ImageData::Unmap() {
#if defined(OS_NACL)
  if (--map_count_ == 0)
    shm_.Unmap();
#else
  // TODO(brettw) have a way to unmap a TransportDIB. Currently this isn't
  // possible since deleting the TransportDIB also frees all the handles.
  // We need to add a method to TransportDIB to release the handles.
#endif
}

int32_t ImageData::GetSharedMemory(int* /* handle */,
                                   uint32_t* /* byte_count */) {
  // Not supported in the proxy (this method is for actually implementing the
  // proxy in the host).
  return PP_ERROR_NOACCESS;
}

SkCanvas* ImageData::GetPlatformCanvas() {
#if defined(OS_NACL)
  return NULL;  // No canvas in NaCl.
#else
  return mapped_canvas_.get();
#endif
}

SkCanvas* ImageData::GetCanvas() {
#if defined(OS_NACL)
  return NULL;  // No canvas in NaCl.
#else
  return mapped_canvas_.get();
#endif
}

void ImageData::SetUsedInReplaceContents() {
  used_in_replace_contents_ = true;
}

void ImageData::RecycleToPlugin(bool zero_contents) {
  used_in_replace_contents_ = false;
  if (zero_contents) {
    void* data = Map();
    memset(data, 0, desc_.stride * desc_.size.height);
    Unmap();
  }
}

#if !defined(OS_NACL)
// static
ImageHandle ImageData::NullHandle() {
#if defined(OS_WIN)
  return NULL;
#elif defined(OS_MACOSX) || defined(OS_ANDROID)
  return ImageHandle();
#else
  return 0;
#endif
}

ImageHandle ImageData::HandleFromInt(int32_t i) {
#if defined(OS_WIN)
    return reinterpret_cast<ImageHandle>(i);
#elif defined(OS_MACOSX) || defined(OS_ANDROID)
    return ImageHandle(i, false);
#else
    return static_cast<ImageHandle>(i);
#endif
}
#endif  // !defined(OS_NACL)

// PPB_ImageData_Proxy ---------------------------------------------------------

PPB_ImageData_Proxy::PPB_ImageData_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_ImageData_Proxy::~PPB_ImageData_Proxy() {
}

// static
PP_Resource PPB_ImageData_Proxy::CreateProxyResource(PP_Instance instance,
                                                     PP_ImageDataFormat format,
                                                     const PP_Size& size,
                                                     PP_Bool init_to_zero) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  // Check the cache.
  scoped_refptr<ImageData> cached_image_data =
      ImageDataCache::GetInstance()->Get(instance, size.width, size.height,
                                         format);
  if (cached_image_data.get()) {
    // We have one we can re-use rather than allocating a new one.
    cached_image_data->RecycleToPlugin(PP_ToBool(init_to_zero));
    return cached_image_data->GetReference();
  }

  HostResource result;
  std::string image_data_desc;
#if defined(OS_NACL)
  ppapi::proxy::SerializedHandle image_handle_wrapper;
  dispatcher->Send(new PpapiHostMsg_PPBImageData_CreateNaCl(
      kApiID, instance, format, size, init_to_zero,
      &result, &image_data_desc, &image_handle_wrapper));
  if (!image_handle_wrapper.is_shmem())
    return 0;
  base::SharedMemoryHandle image_handle = image_handle_wrapper.shmem();
#else
  ImageHandle image_handle = ImageData::NullHandle();
  dispatcher->Send(new PpapiHostMsg_PPBImageData_Create(
      kApiID, instance, format, size, init_to_zero,
      &result, &image_data_desc, &image_handle));
#endif

  if (result.is_null() || image_data_desc.size() != sizeof(PP_ImageDataDesc))
    return 0;

  // We serialize the PP_ImageDataDesc just by copying to a string.
  PP_ImageDataDesc desc;
  memcpy(&desc, image_data_desc.data(), sizeof(PP_ImageDataDesc));

  return (new ImageData(result, desc, image_handle))->GetReference();
}

bool PPB_ImageData_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_ImageData_Proxy, msg)
#if !defined(OS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_Create, OnHostMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_CreateNaCl,
                        OnHostMsgCreateNaCl)
#endif
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBImageData_NotifyUnusedImageData,
                        OnPluginMsgNotifyUnusedImageData)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if !defined(OS_NACL)
void PPB_ImageData_Proxy::OnHostMsgCreate(PP_Instance instance,
                                          int32_t format,
                                          const PP_Size& size,
                                          PP_Bool init_to_zero,
                                          HostResource* result,
                                          std::string* image_data_desc,
                                          ImageHandle* result_image_handle) {
  *result_image_handle = ImageData::NullHandle();

  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return;

  PP_Resource resource = enter.functions()->CreateImageData(
      instance, static_cast<PP_ImageDataFormat>(format), size, init_to_zero);
  if (!resource)
    return;
  result->SetHostResource(instance, resource);

  // Get the description, it's just serialized as a string.
  thunk::EnterResourceNoLock<PPB_ImageData_API> enter_resource(resource, false);
  PP_ImageDataDesc desc;
  if (enter_resource.object()->Describe(&desc) == PP_TRUE) {
    image_data_desc->resize(sizeof(PP_ImageDataDesc));
    memcpy(&(*image_data_desc)[0], &desc, sizeof(PP_ImageDataDesc));
  }

  // Get the shared memory handle.
  uint32_t byte_count = 0;
  int32_t handle = 0;
  if (enter_resource.object()->GetSharedMemory(&handle, &byte_count) == PP_OK) {
#if defined(OS_WIN)
    ImageHandle ih = ImageData::HandleFromInt(handle);
    *result_image_handle = dispatcher()->ShareHandleWithRemote(ih, false);
#else
    *result_image_handle = ImageData::HandleFromInt(handle);
#endif  // defined(OS_WIN)
  }
}

void PPB_ImageData_Proxy::OnHostMsgCreateNaCl(
    PP_Instance instance,
    int32_t format,
    const PP_Size& size,
    PP_Bool init_to_zero,
    HostResource* result,
    std::string* image_data_desc,
    ppapi::proxy::SerializedHandle* result_image_handle) {
  result_image_handle->set_null_shmem();
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;

  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return;

  PP_Resource resource = enter.functions()->CreateImageDataNaCl(
      instance, static_cast<PP_ImageDataFormat>(format), size, init_to_zero);
  if (!resource)
    return;
  result->SetHostResource(instance, resource);

  // Get the description, it's just serialized as a string.
  thunk::EnterResourceNoLock<PPB_ImageData_API> enter_resource(resource, false);
  if (enter_resource.failed())
    return;
  PP_ImageDataDesc desc;
  if (enter_resource.object()->Describe(&desc) == PP_TRUE) {
    image_data_desc->resize(sizeof(PP_ImageDataDesc));
    memcpy(&(*image_data_desc)[0], &desc, sizeof(PP_ImageDataDesc));
  }
  int local_fd;
  uint32_t byte_count;
  if (enter_resource.object()->GetSharedMemory(&local_fd, &byte_count) != PP_OK)
    return;
  // TODO(dmichael): Change trusted interface to return a PP_FileHandle, those
  // casts are ugly.
  base::PlatformFile platform_file =
#if defined(OS_WIN)
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(local_fd));
#elif defined(OS_POSIX)
      local_fd;
#else
  #error Not implemented.
#endif  // defined(OS_WIN)
  result_image_handle->set_shmem(
      dispatcher->ShareHandleWithRemote(platform_file, false),
      byte_count);
}
#endif  // !defined(OS_NACL)

void PPB_ImageData_Proxy::OnPluginMsgNotifyUnusedImageData(
    const HostResource& old_image_data) {
  PluginGlobals* plugin_globals = PluginGlobals::Get();
  if (!plugin_globals)
    return;  // This may happen if the plugin is maliciously sending this
             // message to the renderer.

  EnterPluginFromHostResource<PPB_ImageData_API> enter(old_image_data);
  if (enter.succeeded()) {
    ImageData* image_data = static_cast<ImageData*>(enter.object());
    ImageDataCache::GetInstance()->ImageDataUsable(image_data);
  }

  // The renderer sent us a reference with the message. If the image data was
  // still cached in our process, the proxy still holds a reference so we can
  // remove the one the renderer just sent is. If the proxy no longer holds a
  // reference, we released everything and we should also release the one the
  // renderer just sent us.
  dispatcher()->Send(new PpapiHostMsg_PPBCore_ReleaseResource(
      API_ID_PPB_CORE, old_image_data));
}

}  // namespace proxy
}  // namespace ppapi
