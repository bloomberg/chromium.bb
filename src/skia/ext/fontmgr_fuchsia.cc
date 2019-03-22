// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/fontmgr_fuchsia.h"

#include <lib/zx/vmar.h>

#include <unordered_map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/small_map.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/skia/src/core/SkFontDescriptor.h"
#include "third_party/skia/src/core/SkMakeUnique.h"
#include "third_party/skia/src/ports/SkFontHost_FreeType_common.h"
#include "third_party/skia/src/ports/SkFontMgr_custom.h"

namespace skia {

namespace {

constexpr char kDefaultFont[] = "Roboto";

// Currently fonts::Provider doesn't support font aliases. The map below is
// used to map common web fonts to font families that are expected to be present
// in fonts::Provider.
constexpr struct {
  const char* font_name_in;
  const char* font_name_out;
} kFontMap[] = {{"sans", "Roboto"},
                {"sans-serif", "Roboto"},
                {"arial", "Roboto"},
                {"helvetica", "Roboto"},
                {"roboto", "Roboto"},
                {"roboto slab", "RobotoSlab"},
                {"serif", "RobotoSlab"},
                {"georgia", "RobotoSlab"},
                {"times", "RobotoSlab"},
                {"times roman", "RobotoSlab"},
                {"times new roman", "RobotoSlab"},
                {"roboto mono", "RobotoMono"},
                {"consolas", "RobotoMono"},
                {"courier", "RobotoMono"},
                {"courier new", "RobotoMono"},
                {"monospace", "RobotoMono"}};

fuchsia::fonts::Slant ToFontSlant(SkFontStyle::Slant slant) {
  return (slant == SkFontStyle::kItalic_Slant) ? fuchsia::fonts::Slant::ITALIC
                                               : fuchsia::fonts::Slant::UPRIGHT;
}

void UnmapMemory(const void* buffer, void* context) {
  const uintptr_t size = reinterpret_cast<uintptr_t>(context);
  zx::vmar::root_self()->unmap(reinterpret_cast<uintptr_t>(buffer), size);
}

sk_sp<SkData> BufferToSkData(fuchsia::mem::Buffer data) {
  uintptr_t buffer = 0;
  zx_status_t status = zx::vmar::root_self()->map(
      0, data.vmo, 0, data.size, ZX_VM_FLAG_PERM_READ, &buffer);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    return nullptr;
  }

  return SkData::MakeWithProc(reinterpret_cast<void*>(buffer), data.size,
                              UnmapMemory, reinterpret_cast<void*>(data.size));
}

// SkTypeface that notifies FontCache when it is being destroyed.
class CachedTypeface : public SkTypeface_Stream {
 public:
  CachedTypeface(std::unique_ptr<SkFontData> font_data,
                 const SkFontStyle& style,
                 bool is_fixed_pitch,
                 const SkString family_name,
                 base::OnceClosure on_deleted)
      : SkTypeface_Stream(std::move(font_data),
                          style,
                          is_fixed_pitch,
                          /*sys_font=*/true,
                          family_name),
        on_deleted_(std::move(on_deleted)) {}

  ~CachedTypeface() override {
    if (on_deleted_)
      std::move(on_deleted_).Run();
  }

 private:
  base::OnceClosure on_deleted_;

