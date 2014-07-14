// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/network/network_icon.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "grit/ui_chromeos_resources.h"
#include "grit/ui_chromeos_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/network/network_icon_animation.h"
#include "ui/chromeos/network/network_icon_animation_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size_conversions.h"

using chromeos::DeviceState;
using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ui {
namespace network_icon {

namespace {

//------------------------------------------------------------------------------
// Struct to pass icon badges to NetworkIconImageSource.
struct Badges {
  Badges()
      : top_left(NULL),
        top_right(NULL),
        bottom_left(NULL),
        bottom_right(NULL) {
  }
  const gfx::ImageSkia* top_left;
  const gfx::ImageSkia* top_right;
  const gfx::ImageSkia* bottom_left;
  const gfx::ImageSkia* bottom_right;
};

//------------------------------------------------------------------------------
// class used for maintaining a map of network state and images.
class NetworkIconImpl {
 public:
  NetworkIconImpl(const std::string& path, IconType icon_type);

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update(const chromeos::NetworkState* network);

  // Returns the cached image url for |image_| based on |scale_factor|.
  const std::string& GetImageUrl(float scale_factor);

  const gfx::ImageSkia& image() const { return image_; }

 private:
  typedef std::map<float, std::string> ImageUrlMap;

  // Updates |strength_index_| for wireless networks. Returns true if changed.
  bool UpdateWirelessStrengthIndex(const chromeos::NetworkState* network);

  // Updates the local state for cellular networks. Returns true if changed.
  bool UpdateCellularState(const chromeos::NetworkState* network);

  // Updates the portal state for wireless networks. Returns true if changed.
  bool UpdatePortalState(const chromeos::NetworkState* network);

  // Updates the VPN badge. Returns true if changed.
  bool UpdateVPNBadge();

  // Gets |badges| based on |network| and the current state.
  void GetBadges(const NetworkState* network, Badges* badges);

  // Gets the appropriate icon and badges and composites the image.
  void GenerateImage(const chromeos::NetworkState* network);

  // Network path, used for debugging.
  std::string network_path_;

  // Defines color theme and VPN badging
  const IconType icon_type_;

  // Cached state of the network when the icon was last generated.
  std::string state_;

  // Cached strength index of the network when the icon was last generated.
  int strength_index_;

  // Cached technology badge for the network when the icon was last generated.
  const gfx::ImageSkia* technology_badge_;

  // Cached vpn badge for the network when the icon was last generated.
  const gfx::ImageSkia* vpn_badge_;

  // Cached roaming state of the network when the icon was last generated.
  std::string roaming_state_;

  // Cached portal state of the network when the icon was last generated.
  bool behind_captive_portal_;

  // Generated icon image.
  gfx::ImageSkia image_;

  // Map of cached image urls by scale factor. Cleared whenever image_ is
  // generated.
  ImageUrlMap image_urls_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImpl);
};

//------------------------------------------------------------------------------
// Maintain a static (global) icon map. Note: Icons are never destroyed;
// it is assumed that a finite and reasonable number of network icons will be
// created during a session.

typedef std::map<std::string, NetworkIconImpl*> NetworkIconMap;

NetworkIconMap* GetIconMapInstance(IconType icon_type, bool create) {
  typedef std::map<IconType, NetworkIconMap*> IconTypeMap;
  static IconTypeMap* s_icon_map = NULL;
  if (s_icon_map == NULL) {
    if (!create)
      return NULL;
    s_icon_map = new IconTypeMap;
  }
  if (s_icon_map->count(icon_type) == 0) {
    if (!create)
      return NULL;
    (*s_icon_map)[icon_type] = new NetworkIconMap;
  }
  return (*s_icon_map)[icon_type];
}

NetworkIconMap* GetIconMap(IconType icon_type) {
  return GetIconMapInstance(icon_type, true);
}

void PurgeIconMap(IconType icon_type,
                  const std::set<std::string>& network_paths) {
  NetworkIconMap* icon_map = GetIconMapInstance(icon_type, false);
  if (!icon_map)
    return;
  for (NetworkIconMap::iterator loop_iter = icon_map->begin();
       loop_iter != icon_map->end(); ) {
    NetworkIconMap::iterator cur_iter = loop_iter++;
    if (network_paths.count(cur_iter->first) == 0) {
      delete cur_iter->second;
      icon_map->erase(cur_iter);
    }
  }
}

//------------------------------------------------------------------------------
// Utilities for generating icon images.

// 'NONE' will default to ARCS behavior where appropriate (e.g. no network or
// if a new type gets added).
enum ImageType {
  ARCS,
  BARS,
  NONE
};

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Images for strength bars for wired networks.
const int kNumBarsImages = 5;

// Imagaes for strength arcs for wireless networks.
const int kNumArcsImages = 5;

// Number of discrete images to use for alpha fade animation
const int kNumFadeImages = 10;

//------------------------------------------------------------------------------
// Classes for generating scaled images.

const SkBitmap GetEmptyBitmap(const gfx::Size pixel_size) {
  typedef std::pair<int, int> SizeKey;
  typedef std::map<SizeKey, SkBitmap> SizeBitmapMap;
  static SizeBitmapMap* s_empty_bitmaps = new SizeBitmapMap;

  SizeKey key(pixel_size.width(), pixel_size.height());

  SizeBitmapMap::iterator iter = s_empty_bitmaps->find(key);
  if (iter != s_empty_bitmaps->end())
    return iter->second;

  SkBitmap empty;
  empty.allocN32Pixels(key.first, key.second);
  empty.eraseARGB(0, 0, 0, 0);
  (*s_empty_bitmaps)[key] = empty;
  return empty;
}

class EmptyImageSource: public gfx::ImageSkiaSource {
 public:
  explicit EmptyImageSource(const gfx::Size& size)
      : size_(size) {
  }

  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    gfx::Size pixel_size = gfx::ToFlooredSize(gfx::ScaleSize(size_, scale));
    SkBitmap empty_bitmap = GetEmptyBitmap(pixel_size);
    return gfx::ImageSkiaRep(empty_bitmap, scale);
  }

 private:
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(EmptyImageSource);
};

// This defines how we assemble a network icon.
class NetworkIconImageSource : public gfx::ImageSkiaSource {
 public:
  NetworkIconImageSource(const gfx::ImageSkia& icon, const Badges& badges)
      : icon_(icon),
        badges_(badges) {
  }
  virtual ~NetworkIconImageSource() {}

  // TODO(pkotwicz): Figure out what to do when a new image resolution becomes
  // available.
  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale);
    if (icon_rep.is_null())
      return gfx::ImageSkiaRep();
    gfx::Canvas canvas(icon_rep, false);
    if (badges_.top_left)
      canvas.DrawImageInt(*badges_.top_left, 0, 0);
    if (badges_.top_right)
      canvas.DrawImageInt(*badges_.top_right,
                          icon_.width() - badges_.top_right->width(), 0);
    if (badges_.bottom_left) {
      canvas.DrawImageInt(*badges_.bottom_left,
                          0, icon_.height() - badges_.bottom_left->height());
    }
    if (badges_.bottom_right) {
      canvas.DrawImageInt(*badges_.bottom_right,
                          icon_.width() - badges_.bottom_right->width(),
                          icon_.height() - badges_.bottom_right->height());
    }
    return canvas.ExtractImageRep();
  }

 private:
  const gfx::ImageSkia icon_;
  const Badges badges_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconImageSource);
};

