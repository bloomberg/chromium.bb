// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/manifest/manifest.h"

#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace mojo {
namespace internal {

inline base::StringPiece16 TruncateString16(const std::u16string& string) {
  // We restrict the maximum length for all the strings inside the Manifest
  // when it is sent over Mojo. The renderer process truncates the strings
  // before sending the Manifest and the browser process validates that.
  return base::StringPiece16(string).substr(0, 4 * 1024);
}

inline absl::optional<base::StringPiece16> TruncateOptionalString16(
    const absl::optional<std::u16string>& string) {
  if (!string)
    return absl::nullopt;

  return TruncateString16(*string);
}

}  // namespace internal

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestDataView, ::blink::Manifest> {
  static bool IsNull(const ::blink::Manifest& manifest) {
    return manifest.IsEmpty();
  }

  static void SetToNull(::blink::Manifest* manifest) {
    *manifest = ::blink::Manifest();
  }

  static absl::optional<base::StringPiece16> name(
      const ::blink::Manifest& manifest) {
    return internal::TruncateOptionalString16(manifest.name);
  }

  static absl::optional<base::StringPiece16> short_name(
      const ::blink::Manifest& manifest) {
    return internal::TruncateOptionalString16(manifest.short_name);
  }

  static absl::optional<base::StringPiece16> description(
      const ::blink::Manifest& manifest) {
    return internal::TruncateOptionalString16(manifest.description);
  }

  static absl::optional<base::StringPiece16> id(
      const ::blink::Manifest& manifest) {
    return internal::TruncateOptionalString16(manifest.id);
  }

  static absl::optional<base::StringPiece16> gcm_sender_id(
      const ::blink::Manifest& manifest) {
    return internal::TruncateOptionalString16(manifest.gcm_sender_id);
  }

  static const GURL& start_url(const ::blink::Manifest& manifest) {
    return manifest.start_url;
  }

  static const GURL& scope(const ::blink::Manifest& manifest) {
    return manifest.scope;
  }

  static blink::mojom::DisplayMode display(const ::blink::Manifest& manifest) {
    return manifest.display;
  }

  static const std::vector<blink::mojom::DisplayMode> display_override(
      const ::blink::Manifest& manifest) {
    return manifest.display_override;
  }

  static device::mojom::ScreenOrientationLockType orientation(
      const ::blink::Manifest& manifest) {
    return manifest.orientation;
  }

  static bool has_theme_color(const ::blink::Manifest& m) {
    return m.theme_color.has_value();
  }

  static uint32_t theme_color(const ::blink::Manifest& m) {
    return m.theme_color.value_or(0);
  }

  static bool has_background_color(const ::blink::Manifest& m) {
    return m.background_color.has_value();
  }

  static uint32_t background_color(const ::blink::Manifest& m) {
    return m.background_color.value_or(0);
  }

  static const std::vector<::blink::Manifest::ImageResource>& icons(
      const ::blink::Manifest& manifest) {
    return manifest.icons;
  }

  static const std::vector<::blink::Manifest::ImageResource>& screenshots(
      const ::blink::Manifest& manifest) {
    return manifest.screenshots;
  }

  static const std::vector<::blink::Manifest::ShortcutItem>& shortcuts(
      const ::blink::Manifest& manifest) {
    return manifest.shortcuts;
  }

  static const absl::optional<::blink::Manifest::ShareTarget>& share_target(
      const ::blink::Manifest& manifest) {
    return manifest.share_target;
  }

  static const std::vector<::blink::Manifest::FileHandler>& file_handlers(
      const ::blink::Manifest& manifest) {
    return manifest.file_handlers;
  }

  static const std::vector<::blink::Manifest::ProtocolHandler>&
  protocol_handlers(const ::blink::Manifest& manifest) {
    return manifest.protocol_handlers;
  }

  static const std::vector<::blink::Manifest::UrlHandler>& url_handlers(
      const ::blink::Manifest& manifest) {
    return manifest.url_handlers;
  }

  static const absl::optional<::blink::Manifest::NoteTaking>& note_taking(
      const ::blink::Manifest& manifest) {
    return manifest.note_taking;
  }

  static const std::vector<::blink::Manifest::RelatedApplication>&
  related_applications(const ::blink::Manifest& manifest) {
    return manifest.related_applications;
  }

  static bool prefer_related_applications(const ::blink::Manifest& manifest) {
    return manifest.prefer_related_applications;
  }

  static blink::mojom::CaptureLinks capture_links(
      const ::blink::Manifest& manifest) {
    return manifest.capture_links;
  }

  static bool isolated_storage(const ::blink::Manifest& manifest) {
    return manifest.isolated_storage;
  }

  static bool Read(blink::mojom::ManifestDataView data, ::blink::Manifest* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestImageResourceDataView,
                 ::blink::Manifest::ImageResource> {
  static const GURL& src(const ::blink::Manifest::ImageResource& icon) {
    return icon.src;
  }

  static base::StringPiece16 type(
      const ::blink::Manifest::ImageResource& icon) {
    return internal::TruncateString16(icon.type);
  }
  static const std::vector<gfx::Size>& sizes(
      const ::blink::Manifest::ImageResource& icon) {
    return icon.sizes;
  }

  static const std::vector<::blink::mojom::ManifestImageResource_Purpose>&
  purpose(const ::blink::Manifest::ImageResource& icon) {
    return icon.purpose;
  }

  static bool Read(blink::mojom::ManifestImageResourceDataView data,
                   ::blink::Manifest::ImageResource* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestShortcutItemDataView,
                 ::blink::Manifest::ShortcutItem> {
  static base::StringPiece16 name(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return internal::TruncateString16(shortcut.name);
  }

  static absl::optional<base::StringPiece16> short_name(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return internal::TruncateOptionalString16(shortcut.short_name);
  }

  static absl::optional<base::StringPiece16> description(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return internal::TruncateOptionalString16(shortcut.description);
  }

  static const GURL& url(const ::blink::Manifest::ShortcutItem& shortcut) {
    return shortcut.url;
  }

  static const std::vector<::blink::Manifest::ImageResource>& icons(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return shortcut.icons;
  }

  static bool Read(blink::mojom::ManifestShortcutItemDataView data,
                   ::blink::Manifest::ShortcutItem* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestRelatedApplicationDataView,
                 ::blink::Manifest::RelatedApplication> {
  static absl::optional<base::StringPiece16> platform(
      const ::blink::Manifest::RelatedApplication& related_application) {
    return internal::TruncateOptionalString16(related_application.platform);
  }

  static const GURL& url(
      const ::blink::Manifest::RelatedApplication& related_application) {
    return related_application.url;
  }

  static absl::optional<base::StringPiece16> id(
      const ::blink::Manifest::RelatedApplication& related_application) {
    return internal::TruncateOptionalString16(related_application.id);
  }

  static bool Read(blink::mojom::ManifestRelatedApplicationDataView data,
                   ::blink::Manifest::RelatedApplication* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestFileFilterDataView,
                 ::blink::Manifest::FileFilter> {
  static base::StringPiece16 name(
      const ::blink::Manifest::FileFilter& share_target_file) {
    return internal::TruncateString16(share_target_file.name);
  }

  static const std::vector<base::StringPiece16> accept(
      const ::blink::Manifest::FileFilter& share_target_file) {
    std::vector<base::StringPiece16> accept_types;

    for (const std::u16string& accept_type : share_target_file.accept)
      accept_types.push_back(internal::TruncateString16(accept_type));

    return accept_types;
  }

  static bool Read(blink::mojom::ManifestFileFilterDataView data,
                   ::blink::Manifest::FileFilter* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestUrlHandlerDataView,
                 ::blink::Manifest::UrlHandler> {
  static const url::Origin& origin(
      const ::blink::Manifest::UrlHandler& url_handler) {
    return url_handler.origin;
  }

  static bool has_origin_wildcard(
      const ::blink::Manifest::UrlHandler& url_handler) {
    return url_handler.has_origin_wildcard;
  }

  static bool Read(blink::mojom::ManifestUrlHandlerDataView data,
                   ::blink::Manifest::UrlHandler* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestShareTargetParamsDataView,
                 ::blink::Manifest::ShareTargetParams> {
  static const absl::optional<base::StringPiece16> text(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return internal::TruncateOptionalString16(share_target_params.text);
  }
  static const absl::optional<base::StringPiece16> title(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return internal::TruncateOptionalString16(share_target_params.title);
  }
  static const absl::optional<base::StringPiece16> url(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return internal::TruncateOptionalString16(share_target_params.url);
  }
  static const std::vector<blink::Manifest::FileFilter>& files(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return share_target_params.files;
  }

  static bool Read(blink::mojom::ManifestShareTargetParamsDataView data,
                   ::blink::Manifest::ShareTargetParams* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestShareTargetDataView,
                 ::blink::Manifest::ShareTarget> {
  static const GURL& action(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.action;
  }
  static ::blink::mojom::ManifestShareTarget_Method method(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.method;
  }
  static ::blink::mojom::ManifestShareTarget_Enctype enctype(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.enctype;
  }
  static const ::blink::Manifest::ShareTargetParams& params(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.params;
  }
  static bool Read(blink::mojom::ManifestShareTargetDataView data,
                   ::blink::Manifest::ShareTarget* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestFileHandlerDataView,
                 ::blink::Manifest::FileHandler> {
  static const GURL& action(const ::blink::Manifest::FileHandler& entry) {
    return entry.action;
  }

  static const std::u16string& name(
      const ::blink::Manifest::FileHandler& entry) {
    return entry.name;
  }

  static const std::vector<::blink::Manifest::ImageResource>& icons(
      const ::blink::Manifest::FileHandler& entry) {
    return entry.icons;
  }

  static const std::map<std::u16string, std::vector<std::u16string>>& accept(
      const ::blink::Manifest::FileHandler& entry) {
    return entry.accept;
  }

  static bool Read(blink::mojom::ManifestFileHandlerDataView data,
                   ::blink::Manifest::FileHandler* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestProtocolHandlerDataView,
                 ::blink::Manifest::ProtocolHandler> {
  static base::StringPiece16 protocol(
      const ::blink::Manifest::ProtocolHandler& protocol) {
    return internal::TruncateString16(protocol.protocol);
  }
  static const GURL& url(const ::blink::Manifest::ProtocolHandler& protocol) {
    return protocol.url;
  }
  static bool Read(blink::mojom::ManifestProtocolHandlerDataView data,
                   ::blink::Manifest::ProtocolHandler* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestNoteTakingDataView,
                 ::blink::Manifest::NoteTaking> {
  static const GURL new_note_url(
      const ::blink::Manifest::NoteTaking& note_taking) {
    return note_taking.new_note_url;
  }

  static bool Read(blink::mojom::ManifestNoteTakingDataView data,
                   ::blink::Manifest::NoteTaking* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_MOJOM_TRAITS_H_