  DISALLOW_COPY_AND_ASSIGN(CachedTypeface);
};

sk_sp<SkTypeface> CreateTypefaceFromSkStream(
    std::unique_ptr<SkStreamAsset> stream,
    const SkFontArguments& args,
    base::OnceClosure on_deleted) {
  using Scanner = SkTypeface_FreeType::Scanner;
  Scanner scanner;
  bool is_fixed_pitch;
  SkFontStyle style;
  SkString name;
  Scanner::AxisDefinitions axis_definitions;
  if (!scanner.scanFont(stream.get(), args.getCollectionIndex(), &name, &style,
                        &is_fixed_pitch, &axis_definitions)) {
    return nullptr;
  }

  const SkFontArguments::VariationPosition position =
      args.getVariationDesignPosition();
  SkAutoSTMalloc<4, SkFixed> axis_values(axis_definitions.count());
  Scanner::computeAxisValues(axis_definitions, position, axis_values, name);

  auto font_data =
      std::make_unique<SkFontData>(std::move(stream), args.getCollectionIndex(),
                                   axis_values.get(), axis_definitions.count());
  return sk_make_sp<CachedTypeface>(std::move(font_data), style, is_fixed_pitch,
                                    name, std::move(on_deleted));
}

sk_sp<SkTypeface> CreateTypefaceFromBuffer(fuchsia::mem::Buffer buffer,
                                           base::OnceClosure on_deleted) {
  sk_sp<SkData> data = BufferToSkData(std::move(buffer));
  if (!data)
    return nullptr;

  // TODO(https://crbug.com/800156): Initialize font arguments with font index
  // when font collection support is implemented in Provider.
  SkFontArguments args;

  return CreateTypefaceFromSkStream(
      std::make_unique<SkMemoryStream>(std::move(data)), args,
      std::move(on_deleted));
}

}  // namespace

class FuchsiaFontManager::FontCache {
 public:
  FontCache();
  ~FontCache();

  sk_sp<SkTypeface> GetTypefaceFromBuffer(fuchsia::mem::Buffer buffer);

 private:
  void OnTypefaceDeleted(zx_koid_t vmo_koid);

  THREAD_CHECKER(thread_checker_);

  // SkTypeface cache. They key is koid of the VMO that contains the typeface.
  // This allows to reuse previously-created SkTypeface when fonts::Provider
  // returns FontData with the same VMO.
  base::small_map<std::unordered_map<zx_koid_t, SkTypeface*>> typefaces_;

  base::WeakPtrFactory<FontCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FontCache);
};

FuchsiaFontManager::FontCache::FontCache() : weak_factory_(this) {}

FuchsiaFontManager::FontCache::~FontCache() = default;

sk_sp<SkTypeface> FuchsiaFontManager::FontCache::GetTypefaceFromBuffer(
    fuchsia::mem::Buffer buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  zx_info_handle_basic_t vmo_info;
  zx_status_t status = buffer.vmo.get_info(ZX_INFO_HANDLE_BASIC, &vmo_info,
                                           sizeof(vmo_info), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_object_get_info";
    return nullptr;
  }

  sk_sp<SkTypeface> result;
  SkTypeface** cached_typeface = &(typefaces_[vmo_info.koid]);
  if (*cached_typeface) {
    result = sk_ref_sp(*cached_typeface);
  } else {
    result = CreateTypefaceFromBuffer(
        std::move(buffer),
        base::BindOnce(&FontCache::OnTypefaceDeleted,
                       weak_factory_.GetWeakPtr(), vmo_info.koid));
    *cached_typeface = result.get();
  }
  return result;
}

void FuchsiaFontManager::FontCache::OnTypefaceDeleted(zx_koid_t vmo_koid) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool was_found = typefaces_.erase(vmo_koid) != 0;
  DCHECK(was_found);
}

FuchsiaFontManager::FuchsiaFontManager(
    fuchsia::fonts::ProviderSyncPtr font_provider)
    : font_provider_(std::move(font_provider)), font_cache_(new FontCache()) {
  for (auto& m : kFontMap) {
    font_map_[m.font_name_in] = m.font_name_out;
  }

  // Get default font.
  default_typeface_.reset(onMatchFamilyStyle(kDefaultFont, SkFontStyle()));
  if (!default_typeface_) {
    default_typeface_ = sk_make_sp<SkTypeface_Empty>();
    LOG(ERROR) << "Failed to get default font from fonts::Provider.";
  }
}

FuchsiaFontManager::~FuchsiaFontManager() = default;

int FuchsiaFontManager::onCountFamilies() const {
  NOTREACHED();
  return 0;
}