//------------------------------------------------------------------------------
// Utilities for extracting icon images.

// A struct used for caching image urls.
struct ImageIdForNetworkType {
  ImageIdForNetworkType(IconType icon_type,
                        const std::string& network_type,
                        float scale_factor) :
      icon_type(icon_type),
      network_type(network_type),
      scale_factor(scale_factor) {}
  bool operator<(const ImageIdForNetworkType& other) const {
    if (icon_type != other.icon_type)
      return icon_type < other.icon_type;
    if (network_type != other.network_type)
      return network_type < other.network_type;
    return scale_factor < other.scale_factor;
  }
  IconType icon_type;
  std::string network_type;
  float scale_factor;
};

typedef std::map<ImageIdForNetworkType, std::string> ImageIdUrlMap;

bool IconTypeIsDark(IconType icon_type) {
  return (icon_type != ICON_TYPE_TRAY);
}

bool IconTypeHasVPNBadge(IconType icon_type) {
  return (icon_type != ICON_TYPE_LIST);
}

int NumImagesForType(ImageType type) {
  return (type == BARS) ? kNumBarsImages : kNumArcsImages;
}

gfx::ImageSkia* BaseImageForType(ImageType image_type, IconType icon_type) {
  gfx::ImageSkia* image;
  if (image_type == BARS) {
    image =  ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_BARS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_BARS_LIGHT);
  } else {
    image =  ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_ARCS_LIGHT);
  }
  return image;
}

