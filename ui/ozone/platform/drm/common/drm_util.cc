// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <algorithm>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"

#if !defined(DRM_FORMAT_R16)
// TODO(riju): crbug.com/733703
#define DRM_FORMAT_R16 fourcc_code('R', '1', '6', ' ')
#endif

namespace ui {

namespace {

static const size_t kDefaultCursorWidth = 64;
static const size_t kDefaultCursorHeight = 64;

bool IsCrtcInUse(
    uint32_t crtc,
    const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
        displays) {
  for (size_t i = 0; i < displays.size(); ++i) {
    if (crtc == displays[i]->crtc()->crtc_id)
      return true;
  }

  return false;
}

// Return a CRTC compatible with |connector| and not already used in |displays|.
// If there are multiple compatible CRTCs, the one that supports the majority of
// planes will be returned.
uint32_t GetCrtc(
    int fd,
    drmModeConnector* connector,
    drmModeRes* resources,
    const std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>&
        displays) {
  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(fd));
  std::vector<ScopedDrmPlanePtr> planes;
  for (uint32_t i = 0; i < plane_resources->count_planes; i++)
    planes.emplace_back(drmModeGetPlane(fd, plane_resources->planes[i]));

  DCHECK_GE(32, resources->count_crtcs);
  uint32_t best_crtc = 0;
  int best_crtc_planes = -1;

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoders[i]));
    if (!encoder)
      continue;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      int crtc_bit = 1 << j;
      if (!(encoder->possible_crtcs & crtc_bit) ||
          IsCrtcInUse(resources->crtcs[j], displays))
        continue;

      int supported_planes = std::count_if(
          planes.begin(), planes.end(), [crtc_bit](const ScopedDrmPlanePtr& p) {
            return p->possible_crtcs & crtc_bit;
          });

      uint32_t assigned_crtc = 0;
      if (connector->encoder_id == encoder->encoder_id)
        assigned_crtc = encoder->crtc_id;
      if (supported_planes > best_crtc_planes ||
          (supported_planes == best_crtc_planes &&
           assigned_crtc == resources->crtcs[j])) {
        best_crtc_planes = supported_planes;
        best_crtc = resources->crtcs[j];
      }
    }
  }

  return best_crtc;
}

// Computes the refresh rate for the specific mode. If we have enough
// information use the mode timings to compute a more exact value otherwise
// fallback to using the mode's vertical refresh rate (the kernel computes this
// the same way, however there is a loss in precision since |vrefresh| is sent
// as an integer).
float GetRefreshRate(const drmModeModeInfo& mode) {
  if (!mode.htotal || !mode.vtotal)
    return mode.vrefresh;

  float clock = mode.clock;
  float htotal = mode.htotal;
  float vtotal = mode.vtotal;

  return (clock * 1000.0f) / (htotal * vtotal);
}

display::DisplayConnectionType GetDisplayType(drmModeConnector* connector) {
  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_VGA:
      return display::DISPLAY_CONNECTION_TYPE_VGA;
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
      return display::DISPLAY_CONNECTION_TYPE_DVI;
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_eDP:
    case DRM_MODE_CONNECTOR_DSI:
      return display::DISPLAY_CONNECTION_TYPE_INTERNAL;
    case DRM_MODE_CONNECTOR_DisplayPort:
      return display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
      return display::DISPLAY_CONNECTION_TYPE_HDMI;
    default:
      return display::DISPLAY_CONNECTION_TYPE_UNKNOWN;
  }
}

int GetDrmProperty(int fd,
                   drmModeConnector* connector,
                   const std::string& name,
                   ScopedDrmPropertyPtr* property) {
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr tmp(drmModeGetProperty(fd, connector->props[i]));
    if (!tmp)
      continue;

    if (name == tmp->name) {
      *property = std::move(tmp);
      return i;
    }
  }

  return -1;
}

