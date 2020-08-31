// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/image/image.h"

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

// This model provides data (icon ids and tooltip) for the content setting icons
// that are displayed in the location bar.
class ContentSettingImageModel {
 public:
  // The type of the content setting image model. This enum is used in
  // histograms and thus is append-only.
  enum class ImageType {
    COOKIES = 0,
    IMAGES = 1,
    JAVASCRIPT = 2,
    PPAPI_BROKER = 3,
    PLUGINS = 4,
    POPUPS = 5,
    GEOLOCATION = 6,
    MIXEDSCRIPT = 7,
    PROTOCOL_HANDLERS = 8,
    MEDIASTREAM = 9,
    ADS = 10,
    AUTOMATIC_DOWNLOADS = 11,
    MIDI_SYSEX = 12,
    SOUND = 13,
    FRAMEBUST = 14,
    // CLIPBOARD_READ = 15, // Replaced by CLIPBOARD_READ_WRITE in M81.
    SENSORS = 16,
    NOTIFICATIONS_QUIET_PROMPT = 17,
    CLIPBOARD_READ_WRITE = 18,

    NUM_IMAGE_TYPES
  };

  virtual ~ContentSettingImageModel() {}

  // Generates a vector of all image models to be used within one window.
  static std::vector<std::unique_ptr<ContentSettingImageModel>>
  GenerateContentSettingImageModels();

  // Returns the corresponding index into the above vector for the given
  // ContentSettingsType. For testing.
  static size_t GetContentSettingImageModelIndexForTesting(
      ImageType image_type);

  // Factory method.
  static std::unique_ptr<ContentSettingImageModel> CreateForContentType(
      ImageType image_type);

  void Update(content::WebContents* contents);

  // Creates the model for the bubble that will be attached to this image.
  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModel(
      ContentSettingBubbleModel::Delegate* delegate,
      content::WebContents* web_contents);

  // Whether the animation should be run for the given |web_contents|.
  bool ShouldRunAnimation(content::WebContents* web_contents);

  // Remembers that the animation has already run for the given |web_contents|,
  // so that we do not restart it when the parent view is updated.
  void SetAnimationHasRun(content::WebContents* web_contents);

  // Whether to automatically trigger the new bubble.
  bool ShouldAutoOpenBubble(content::WebContents* contents);

  // Remembers that the bubble was auto-opened for the given |web_contents|,
  // so that we do not auto-open it again when the parent view is updated.
  void SetBubbleWasAutoOpened(content::WebContents* contents);

  bool is_visible() const { return is_visible_; }

  // Retrieve the icon that represents this content setting. Blocked content
  // settings icons will have a blocked badge.
  gfx::Image GetIcon(SkColor icon_color) const;

  // Returns the resource ID of a string to show when the icon appears, or 0 if
  // we don't wish to show anything.
  int explanatory_string_id() const { return explanatory_string_id_; }
  const base::string16& get_tooltip() const { return tooltip_; }
  const gfx::VectorIcon* get_icon_badge() const { return icon_badge_; }

  ImageType image_type() const { return image_type_; }

  // Public for testing.
  void set_explanatory_string_id(int text_id) {
    explanatory_string_id_ = text_id;
  }

  bool ShouldNotifyAccessibility(content::WebContents* contents) const;
  void AccessibilityWasNotified(content::WebContents* contents);

  bool ShouldShowPromo(content::WebContents* contents);
  virtual void SetPromoWasShown(content::WebContents* contents);

 protected:
  explicit ContentSettingImageModel(
      ImageType type,
      bool image_type_should_notify_accessibility = false);

  // Notifies this model that its setting might have changed and it may need to
  // update its visibility, icon and tooltip. This method returns whether the
  // model should be visible.
  virtual bool UpdateAndGetVisibility(content::WebContents* web_contents) = 0;

  // Internal implementation by subclasses of bubble model creation.
  virtual std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      content::WebContents* web_contents) = 0;

  void set_icon(const gfx::VectorIcon& icon, const gfx::VectorIcon& badge) {
    icon_ = &icon;
    icon_badge_ = &badge;
  }

  void set_tooltip(const base::string16& tooltip) { tooltip_ = tooltip; }
  void set_should_auto_open_bubble(const bool should_auto_open_bubble) {
    should_auto_open_bubble_ = should_auto_open_bubble;
  }
  void set_should_show_promo(const bool should_show_promo) {
    should_show_promo_ = should_show_promo;
  }

 private:
  bool is_visible_ = false;

  const gfx::VectorIcon* icon_;
  const gfx::VectorIcon* icon_badge_;
  int explanatory_string_id_ = 0;
  base::string16 tooltip_;
  const ImageType image_type_;
  const bool image_type_should_notify_accessibility_;
  bool should_auto_open_bubble_ = false;
  bool should_show_promo_ = false;
  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModel);
};

// A subclass for an image model tied to a single content type.
class ContentSettingSimpleImageModel : public ContentSettingImageModel {
 public:
  ContentSettingSimpleImageModel(
      ImageType type,
      ContentSettingsType content_type,
      bool image_type_should_notify_accessibility = false);

  // ContentSettingImageModel implementation.
  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      content::WebContents* web_contents) override;

  ContentSettingsType content_type() { return content_type_; }

 private:
  ContentSettingsType content_type_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingSimpleImageModel);
};

class ContentSettingFramebustBlockImageModel : public ContentSettingImageModel {
 public:
  ContentSettingFramebustBlockImageModel();

  bool UpdateAndGetVisibility(content::WebContents* web_contents) override;

  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      content::WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingFramebustBlockImageModel);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_