ImageType ImageTypeForNetworkType(const std::string& type) {
  if (type == shill::kTypeWifi)
    return ARCS;
  else if (type == shill::kTypeCellular || type == shill::kTypeWimax)
    return BARS;
  return NONE;
}

gfx::ImageSkia GetImageForIndex(ImageType image_type,
                                IconType icon_type,
                                int index) {
  int num_images = NumImagesForType(image_type);
  if (index < 0 || index >= num_images)
    return gfx::ImageSkia();
  gfx::ImageSkia* images = BaseImageForType(image_type, icon_type);
  int width = images->width();
  int height = images->height() / num_images;
  return gfx::ImageSkiaOperations::ExtractSubset(*images,
      gfx::Rect(0, index * height, width, height));
}

const gfx::ImageSkia GetConnectedImage(IconType icon_type,
                                       const std::string& network_type) {
  if (network_type == shill::kTypeVPN) {
    return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NETWORK_VPN);
  }
  ImageType image_type = ImageTypeForNetworkType(network_type);
  const int connected_index = NumImagesForType(image_type) - 1;
  return GetImageForIndex(image_type, icon_type, connected_index);
}

const gfx::ImageSkia GetDisconnectedImage(IconType icon_type,
                                          const std::string& network_type) {
  if (network_type == shill::kTypeVPN) {
    // Note: same as connected image, shouldn't normally be seen.
    return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_NETWORK_VPN);
  }
  ImageType image_type = ImageTypeForNetworkType(network_type);
  const int disconnected_index = 0;
  return GetImageForIndex(image_type, icon_type, disconnected_index);
}

gfx::ImageSkia* ConnectingWirelessImage(ImageType image_type,
                                        IconType icon_type,
                                        double animation) {
  static gfx::ImageSkia* s_bars_images_dark[kNumBarsImages - 1];
  static gfx::ImageSkia* s_bars_images_light[kNumBarsImages - 1];
  static gfx::ImageSkia* s_arcs_images_dark[kNumArcsImages - 1];
  static gfx::ImageSkia* s_arcs_images_light[kNumArcsImages - 1];
  int image_count = NumImagesForType(image_type) - 1;
  int index = animation * nextafter(static_cast<float>(image_count), 0);
  index = std::max(std::min(index, image_count - 1), 0);
  gfx::ImageSkia** images;
  bool dark = IconTypeIsDark(icon_type);
  if (image_type == BARS)
    images = dark ? s_bars_images_dark : s_bars_images_light;
  else
    images = dark ? s_arcs_images_dark : s_arcs_images_light;
  if (!images[index]) {
    // Lazily cache images.
    gfx::ImageSkia source = GetImageForIndex(image_type, icon_type, index + 1);
    images[index] = new gfx::ImageSkia(
        gfx::ImageSkiaOperations::CreateBlendedImage(
            gfx::ImageSkia(new EmptyImageSource(source.size()), source.size()),
            source,
            kConnectingImageAlpha));
  }
  return images[index];
}

