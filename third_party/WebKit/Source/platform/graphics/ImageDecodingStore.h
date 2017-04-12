/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageDecodingStore_h
#define ImageDecodingStore_h

#include <memory>
#include "SkSize.h"
#include "SkTypes.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/skia/SkSizeHash.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/wtf/DoublyLinkedList.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ImageFrameGenerator;

// FUNCTION
//
// ImageDecodingStore is a class used to manage cached decoder objects.
//
// EXTERNAL OBJECTS
//
// ImageDecoder
//   A decoder object. It is used to decode raw data into bitmap images.
//
// ImageFrameGenerator
//   This is a direct user of this cache. Responsible for generating bitmap
//   images using an ImageDecoder. It contains encoded image data and is used to
//   represent one image file. It is used to index image and decoder objects in
//   the cache.
//
// THREAD SAFETY
//
// All public methods can be used on any thread.

class PLATFORM_EXPORT ImageDecodingStore final {
  USING_FAST_MALLOC(ImageDecodingStore);
  WTF_MAKE_NONCOPYABLE(ImageDecodingStore);

 public:
  static std::unique_ptr<ImageDecodingStore> Create() {
    return WTF::WrapUnique(new ImageDecodingStore);
  }
  ~ImageDecodingStore();

  static ImageDecodingStore& Instance();

  // Accesses a cached decoder object. A decoder is indexed by origin
  // (ImageFrameGenerator) and scaled size. Returns true if the cached object is
  // found.
  bool LockDecoder(const ImageFrameGenerator*,
                   const SkISize& scaled_size,
                   ImageDecoder**);
  void UnlockDecoder(const ImageFrameGenerator*, const ImageDecoder*);
  void InsertDecoder(const ImageFrameGenerator*, std::unique_ptr<ImageDecoder>);
  void RemoveDecoder(const ImageFrameGenerator*, const ImageDecoder*);

  // Remove all cache entries indexed by ImageFrameGenerator.
  void RemoveCacheIndexedByGenerator(const ImageFrameGenerator*);

  void Clear();
  void SetCacheLimitInBytes(size_t);
  size_t MemoryUsageInBytes();
  int CacheEntries();
  int DecoderCacheEntries();

 private:
  // Decoder cache entry is identified by:
  // 1. Pointer to ImageFrameGenerator.
  // 2. Size of the image.
  typedef std::pair<const ImageFrameGenerator*, SkISize> DecoderCacheKey;

  // Base class for all cache entries.
  class CacheEntry : public DoublyLinkedListNode<CacheEntry> {
    USING_FAST_MALLOC(CacheEntry);
    WTF_MAKE_NONCOPYABLE(CacheEntry);
    friend class WTF::DoublyLinkedListNode<CacheEntry>;

   public:
    enum CacheType {
      kTypeDecoder,
    };

    CacheEntry(const ImageFrameGenerator* generator, int use_count)
        : generator_(generator), use_count_(use_count), prev_(0), next_(0) {}

    virtual ~CacheEntry() { DCHECK(!use_count_); }

    const ImageFrameGenerator* Generator() const { return generator_; }
    int UseCount() const { return use_count_; }
    void IncrementUseCount() { ++use_count_; }
    void DecrementUseCount() {
      --use_count_;
      DCHECK_GE(use_count_, 0);
    }

    // FIXME: getSafeSize() returns the size in bytes truncated to a 32-bit
    // integer. Find a way to get the size in 64-bits.
    virtual size_t MemoryUsageInBytes() const = 0;
    virtual CacheType GetType() const = 0;

   protected:
    const ImageFrameGenerator* generator_;
    int use_count_;

   private:
    CacheEntry* prev_;
    CacheEntry* next_;
  };

  class DecoderCacheEntry final : public CacheEntry {
   public:
    static std::unique_ptr<DecoderCacheEntry> Create(
        const ImageFrameGenerator* generator,
        std::unique_ptr<ImageDecoder> decoder) {
      return WTF::WrapUnique(
          new DecoderCacheEntry(generator, 0, std::move(decoder)));
    }