std::string GetNameForEnumValue(drmModePropertyRes* property, uint32_t value) {
  for (int i = 0; i < property->count_enums; ++i) {
    if (property->enums[i].value == value)
      return property->enums[i].name;
  }

  return std::string();
}

ScopedDrmPropertyBlobPtr GetDrmPropertyBlob(int fd,
                                            drmModeConnector* connector,
                                            const std::string& name) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(fd, connector, name, &property);
  if (index < 0)
    return nullptr;

  if (property->flags & DRM_MODE_PROP_BLOB) {
    return ScopedDrmPropertyBlobPtr(
        drmModeGetPropertyBlob(fd, connector->prop_values[index]));
  }

  return nullptr;
}

bool IsAspectPreserving(int fd, drmModeConnector* connector) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(fd, connector, "scaling mode", &property);
  if (index < 0)
    return false;

  return (GetNameForEnumValue(property.get(), connector->prop_values[index]) ==
          "Full aspect");
}

int ConnectorIndex(int device_index, int display_index) {
  DCHECK_LT(device_index, 16);
  DCHECK_LT(display_index, 16);
  return ((device_index << 4) + display_index) & 0xFF;
}

bool HasColorCorrectionMatrix(int fd, drmModeCrtc* crtc) {
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC));

  for (uint32_t i = 0; i < crtc_props->count_props; ++i) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(fd, crtc_props->props[i]));
    if (property && !strcmp(property->name, "CTM"))
      return true;
  }
  return false;
}

bool AreDisplayModesEqual(const DisplayMode_Params& lhs,
                          const DisplayMode_Params& rhs) {
  return lhs.size == rhs.size && lhs.is_interlaced == rhs.is_interlaced &&
         lhs.refresh_rate == rhs.refresh_rate;
}

}  // namespace

DisplayMode_Params GetDisplayModeParams(const display::DisplayMode& mode) {
  DisplayMode_Params params;
  params.size = mode.size();
  params.is_interlaced = mode.is_interlaced();
  params.refresh_rate = mode.refresh_rate();
  return params;
}

std::unique_ptr<display::DisplayMode> CreateDisplayModeFromParams(
    const DisplayMode_Params& pmode) {
  return std::make_unique<display::DisplayMode>(pmode.size, pmode.is_interlaced,
                                                pmode.refresh_rate);
}

const gfx::Size ModeSize(const drmModeModeInfo& mode) {
  return gfx::Size(mode.hdisplay, mode.vdisplay);
}

float ModeRefreshRate(const drmModeModeInfo& mode) {
  return GetRefreshRate(mode);
}

bool ModeIsInterlaced(const drmModeModeInfo& mode) {
  return mode.flags & DRM_MODE_FLAG_INTERLACE;
}

gfx::Size GetMaximumCursorSize(int fd) {
  uint64_t width = 0, height = 0;
  // Querying cursor dimensions is optional and is unsupported on older Chrome
  // OS kernels.
  if (drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &width) != 0 ||
      drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &height) != 0) {
    return gfx::Size(kDefaultCursorWidth, kDefaultCursorHeight);
  }
  return gfx::Size(width, height);
}

HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    ScopedDrmConnectorPtr connector,
    ScopedDrmCrtcPtr crtc,
    size_t index)
    : connector_(std::move(connector)), crtc_(std::move(crtc)), index_(index) {}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() {
}