gfx::ImageSkia* ConnectingVpnImage(double animation) {
  int index = animation * nextafter(static_cast<float>(kNumFadeImages), 0);
  static gfx::ImageSkia* s_vpn_images[kNumFadeImages];
  if (!s_vpn_images[index]) {
    // Lazily cache images.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia* icon = rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN);
    s_vpn_images[index] = new gfx::ImageSkia(
        gfx::ImageSkiaOperations::CreateBlendedImage(
            gfx::ImageSkia(new EmptyImageSource(icon->size()), icon->size()),
            *icon,
            animation));
  }
  return s_vpn_images[index];
}

gfx::ImageSkia* ConnectingVpnBadge(double animation) {
  int index = animation * nextafter(static_cast<float>(kNumFadeImages), 0);
  static gfx::ImageSkia* s_vpn_badges[kNumFadeImages];
  if (!s_vpn_badges[index]) {
    // Lazily cache images.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia* icon =
        rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);  // For size
    gfx::ImageSkia* badge =
        rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN_BADGE);
    s_vpn_badges[index] = new gfx::ImageSkia(
        gfx::ImageSkiaOperations::CreateBlendedImage(
            gfx::ImageSkia(new EmptyImageSource(icon->size()), icon->size()),
            *badge,
            animation));
  }
  return s_vpn_badges[index];
}

int StrengthIndex(int strength, int count) {
  // Return an index in the range [1, count-1].
  const float findex = (static_cast<float>(strength) / 100.0f) *
      nextafter(static_cast<float>(count - 1), 0);
  int index = 1 + static_cast<int>(findex);
  index = std::max(std::min(index, count - 1), 1);
  return index;
}

int GetStrengthIndex(const NetworkState* network) {
  ImageType image_type = ImageTypeForNetworkType(network->type());
  if (image_type == ARCS)
    return StrengthIndex(network->signal_strength(), kNumArcsImages);
  else if (image_type == BARS)
    return StrengthIndex(network->signal_strength(), kNumBarsImages);
  return 0;
}

const gfx::ImageSkia* BadgeForNetworkTechnology(const NetworkState* network,
                                                IconType icon_type) {
  const int kUnknownBadgeType = -1;
  int id = kUnknownBadgeType;
  const std::string& technology = network->network_technology();
  if (technology == shill::kNetworkTechnologyEvdo) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_EVDO_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_EVDO_LIGHT;
  } else if (technology == shill::kNetworkTechnology1Xrtt) {
    id = IDR_AURA_UBER_TRAY_NETWORK_1X;
  } else if (technology == shill::kNetworkTechnologyGprs) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_LIGHT;
  } else if (technology == shill::kNetworkTechnologyEdge) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_EDGE_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_EDGE_LIGHT;
  } else if (technology == shill::kNetworkTechnologyUmts) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_3G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_3G_LIGHT;
  } else if (technology == shill::kNetworkTechnologyHspa) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_LIGHT;
  } else if (technology == shill::kNetworkTechnologyHspaPlus) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_HSPA_PLUS_LIGHT;
  } else if (technology == shill::kNetworkTechnologyLte) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_LTE_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_LTE_LIGHT;
  } else if (technology == shill::kNetworkTechnologyLteAdvanced) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_LTE_ADVANCED_LIGHT;
  } else if (technology == shill::kNetworkTechnologyGsm) {
    id = IconTypeIsDark(icon_type) ?
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_GPRS_LIGHT;
  }
  if (id == kUnknownBadgeType)
    return NULL;
  else
    return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

const gfx::ImageSkia* BadgeForVPN(IconType icon_type) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_AURA_UBER_TRAY_NETWORK_VPN_BADGE);
}