void FuchsiaFontManager::onGetFamilyName(int index,
                                         SkString* family_name) const {
  NOTREACHED();
}

SkFontStyleSet* FuchsiaFontManager::onCreateStyleSet(int index) const {
  NOTREACHED();
  return nullptr;
}

SkFontStyleSet* FuchsiaFontManager::onMatchFamily(
    const char family_name[]) const {
  NOTREACHED();
  return nullptr;
}

SkTypeface* FuchsiaFontManager::onMatchFamilyStyle(
    const char family_name[],
    const SkFontStyle& style) const {
  std::string family_name_lowercase = base::ToLowerASCII(family_name);

  fuchsia::fonts::Request request;
  auto it = font_map_.find(family_name_lowercase);
  request.family = (it != font_map_.end()) ? it->second.c_str() : family_name;
  request.weight = style.weight();
  request.width = style.width();
  request.slant = ToFontSlant(style.slant());
  request.language = fidl::VectorPtr<fidl::StringPtr>::New(0);

  fuchsia::fonts::ResponsePtr response;
  zx_status_t status = font_provider_->GetFont(std::move(request), &response);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "Failed to query font provider.";
  } else if (response) {
    sk_sp<SkTypeface> result =
        font_cache_->GetTypefaceFromBuffer(std::move(response->buffer));
    if (result)
      return result.release();

    LOG(ERROR) << "fonts::Provider returned invalid FontData for "
               << family_name;
  }

  // If Sans was requested and we failed to get a valid response from
  // fonts::Provider then return |default_typeface_|. blink::FontCache queries
  // Sans as a last-resort font. Returning |default_typeface_| here ensures that
  // the renderer doesn't crash when fonts::Provider stops working.
  if (family_name_lowercase == "sans") {
    // Copy |default_typeface_| to increment ref-count before returning it.
    sk_sp<SkTypeface> result = default_typeface_;
    return result.release();
  }

  return nullptr;
}

SkTypeface* FuchsiaFontManager::onMatchFamilyStyleCharacter(
    const char family_name[],
    const SkFontStyle& style,
    const char* bcp47[],
    int bcp47Count,
    SkUnichar character) const {
  if (u_hasBinaryProperty(character, UCHAR_EMOJI))
    return onMatchFamilyStyle("Noto Color Emoji", style);

  return nullptr;
}

SkTypeface* FuchsiaFontManager::onMatchFaceStyle(
    const SkTypeface* typeface,
    const SkFontStyle& style) const {
  NOTREACHED();
  return nullptr;
}

sk_sp<SkTypeface> FuchsiaFontManager::onMakeFromData(sk_sp<SkData> data,
                                                     int ttc_index) const {
  return makeFromStream(std::make_unique<SkMemoryStream>(std::move(data)),
                        ttc_index);
}

sk_sp<SkTypeface> FuchsiaFontManager::onMakeFromStreamIndex(
    std::unique_ptr<SkStreamAsset> stream,
    int ttc_index) const {
  return makeFromStream(std::move(stream),
                        SkFontArguments().setCollectionIndex(ttc_index));
}

sk_sp<SkTypeface> FuchsiaFontManager::onMakeFromStreamArgs(
    std::unique_ptr<SkStreamAsset> stream,
    const SkFontArguments& args) const {
  return CreateTypefaceFromSkStream(std::move(stream), args,
                                    /*on_deleted=*/base::OnceClosure());
}

sk_sp<SkTypeface> FuchsiaFontManager::onMakeFromFile(const char path[],
                                                     int ttc_index) const {
  NOTREACHED();
  return nullptr;
}

sk_sp<SkTypeface> FuchsiaFontManager::onLegacyMakeTypeface(
    const char family_name[],
    SkFontStyle style) const {
  sk_sp<SkTypeface> typeface;

  if (family_name)
    typeface.reset(this->onMatchFamilyStyle(family_name, style));

  return typeface;
}

}  // namespace skia
