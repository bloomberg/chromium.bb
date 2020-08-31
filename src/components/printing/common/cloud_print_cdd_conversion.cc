// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/cloud_print_cdd_conversion.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/cloud_devices/common/printer_description.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"

#if defined(OS_CHROMEOS)
#include "base/feature_list.h"
#include "printing/printing_features.h"
#endif  // defined(OS_CHROMEOS)

namespace printer = cloud_devices::printer;

namespace cloud_print {

namespace {

printer::DuplexType ToCloudDuplexType(printing::mojom::DuplexMode mode) {
  switch (mode) {
    case printing::mojom::DuplexMode::kSimplex:
      return printer::DuplexType::NO_DUPLEX;
    case printing::mojom::DuplexMode::kLongEdge:
      return printer::DuplexType::LONG_EDGE;
    case printing::mojom::DuplexMode::kShortEdge:
      return printer::DuplexType::SHORT_EDGE;
    default:
      NOTREACHED();
  }
  return printer::DuplexType::NO_DUPLEX;
}

#if defined(OS_CHROMEOS)
printer::TypedValueVendorCapability::ValueType ToCloudValueType(
    base::Value::Type type) {
  switch (type) {
    case base::Value::Type::BOOLEAN:
      return printer::TypedValueVendorCapability::ValueType::BOOLEAN;
    case base::Value::Type::DOUBLE:
      return printer::TypedValueVendorCapability::ValueType::FLOAT;
    case base::Value::Type::INTEGER:
      return printer::TypedValueVendorCapability::ValueType::INTEGER;
    case base::Value::Type::STRING:
      return printer::TypedValueVendorCapability::ValueType::STRING;
    default:
      NOTREACHED();
  }
  return printer::TypedValueVendorCapability::ValueType::STRING;
}
#endif  // defined(OS_CHROMEOS)

printer::Media ConvertPaperToMedia(
    const printing::PrinterSemanticCapsAndDefaults::Paper& paper) {
  gfx::Size paper_size = paper.size_um;
  if (paper_size.width() > paper_size.height())
    paper_size.SetSize(paper_size.height(), paper_size.width());
  printer::Media new_media(paper.display_name, paper.vendor_id,
                           paper_size.width(), paper_size.height());
  new_media.MatchBySize();
  return new_media;
}

printer::MediaCapability GetMediaCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::MediaCapability media_capabilities;
  bool is_default_set = false;

  printer::Media default_media(semantic_info.default_paper.display_name,
                               semantic_info.default_paper.vendor_id,
                               semantic_info.default_paper.size_um.width(),
                               semantic_info.default_paper.size_um.height());
  default_media.MatchBySize();

  for (const auto& paper : semantic_info.papers) {
    printer::Media new_media = ConvertPaperToMedia(paper);
    if (!new_media.IsValid())
      continue;

    if (media_capabilities.Contains(new_media))
      continue;

    if (!default_media.IsValid())
      default_media = new_media;
    media_capabilities.AddDefaultOption(new_media, new_media == default_media);
    is_default_set = is_default_set || (new_media == default_media);
  }
  if (!is_default_set && default_media.IsValid())
    media_capabilities.AddDefaultOption(default_media, true);

  // Allow user defined paper sizes to be repeats of existing paper sizes.
  // Do not allow user defined paper sizes to be the default, for now.
  // TODO(thestig): Figure out the default paper policy here.
  for (const auto& paper : semantic_info.user_defined_papers) {
    printer::Media new_media = ConvertPaperToMedia(paper);
    if (!new_media.IsValid())
      continue;

    media_capabilities.AddOption(new_media);
  }
  return media_capabilities;
}

printer::DpiCapability GetDpiCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::DpiCapability dpi_capabilities;
  bool is_default_set = false;

  printer::Dpi default_dpi(semantic_info.default_dpi.width(),
                           semantic_info.default_dpi.height());
  for (const auto& dpi : semantic_info.dpis) {
    printer::Dpi new_dpi(dpi.width(), dpi.height());
    if (!new_dpi.IsValid())
      continue;

    if (dpi_capabilities.Contains(new_dpi))
      continue;

    if (!default_dpi.IsValid())
      default_dpi = new_dpi;
    dpi_capabilities.AddDefaultOption(new_dpi, new_dpi == default_dpi);
    is_default_set = is_default_set || (new_dpi == default_dpi);
  }
  if (!is_default_set && default_dpi.IsValid())
    dpi_capabilities.AddDefaultOption(default_dpi, true);