gfx::ImageSkia GetIcon(const NetworkState* network,
                       IconType icon_type,
                       int strength_index) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (network->Matches(NetworkTypePattern::Ethernet())) {
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
  } else if (network->Matches(NetworkTypePattern::Wireless())) {
    DCHECK(strength_index > 0);
    return GetImageForIndex(
        ImageTypeForNetworkType(network->type()), icon_type, strength_index);
  } else if (network->Matches(NetworkTypePattern::VPN())) {
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_VPN);
  } else {
    LOG(WARNING) << "Request for icon for unsupported type: "
                 << network->type();
    return *rb.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED);
  }
}

//------------------------------------------------------------------------------
// Get connecting images

gfx::ImageSkia GetConnectingVpnImage(IconType icon_type) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* connected_network = NULL;
  if (icon_type == ICON_TYPE_TRAY) {
    connected_network =
        handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  }
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();

  if (connected_network) {
    gfx::ImageSkia icon = GetImageForNetwork(connected_network, icon_type);
    Badges badges;
    badges.bottom_left = ConnectingVpnBadge(animation);
    return gfx::ImageSkia(
        new NetworkIconImageSource(icon, badges), icon.size());
  } else {
    gfx::ImageSkia* icon = ConnectingVpnImage(animation);
    return gfx::ImageSkia(
        new NetworkIconImageSource(*icon, Badges()), icon->size());
  }
}

gfx::ImageSkia GetConnectingImage(IconType icon_type,
                                  const std::string& network_type) {
  if (network_type == shill::kTypeVPN)
    return GetConnectingVpnImage(icon_type);

  ImageType image_type = ImageTypeForNetworkType(network_type);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();

  gfx::ImageSkia* icon = ConnectingWirelessImage(
      image_type, icon_type, animation);
  return gfx::ImageSkia(
      new NetworkIconImageSource(*icon, Badges()), icon->size());
}

std::string GetConnectingImageUrl(IconType icon_type,
                                  const std::string& network_type,
                                  float scale_factor) {
  // Caching the connecting image is complicated and we will never draw more
  // than a few per frame, so just generate the image url each time.
  gfx::ImageSkia image = GetConnectingImage(icon_type, network_type);
  gfx::ImageSkiaRep image_rep = image.GetRepresentation(scale_factor);
  return webui::GetBitmapDataUrl(image_rep.sk_bitmap());
}

}  // namespace

//------------------------------------------------------------------------------
// NetworkIconImpl

NetworkIconImpl::NetworkIconImpl(const std::string& path, IconType icon_type)
    : network_path_(path),
      icon_type_(icon_type),
      strength_index_(-1),
      technology_badge_(NULL),
      vpn_badge_(NULL),
      behind_captive_portal_(false) {
  // Default image
  image_ = GetDisconnectedImage(icon_type, shill::kTypeWifi);
}

void NetworkIconImpl::Update(const NetworkState* network) {
  DCHECK(network);
  // Determine whether or not we need to update the icon.
  bool dirty = image_.isNull();

  // If the network state has changed, the icon needs updating.
  if (state_ != network->connection_state()) {
    state_ = network->connection_state();
    dirty = true;
  }

  dirty |= UpdatePortalState(network);

  if (network->Matches(NetworkTypePattern::Wireless())) {
    dirty |= UpdateWirelessStrengthIndex(network);
  }

  if (network->Matches(NetworkTypePattern::Cellular()))
    dirty |= UpdateCellularState(network);

  if (IconTypeHasVPNBadge(icon_type_) &&
      network->Matches(NetworkTypePattern::NonVirtual())) {
    dirty |= UpdateVPNBadge();
  }

  if (dirty) {
    // Set the icon and badges based on the network and generate the image.
    GenerateImage(network);
  }
}

bool NetworkIconImpl::UpdateWirelessStrengthIndex(const NetworkState* network) {
  int index = GetStrengthIndex(network);
  if (index != strength_index_) {
    strength_index_ = index;
    return true;
  }
  return false;
}

