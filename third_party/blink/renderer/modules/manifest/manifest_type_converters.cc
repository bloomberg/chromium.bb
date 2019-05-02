// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_type_converters.h"

#include <utility>

#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

blink::mojom::blink::ManifestPtr
TypeConverter<blink::mojom::blink::ManifestPtr,
              const blink::Manifest*>::Convert(const blink::Manifest* input) {
  auto output = blink::mojom::blink::Manifest::New();
  if (!input->name.is_null()) {
    output->name =
        WTF::String(blink::WebString::FromUTF16(input->name.string()));
  }

  if (!input->short_name.is_null()) {
    output->short_name =
        WTF::String(blink::WebString::FromUTF16(input->short_name.string()));
  }

  output->start_url = blink::KURL(input->start_url);
  output->display = input->display;
  output->orientation = input->orientation;

  if (input->icons.size() > 0) {
    WTF::Vector<blink::mojom::blink::ManifestImageResourcePtr> mojo_icons;
    mojo_icons.ReserveInitialCapacity(
        static_cast<WTF::wtf_size_t>(input->icons.size()));
    for (auto& icon : input->icons) {
      mojo_icons.push_back(
          blink::mojom::blink::ManifestImageResource::From(&icon));
    }
    output->icons = std::move(mojo_icons);
  }

  if (input->share_target.has_value()) {
    output->share_target = blink::mojom::blink::ManifestShareTarget::From(
        &input->share_target.value());
  }

  if (input->file_handler.has_value()) {
    WTF::Vector<blink::mojom::blink::ManifestFileFilterPtr> file_filters;
    file_filters.ReserveInitialCapacity(
        static_cast<WTF::wtf_size_t>(input->file_handler->files.size()));
    for (auto& file_handler : input->file_handler->files) {
      file_filters.push_back(
          blink::mojom::blink::ManifestFileFilter::From(&file_handler));
    }
    output->file_handler->action = blink::KURL(input->file_handler->action);
    output->file_handler->files = std::move(file_filters);
  }

  if (input->related_applications.size() > 0) {
    WTF::Vector<blink::mojom::blink::ManifestRelatedApplicationPtr>
        mojo_related_applications;
    mojo_related_applications.ReserveInitialCapacity(
        static_cast<WTF::wtf_size_t>(input->related_applications.size()));
    for (auto& related_application : input->related_applications) {
      mojo_related_applications.push_back(
          blink::mojom::blink::ManifestRelatedApplication::From(
              &related_application));
    }
    output->related_applications = std::move(mojo_related_applications);
  }

  output->prefer_related_applications = input->prefer_related_applications;

  output->has_theme_color = input->theme_color.has_value();
  if (output->has_theme_color)
    output->theme_color = input->theme_color.value();

  output->has_background_color = input->background_color.has_value();
  if (output->has_background_color)
    output->background_color = input->background_color.value();

  output->splash_screen_url = blink::KURL(input->splash_screen_url);

  if (!input->gcm_sender_id.is_null()) {
    output->gcm_sender_id =
        WTF::String(blink::WebString::FromUTF16(input->gcm_sender_id.string()));
  }

  output->scope = blink::KURL(input->scope);

  return output;
}

blink::mojom::blink::ManifestImageResourcePtr
TypeConverter<blink::mojom::blink::ManifestImageResourcePtr,
              const blink::Manifest::ImageResource*>::
    Convert(const blink::Manifest::ImageResource* input) {
  auto output = blink::mojom::blink::ManifestImageResource::New();
  output->src = blink::KURL(input->src);
  output->type = WTF::String(blink::WebString::FromUTF16(input->type));

  if (input->sizes.size() > 0) {
    output->sizes.ReserveInitialCapacity(
        static_cast<WTF::wtf_size_t>(input->sizes.size()));
    for (auto& size : input->sizes)
      output->sizes.push_back(blink::WebSize(size));
  }

  if (input->purpose.size() > 0) {
    output->purpose.ReserveInitialCapacity(
        static_cast<WTF::wtf_size_t>(input->purpose.size()));
    for (auto purpose : input->purpose) {
      output->purpose.push_back(
          EnumTraits<
              blink::mojom::ManifestImageResource_Purpose,
              ::blink::Manifest::ImageResource::Purpose>::ToMojom(purpose));
    }
  }

  return output;
}

blink::mojom::blink::ManifestShareTargetPtr
TypeConverter<blink::mojom::blink::ManifestShareTargetPtr,
              const blink::Manifest::ShareTarget*>::
    Convert(const blink::Manifest::ShareTarget* input) {
  auto output = blink::mojom::blink::ManifestShareTarget::New();
  output->action = blink::KURL(input->action);
  output->method =
      EnumTraits<blink::mojom::ManifestShareTarget_Method,
                 ::blink::Manifest::ShareTarget::Method>::ToMojom(input
                                                                      ->method);
  output->enctype = EnumTraits<
      blink::mojom::ManifestShareTarget_Enctype,
      ::blink::Manifest::ShareTarget::Enctype>::ToMojom(input->enctype);

  output->params =
      blink::mojom::blink::ManifestShareTargetParams::From(&input->params);

  return output;
}

blink::mojom::blink::ManifestShareTargetParamsPtr
TypeConverter<blink::mojom::blink::ManifestShareTargetParamsPtr,
              const blink::Manifest::ShareTargetParams*>::
    Convert(const blink::Manifest::ShareTargetParams* input) {
  auto output = blink::mojom::blink::ManifestShareTargetParams::New();
  if (!input->title.is_null()) {
    output->title =
        WTF::String(blink::WebString::FromUTF16(input->title.string()));
  }

  if (!input->text.is_null()) {
    output->text =
        WTF::String(blink::WebString::FromUTF16(input->text.string()));
  }

  if (!input->url.is_null())
    output->url = WTF::String(blink::WebString::FromUTF16(input->url.string()));

  if (input->files.size() > 0) {
    WTF::Vector<blink::mojom::blink::ManifestFileFilterPtr> mojo_files;
    mojo_files.ReserveInitialCapacity(
        static_cast<WTF::wtf_size_t>(input->files.size()));
    for (auto& file : input->files) {
      mojo_files.push_back(
          blink::mojom::blink::ManifestFileFilter::From(&file));
    }
    output->files = std::move(mojo_files);
  }

  return output;
}

blink::mojom::blink::ManifestFileFilterPtr
TypeConverter<blink::mojom::blink::ManifestFileFilterPtr,
              const blink::Manifest::FileFilter*>::
    Convert(const blink::Manifest::FileFilter* input) {
  auto output = blink::mojom::blink::ManifestFileFilter::New();
  output->name = WTF::String(blink::WebString::FromUTF16(input->name));

  if (input->accept.size() > 0) {
    for (auto& accept : input->accept) {
      output->accept.push_back(
          WTF::String(blink::WebString::FromUTF16(accept)));
    }
  }

  return output;
}

blink::mojom::blink::ManifestRelatedApplicationPtr
TypeConverter<blink::mojom::blink::ManifestRelatedApplicationPtr,
              const blink::Manifest::RelatedApplication*>::
    Convert(const blink::Manifest::RelatedApplication* input) {
  auto output = blink::mojom::blink::ManifestRelatedApplication::New();
  if (!input->platform.is_null()) {
    output->platform =
        WTF::String(blink::WebString::FromUTF16(input->platform.string()));
  }

  output->url = blink::KURL(input->url);

  if (!input->id.is_null())
    output->id = WTF::String(blink::WebString::FromUTF16(input->id.string()));

  return output;
}

}  // namespace mojo