    DecoderCacheEntry(const ImageFrameGenerator* generator,
                      int count,
                      std::unique_ptr<ImageDecoder> decoder)
        : CacheEntry(generator, count),
          cached_decoder_(std::move(decoder)),
          size_(SkISize::Make(cached_decoder_->DecodedSize().Width(),
                              cached_decoder_->DecodedSize().Height())) {}

    size_t MemoryUsageInBytes() const override {
      return size_.width() * size_.height() * 4;
    }
    CacheType GetType() const override { return kTypeDecoder; }

    static DecoderCacheKey MakeCacheKey(const ImageFrameGenerator* generator,
                                        const SkISize& size) {
      return std::make_pair(generator, size);
    }
    static DecoderCacheKey MakeCacheKey(const ImageFrameGenerator* generator,
                                        const ImageDecoder* decoder) {
      return std::make_pair(generator,
                            SkISize::Make(decoder->DecodedSize().Width(),
                                          decoder->DecodedSize().Height()));
    }
    DecoderCacheKey CacheKey() const { return MakeCacheKey(generator_, size_); }
    ImageDecoder* CachedDecoder() const { return cached_decoder_.get(); }

   private:
    std::unique_ptr<ImageDecoder> cached_decoder_;
    SkISize size_;
  };

  ImageDecodingStore();

  void Prune();

  // These helper methods are called while m_mutex is locked.
  template <class T, class U, class V>
  void InsertCacheInternal(std::unique_ptr<T> cache_entry,
                           U* cache_map,
                           V* identifier_map);

  // Helper method to remove a cache entry. Ownership is transferred to
  // deletionList. Use of Vector<> is handy when removing multiple entries.
  template <class T, class U, class V>
  void RemoveFromCacheInternal(
      const T* cache_entry,
      U* cache_map,
      V* identifier_map,
      Vector<std::unique_ptr<CacheEntry>>* deletion_list);

  // Helper method to remove a cache entry. Uses the templated version base on
  // the type of cache entry.
  void RemoveFromCacheInternal(
      const CacheEntry*,
      Vector<std::unique_ptr<CacheEntry>>* deletion_list);

  // Helper method to remove all cache entries associated with an
  // ImageFrameGenerator. Ownership of the cache entries is transferred to
  // |deletionList|.
  template <class U, class V>
  void RemoveCacheIndexedByGeneratorInternal(
      U* cache_map,
      V* identifier_map,
      const ImageFrameGenerator*,
      Vector<std::unique_ptr<CacheEntry>>* deletion_list);

  // Helper method to remove cache entry pointers from the LRU list.
  void RemoveFromCacheListInternal(
      const Vector<std::unique_ptr<CacheEntry>>& deletion_list);

  // A doubly linked list that maintains usage history of cache entries.
  // This is used for eviction of old entries.
  // Head of this list is the least recently used cache entry.
  // Tail of this list is the most recently used cache entry.
  DoublyLinkedList<CacheEntry> ordered_cache_list_;

  // A lookup table for all decoder cache objects. Owns all decoder cache
  // objects.
  typedef HashMap<DecoderCacheKey, std::unique_ptr<DecoderCacheEntry>>
      DecoderCacheMap;
  DecoderCacheMap decoder_cache_map_;

  // A lookup table to map ImageFrameGenerator to all associated
  // decoder cache keys.
  typedef HashSet<DecoderCacheKey> DecoderCacheKeySet;
  typedef HashMap<const ImageFrameGenerator*, DecoderCacheKeySet>
      DecoderCacheKeyMap;
  DecoderCacheKeyMap decoder_cache_key_map_;

  size_t heap_limit_in_bytes_;
  size_t heap_memory_usage_in_bytes_;

  // Protect concurrent access to these members:
  //   m_orderedCacheList
  //   m_decoderCacheMap and all CacheEntrys stored in it
  //   m_decoderCacheKeyMap
  //   m_heapLimitInBytes
  //   m_heapMemoryUsageInBytes
  // This mutex also protects calls to underlying skBitmap's
  // lockPixels()/unlockPixels() as they are not threadsafe.
  Mutex mutex_;
};

}  // namespace blink

#endif