bool NetworkIconImpl::UpdateCellularState(const NetworkState* network) {
  bool dirty = false;
  const gfx::ImageSkia* technology_badge =
      BadgeForNetworkTechnology(network, icon_type_);
  if (technology_badge != technology_badge_) {
    technology_badge_ = technology_badge;
    dirty = true;
  }
  std::string roaming_state = network->roaming();
  if (roaming_state != roaming_state_) {
    roaming_state_ = roaming_state;
    dirty = true;
  }
  return dirty;
}

bool NetworkIconImpl::UpdatePortalState(const NetworkState* network) {
  bool behind_captive_portal = false;
  if (network) {
    NetworkPortalDetector::CaptivePortalState state =
        NetworkPortalDetector::Get()->GetCaptivePortalState(network->guid());
    behind_captive_portal =
        state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  }

  if (behind_captive_portal == behind_captive_portal_)
    return false;
  behind_captive_portal_ = behind_captive_portal;
  return true;
}

bool NetworkIconImpl::UpdateVPNBadge() {
  const NetworkState* vpn = NetworkHandler::Get()->network_state_handler()->
      ConnectedNetworkByType(NetworkTypePattern::VPN());
  if (vpn && vpn_badge_ == NULL) {
    vpn_badge_ = BadgeForVPN(icon_type_);
    return true;
  } else if (!vpn && vpn_badge_ != NULL) {
    vpn_badge_ = NULL;
    return true;
  }
  return false;
}

void NetworkIconImpl::GetBadges(const NetworkState* network, Badges* badges) {
  DCHECK(network);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  const std::string& type = network->type();
  if (type == shill::kTypeWifi) {
    if (network->security() != shill::kSecurityNone &&
        IconTypeIsDark(icon_type_)) {
      badges->bottom_right = rb.GetImageSkiaNamed(
          IDR_AURA_UBER_TRAY_NETWORK_SECURE_DARK);
    }
  } else if (type == shill::kTypeWimax) {
    technology_badge_ = rb.GetImageSkiaNamed(
        IconTypeIsDark(icon_type_) ?
        IDR_AURA_UBER_TRAY_NETWORK_4G_DARK :
        IDR_AURA_UBER_TRAY_NETWORK_4G_LIGHT);
  } else if (type == shill::kTypeCellular) {
    if (network->roaming() == shill::kRoamingStateRoaming) {
      // For networks that are always in roaming don't show roaming badge.
      const DeviceState* device =
          handler->GetDeviceState(network->device_path());
      LOG_IF(WARNING, !device) << "Could not find device state for "
                               << network->device_path();
      if (!device || !device->provider_requires_roaming()) {
        badges->bottom_right = rb.GetImageSkiaNamed(
            IconTypeIsDark(icon_type_) ?
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_DARK :
            IDR_AURA_UBER_TRAY_NETWORK_ROAMING_LIGHT);
      }
    }
  }
  if (!network->IsConnectingState()) {
    badges->top_left = technology_badge_;
    badges->bottom_left = vpn_badge_;
  }

  if (behind_captive_portal_) {
    gfx::ImageSkia* badge = rb.GetImageSkiaNamed(
       IconTypeIsDark(icon_type_) ?
       IDR_AURA_UBER_TRAY_NETWORK_PORTAL_DARK :
       IDR_AURA_UBER_TRAY_NETWORK_PORTAL_LIGHT);
    badges->bottom_right = badge;
  }
}

void NetworkIconImpl::GenerateImage(const NetworkState* network) {
  DCHECK(network);
  gfx::ImageSkia icon = GetIcon(network, icon_type_, strength_index_);
  Badges badges;
  GetBadges(network, &badges);
  image_ = gfx::ImageSkia(
      new NetworkIconImageSource(icon, badges), icon.size());
  image_urls_.clear();
}