std::vector<std::unique_ptr<HardwareDisplayControllerInfo>>
GetAvailableDisplayControllerInfos(int fd) {
  ScopedDrmResourcesPtr resources(drmModeGetResources(fd));
  DCHECK(resources) << "Failed to get DRM resources";
  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> displays;

  std::vector<ScopedDrmConnectorPtr> available_connectors;
  std::vector<ScopedDrmConnectorPtr::element_type*> connectors;
  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmConnectorPtr connector(
        drmModeGetConnector(fd, resources->connectors[i]));
    connectors.push_back(connector.get());

    if (connector && connector->connection == DRM_MODE_CONNECTED &&
        connector->count_modes != 0) {
      available_connectors.push_back(std::move(connector));
    }
  }

  base::flat_map<ScopedDrmConnectorPtr::element_type*, int> connector_crtcs;
  for (auto& c : available_connectors) {
    uint32_t possible_crtcs = 0;
    for (int i = 0; i < c->count_encoders; ++i) {
      ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, c->encoders[i]));
      if (!encoder)
        continue;
      possible_crtcs |= encoder->possible_crtcs;
    }
    connector_crtcs[c.get()] = possible_crtcs;
  }
  // Make sure to start assigning a crtc to the connector that supports the
  // fewest crtcs first.
  std::stable_sort(available_connectors.begin(), available_connectors.end(),
                   [&connector_crtcs](const ScopedDrmConnectorPtr& c1,
                                      const ScopedDrmConnectorPtr& c2) {
                     // When c1 supports a proper subset of the crtcs of c2, we
                     // should process c1 first (return true).
                     int c1_crtcs = connector_crtcs[c1.get()];
                     int c2_crtcs = connector_crtcs[c2.get()];
                     return (c1_crtcs & c2_crtcs) == c1_crtcs &&
                            c1_crtcs != c2_crtcs;
                   });

  for (auto& c : available_connectors) {
    uint32_t crtc_id = GetCrtc(fd, c.get(), resources.get(), displays);
    if (!crtc_id)
      continue;

    ScopedDrmCrtcPtr crtc(drmModeGetCrtc(fd, crtc_id));
    size_t index = std::find(connectors.begin(), connectors.end(), c.get()) -
                   connectors.begin();
    DCHECK_LT(index, connectors.size());
    displays.push_back(std::make_unique<HardwareDisplayControllerInfo>(
        std::move(c), std::move(crtc), index));
  }

  return displays;
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock && lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay && lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start && lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal && lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start && lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal && lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags && strcmp(lhs.name, rhs.name) == 0;
}

std::unique_ptr<display::DisplayMode> CreateDisplayMode(
    const drmModeModeInfo& mode) {
  return std::make_unique<display::DisplayMode>(
      gfx::Size(mode.hdisplay, mode.vdisplay),
      mode.flags & DRM_MODE_FLAG_INTERLACE, GetRefreshRate(mode));
}

std::unique_ptr<display::DisplaySnapshot> CreateDisplaySnapshot(
    HardwareDisplayControllerInfo* info,
    int fd,
    const base::FilePath& sys_path,
    size_t device_index,
    const gfx::Point& origin) {
  int64_t display_id = ConnectorIndex(device_index, info->index());
  const gfx::Size physical_size =
      gfx::Size(info->connector()->mmWidth, info->connector()->mmHeight);
  const display::DisplayConnectionType type = GetDisplayType(info->connector());
  const bool is_aspect_preserving_scaling =
      IsAspectPreserving(fd, info->connector());
  const bool has_color_correction_matrix =
      HasColorCorrectionMatrix(fd, info->crtc());
  const gfx::Size maximum_cursor_size = GetMaximumCursorSize(fd);

  std::vector<uint8_t> edid;
  std::string display_name;
  int64_t product_id = display::DisplaySnapshot::kInvalidProductID;
  bool has_overscan = false;
  gfx::ColorSpace display_color_space;

  ScopedDrmPropertyBlobPtr edid_blob(
      GetDrmPropertyBlob(fd, info->connector(), "EDID"));
  if (edid_blob) {
    edid.assign(static_cast<uint8_t*>(edid_blob->data),
                static_cast<uint8_t*>(edid_blob->data) + edid_blob->length);

    display::GetDisplayIdFromEDID(edid, display_id, &display_id, &product_id);

    display::ParseOutputDeviceData(edid, nullptr, nullptr, &display_name,
                                   nullptr, nullptr);
    display::ParseOutputOverscanFlag(edid, &has_overscan);

    display_color_space = GetColorSpaceFromEdid(edid);
  } else {
    VLOG(1) << "Failed to get EDID blob for connector "
            << info->connector()->connector_id;
  }

  display::DisplaySnapshot::DisplayModeList modes;
  const display::DisplayMode* current_mode = nullptr;
  const display::DisplayMode* native_mode = nullptr;
  for (int i = 0; i < info->connector()->count_modes; ++i) {
    const drmModeModeInfo& mode = info->connector()->modes[i];
    modes.push_back(CreateDisplayMode(mode));

    if (info->crtc()->mode_valid && SameMode(info->crtc()->mode, mode))
      current_mode = modes.back().get();

    if (mode.type & DRM_MODE_TYPE_PREFERRED)
      native_mode = modes.back().get();
  }

  // If no preferred mode is found then use the first one. Using the first one
  // since it should be the best mode.
  if (!native_mode && !modes.empty())
    native_mode = modes.front().get();

  return std::make_unique<display::DisplaySnapshot>(
      display_id, origin, physical_size, type, is_aspect_preserving_scaling,
      has_overscan, has_color_correction_matrix, display_color_space,
      display_name, sys_path, std::move(modes), edid, current_mode, native_mode,
      product_id, maximum_cursor_size);
}