  return dpi_capabilities;
}

#if defined(OS_CHROMEOS)
printer::VendorCapabilities GetVendorCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::VendorCapabilities vendor_capabilities;
  for (const auto& capability : semantic_info.advanced_capabilities) {
    std::string capability_name = capability.display_name.empty()
                                      ? capability.name
                                      : capability.display_name;
    if (capability.values.empty()) {
      vendor_capabilities.AddOption(
          printer::VendorCapability(capability.name, capability_name,
                                    printer::TypedValueVendorCapability(
                                        ToCloudValueType(capability.type))));
      continue;
    }

    printer::SelectVendorCapability select_capability;
    for (const auto& value : capability.values) {
      std::string localized_value =
          value.display_name.empty() ? value.name : value.display_name;
      select_capability.AddDefaultOption(
          printer::SelectVendorCapabilityOption(value.name, localized_value),
          value.name == capability.default_value);
    }
    vendor_capabilities.AddOption(printer::VendorCapability(
        capability.name, capability_name, std::move(select_capability)));
  }

  return vendor_capabilities;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

base::Value PrinterSemanticCapsAndDefaultsToCdd(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  cloud_devices::CloudDeviceDescription description;

  printer::ContentTypesCapability content_types;
  content_types.AddOption("application/pdf");
  content_types.SaveTo(&description);

  if (semantic_info.collate_capable) {
    printer::CollateCapability collate;
    collate.set_default_value(semantic_info.collate_default);
    collate.SaveTo(&description);
  }

  printer::Copies copies_val;
  copies_val.max_value = semantic_info.copies_max;

  printer::CopiesCapability copies_cap;
  copies_cap.set_value(copies_val);
  copies_cap.SaveTo(&description);

  if (semantic_info.duplex_modes.size() > 1) {
    printer::DuplexCapability duplex;
    for (printing::mojom::DuplexMode mode : semantic_info.duplex_modes) {
      duplex.AddDefaultOption(ToCloudDuplexType(mode),
                              semantic_info.duplex_default == mode);
    }
    duplex.SaveTo(&description);
  }

  printer::ColorCapability color;
  if (semantic_info.color_default || semantic_info.color_changeable) {
    printer::Color standard_color(printer::ColorType::STANDARD_COLOR);
    standard_color.vendor_id = base::NumberToString(semantic_info.color_model);
    color.AddDefaultOption(standard_color, semantic_info.color_default);
  }
  if (!semantic_info.color_default || semantic_info.color_changeable) {
    printer::Color standard_monochrome(printer::ColorType::STANDARD_MONOCHROME);
    standard_monochrome.vendor_id =
        base::NumberToString(semantic_info.bw_model);
    color.AddDefaultOption(standard_monochrome, !semantic_info.color_default);
  }
  color.SaveTo(&description);

  if (!semantic_info.papers.empty()) {
    printer::MediaCapability media = GetMediaCapabilities(semantic_info);
    DCHECK(media.IsValid());
    media.SaveTo(&description);
  }

  if (!semantic_info.dpis.empty()) {
    printer::DpiCapability dpi = GetDpiCapabilities(semantic_info);
    DCHECK(dpi.IsValid());
    dpi.SaveTo(&description);
  }

  printer::OrientationCapability orientation;
  orientation.AddDefaultOption(printer::OrientationType::PORTRAIT, true);
  orientation.AddOption(printer::OrientationType::LANDSCAPE);
  orientation.AddOption(printer::OrientationType::AUTO_ORIENTATION);
  orientation.SaveTo(&description);

#if defined(OS_CHROMEOS)
  printer::PinCapability pin;
  pin.set_value(semantic_info.pin_supported);
  pin.SaveTo(&description);

  if (base::FeatureList::IsEnabled(
          printing::features::kAdvancedPpdAttributes) &&
      !semantic_info.advanced_capabilities.empty()) {
    printer::VendorCapabilities vendor_capabilities =
        GetVendorCapabilities(semantic_info);
    vendor_capabilities.SaveTo(&description);
  }
#endif  // defined(OS_CHROMEOS)

  return std::move(description).ToValue();
}

}  // namespace cloud_print