const std::string& NetworkIconImpl::GetImageUrl(float scale_factor) {
  ImageUrlMap::iterator iter = image_urls_.find(scale_factor);
  if (iter != image_urls_.end())
    return iter->second;

  VLOG(2) << "Generating bitmap URL for: " << network_path_;
  gfx::ImageSkiaRep image_rep = image_.GetRepresentation(scale_factor);
  iter = image_urls_.insert(std::make_pair(
      scale_factor, webui::GetBitmapDataUrl(image_rep.sk_bitmap()))).first;
  return iter->second;
}

namespace {

NetworkIconImpl* FindAndUpdateImageImpl(const NetworkState* network,
                                        IconType icon_type) {
  // Find or add the icon.
  NetworkIconMap* icon_map = GetIconMap(icon_type);
  NetworkIconImpl* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->path());
  if (iter == icon_map->end()) {
    icon = new NetworkIconImpl(network->path(), icon_type);
    icon_map->insert(std::make_pair(network->path(), icon));
  } else {
    icon = iter->second;
  }

  // Update and return the icon's image.
  icon->Update(network);
  return icon;
}

}  // namespace

//------------------------------------------------------------------------------
// Public interface

gfx::ImageSkia GetImageForNetwork(const NetworkState* network,
                                  IconType icon_type) {
  DCHECK(network);
  if (!network->visible())
    return GetDisconnectedImage(icon_type, network->type());

  if (network->IsConnectingState())
    return GetConnectingImage(icon_type, network->type());

  NetworkIconImpl* icon = FindAndUpdateImageImpl(network, icon_type);
  return icon->image();
}

std::string GetImageUrlForNetwork(const NetworkState* network,
                                  IconType icon_type,
                                  float scale_factor) {
  DCHECK(network);
  // Handle connecting icons.
  if (network->IsConnectingState())
    return GetConnectingImageUrl(icon_type, network->type(), scale_factor);

  NetworkIconImpl* icon = FindAndUpdateImageImpl(network, icon_type);
  return icon->GetImageUrl(scale_factor);
}

gfx::ImageSkia GetImageForConnectedNetwork(IconType icon_type,
                                           const std::string& network_type) {
  return GetConnectedImage(icon_type, network_type);
}

gfx::ImageSkia GetImageForConnectingNetwork(IconType icon_type,
                                            const std::string& network_type) {
  return GetConnectingImage(icon_type, network_type);
}

gfx::ImageSkia GetImageForDisconnectedNetwork(IconType icon_type,
                                              const std::string& network_type) {
  return GetDisconnectedImage(icon_type, network_type);
}

base::string16 GetLabelForNetwork(const chromeos::NetworkState* network,
                                  IconType icon_type) {
  DCHECK(network);
  std::string activation_state = network->activation_state();
  if (icon_type == ICON_TYPE_LIST) {
    // Show "<network>: [Connecting|Activating]..."
    if (network->IsConnectingState()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_CONNECTING,
          base::UTF8ToUTF16(network->name()));
    }
    if (activation_state == shill::kActivationStateActivating) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING,
          base::UTF8ToUTF16(network->name()));
    }
    // Show "Activate <network>" in list view only.
    if (activation_state == shill::kActivationStateNotActivated ||
        activation_state == shill::kActivationStatePartiallyActivated) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATE,
          base::UTF8ToUTF16(network->name()));
    }
  } else {
    // Show "[Connected to|Connecting to|Activating] <network>" (non-list view).
    if (network->IsConnectedState()) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                        base::UTF8ToUTF16(network->name()));
    }
    if (network->IsConnectingState()) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING,
                                        base::UTF8ToUTF16(network->name()));
    }
    if (activation_state == shill::kActivationStateActivating) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING,
                                        base::UTF8ToUTF16(network->name()));
    }
  }

  // Otherwise just show the network name or 'Ethernet'.
  if (network->Matches(NetworkTypePattern::Ethernet())) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET);
  } else {
    return base::UTF8ToUTF16(network->name());
  }
}