// TODO(rjkroege): Remove in a subsequent CL once Mojo IPC is used everywhere.
std::vector<DisplaySnapshot_Params> CreateDisplaySnapshotParams(
    const MovableDisplaySnapshots& displays) {
  std::vector<DisplaySnapshot_Params> params;
  for (auto& d : displays) {
    DisplaySnapshot_Params p;

    p.display_id = d->display_id();
    p.origin = d->origin();
    p.physical_size = d->physical_size();
    p.type = d->type();
    p.is_aspect_preserving_scaling = d->is_aspect_preserving_scaling();
    p.has_overscan = d->has_overscan();
    p.has_color_correction_matrix = d->has_color_correction_matrix();
    p.color_space = d->color_space();
    p.display_name = d->display_name();
    p.sys_path = d->sys_path();

    std::vector<DisplayMode_Params> mode_params;
    for (const auto& m : d->modes()) {
      mode_params.push_back(GetDisplayModeParams(*m));
    }
    p.modes = mode_params;
    p.edid = d->edid();

    p.has_current_mode = d->current_mode();
    if (d->current_mode())
      p.current_mode = GetDisplayModeParams(*d->current_mode());

    p.has_native_mode = d->native_mode();
    if (d->native_mode())
      p.native_mode = GetDisplayModeParams(*d->native_mode());

    p.product_id = d->product_id();
    p.maximum_cursor_size = d->maximum_cursor_size();

    params.push_back(p);
  }
  return params;
}

std::unique_ptr<display::DisplaySnapshot> CreateDisplaySnapshot(
    const DisplaySnapshot_Params& params) {
  display::DisplaySnapshot::DisplayModeList modes;
  const display::DisplayMode* current_mode = nullptr;
  const display::DisplayMode* native_mode = nullptr;
  for (const auto& mode : params.modes) {
    modes.push_back(CreateDisplayModeFromParams(mode));
    if (AreDisplayModesEqual(params.current_mode, mode))
      current_mode = modes.back().get();
    if (AreDisplayModesEqual(params.native_mode, mode))
      native_mode = modes.back().get();
  }

  return std::make_unique<display::DisplaySnapshot>(
      params.display_id, params.origin, params.physical_size, params.type,
      params.is_aspect_preserving_scaling, params.has_overscan,
      params.has_color_correction_matrix, params.color_space,
      params.display_name, params.sys_path, std::move(modes), params.edid,
      current_mode, native_mode, params.product_id, params.maximum_cursor_size);
}

int GetFourCCFormatFromBufferFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return DRM_FORMAT_R8;
    case gfx::BufferFormat::R_16:
      return DRM_FORMAT_R16;
    case gfx::BufferFormat::RG_88:
      return DRM_FORMAT_GR88;
    case gfx::BufferFormat::RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::UYVY_422:
      return DRM_FORMAT_UYVY;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    default:
      NOTREACHED();
      return 0;
  }
}

gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format) {
  switch (format) {
    case DRM_FORMAT_R8:
      return gfx::BufferFormat::R_8;
    case DRM_FORMAT_GR88:
      return gfx::BufferFormat::RG_88;
    case DRM_FORMAT_ABGR8888:
      return gfx::BufferFormat::RGBA_8888;
    case DRM_FORMAT_XBGR8888:
      return gfx::BufferFormat::RGBX_8888;
    case DRM_FORMAT_ARGB8888:
      return gfx::BufferFormat::BGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return gfx::BufferFormat::BGRX_8888;
    case DRM_FORMAT_RGB565:
      return gfx::BufferFormat::BGR_565;
    case DRM_FORMAT_UYVY:
      return gfx::BufferFormat::UYVY_422;
    case DRM_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case DRM_FORMAT_YVU420:
      return gfx::BufferFormat::YVU_420;
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
  }
}

int GetFourCCFormatForOpaqueFramebuffer(gfx::BufferFormat format) {
  // DRM atomic interface doesn't currently support specifying an alpha
  // blending. We can simulate disabling alpha bleding creating an fb
  // with a format without the alpha channel.
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::UYVY_422:
      return DRM_FORMAT_UYVY;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    default:
      NOTREACHED();
      return 0;
  }
}

OverlaySurfaceCandidateList CreateOverlaySurfaceCandidateListFrom(
    const std::vector<OverlayCheck_Params>& params) {
  OverlaySurfaceCandidateList candidates;
  for (auto& p : params) {
    OverlaySurfaceCandidate osc;
    osc.transform = p.transform;
    osc.buffer_size = p.buffer_size;
    osc.format = p.format;
    osc.display_rect = gfx::RectF(p.display_rect);
    osc.crop_rect = p.crop_rect;
    osc.plane_z_order = p.plane_z_order;
    osc.overlay_handled = p.is_overlay_candidate;
    candidates.push_back(osc);
  }

  return candidates;
}

std::vector<OverlayCheck_Params> CreateParamsFromOverlaySurfaceCandidate(
    const OverlaySurfaceCandidateList& candidates) {
  std::vector<OverlayCheck_Params> overlay_params;
  for (auto& candidate : candidates) {
    overlay_params.push_back(OverlayCheck_Params(candidate));
  }

  return overlay_params;
}

OverlayStatusList CreateOverlayStatusListFrom(
    const std::vector<OverlayCheckReturn_Params>& params) {
  OverlayStatusList returns;
  for (auto& p : params) {
    returns.push_back(p.status);
  }

  return returns;
}

std::vector<OverlayCheckReturn_Params> CreateParamsFromOverlayStatusList(
    const OverlayStatusList& returns) {
  std::vector<OverlayCheckReturn_Params> params;
  for (auto& s : returns) {
    OverlayCheckReturn_Params p;
    p.status = s;
    params.push_back(p);
  }
  return params;
}

gfx::ColorSpace GetColorSpaceFromEdid(const std::vector<uint8_t>& edid) {
  SkColorSpacePrimaries primaries = {0};
  if (!display::ParseChromaticityCoordinates(edid, &primaries))
    return gfx::ColorSpace();

  SkMatrix44 color_space_as_matrix;
  if (!primaries.toXYZD50(&color_space_as_matrix))
    return gfx::ColorSpace();

  double gamma = 0.0;
  if (!display::ParseGammaValue(edid, &gamma))
    return gfx::ColorSpace();

  SkColorSpaceTransferFn transfer = {gamma, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  return gfx::ColorSpace::CreateCustom(color_space_as_matrix, transfer);
}

}  // namespace ui