int GetCellularUninitializedMsg() {
  static base::Time s_uninitialized_state_time;
  static int s_uninitialized_msg(0);

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (handler->GetTechnologyState(NetworkTypePattern::Mobile())
      == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
    s_uninitialized_msg = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    s_uninitialized_state_time = base::Time::Now();
    return s_uninitialized_msg;
  } else if (handler->GetScanningByType(NetworkTypePattern::Mobile())) {
    s_uninitialized_msg = IDS_ASH_STATUS_TRAY_CELLULAR_SCANNING;
    s_uninitialized_state_time = base::Time::Now();
    return s_uninitialized_msg;
  }
  // There can be a delay between leaving the Initializing state and when
  // a Cellular device shows up, so keep showing the initializing
  // animation for a bit to avoid flashing the disconnect icon.
  const int kInitializingDelaySeconds = 1;
  base::TimeDelta dtime = base::Time::Now() - s_uninitialized_state_time;
  if (dtime.InSeconds() < kInitializingDelaySeconds)
    return s_uninitialized_msg;
  return 0;
}

void GetDefaultNetworkImageAndLabel(IconType icon_type,
                                    gfx::ImageSkia* image,
                                    base::string16* label,
                                    bool* animating) {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();
  NetworkConnectionHandler* connect_handler =
      NetworkHandler::Get()->network_connection_handler();
  const NetworkState* connected_network =
      state_handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  const NetworkState* connecting_network =
      state_handler->ConnectingNetworkByType(NetworkTypePattern::Wireless());
  if (!connecting_network && icon_type == ICON_TYPE_TRAY) {
    connecting_network =
        state_handler->ConnectingNetworkByType(NetworkTypePattern::VPN());
  }

  const NetworkState* network;
  // If we are connecting to a network, and there is either no connected
  // network, or the connection was user requested, use the connecting
  // network.
  if (connecting_network &&
      (!connected_network ||
       connect_handler->HasConnectingNetwork(connecting_network->path()))) {
    network = connecting_network;
  } else {
    network = connected_network;
  }

  // Don't show ethernet in the tray
  if (icon_type == ICON_TYPE_TRAY && network &&
      network->Matches(NetworkTypePattern::Ethernet())) {
    *image = gfx::ImageSkia();
    *animating = false;
    return;
  }

  if (!network) {
    // If no connecting network, check if we are activating a network.
    const NetworkState* mobile_network =
        state_handler->FirstNetworkByType(NetworkTypePattern::Mobile());
    if (mobile_network && (mobile_network->activation_state() ==
                           shill::kActivationStateActivating)) {
      network = mobile_network;
    }
  }
  if (!network) {
    // If no connecting network, check for cellular initializing.
    int uninitialized_msg = GetCellularUninitializedMsg();
    if (uninitialized_msg != 0) {
      *image = GetImageForConnectingNetwork(icon_type, shill::kTypeCellular);
      if (label)
        *label = l10n_util::GetStringUTF16(uninitialized_msg);
      *animating = true;
    } else {
      // Otherwise show the disconnected wifi icon.
      *image = GetImageForDisconnectedNetwork(icon_type, shill::kTypeWifi);
      if (label) {
        *label = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
      }
      *animating = false;
    }
    return;
  }
  *animating = network->IsConnectingState();
  // Get icon and label for connected or connecting network.
  *image = GetImageForNetwork(network, icon_type);
  if (label)
    *label = GetLabelForNetwork(network, icon_type);
}

void PurgeNetworkIconCache() {
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkList(
      &networks);
  std::set<std::string> network_paths;
  for (NetworkStateHandler::NetworkStateList::iterator iter = networks.begin();
       iter != networks.end(); ++iter) {
    network_paths.insert((*iter)->path());
  }
  PurgeIconMap(ICON_TYPE_TRAY, network_paths);
  PurgeIconMap(ICON_TYPE_DEFAULT_VIEW, network_paths);
  PurgeIconMap(ICON_TYPE_LIST, network_paths);
}

}  // namespace network_icon
}  // namespace ui
