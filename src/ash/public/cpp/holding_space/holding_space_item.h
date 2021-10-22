// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace ash {

class HoldingSpaceImage;

// Contains data needed to display a single item in the holding space UI.
class ASH_PUBLIC_EXPORT HoldingSpaceItem {
 public:
  // Items types supported by the holding space.
  // NOTE: These values are recorded in histograms and persisted in preferences
  // so append new values to the end and do not change the meaning of existing
  // values.
  enum class Type {
    kPinnedFile = 0,
    kScreenshot = 1,
    kDownload = 2,
    kNearbyShare = 3,
    kScreenRecording = 4,
    kArcDownload = 5,
    kPrintedPdf = 6,
    kDiagnosticsLog = 7,
    kLacrosDownload = 8,
    kScan = 9,
    kPhoneHubCameraRoll = 10,
    kMaxValue = kPhoneHubCameraRoll,
  };

  HoldingSpaceItem(const HoldingSpaceItem&) = delete;
  HoldingSpaceItem operator=(const HoldingSpaceItem&) = delete;
  ~HoldingSpaceItem();

  bool operator==(const HoldingSpaceItem& rhs) const;

  // Returns an image for a given type and file path.
  using ImageResolver = base::OnceCallback<
      std::unique_ptr<HoldingSpaceImage>(Type, const base::FilePath&)>;

  // Creates a HoldingSpaceItem that's backed by a file system URL.
  // NOTE: `file_system_url` is expected to be non-empty.
  static std::unique_ptr<HoldingSpaceItem> CreateFileBackedItem(
      Type type,
      const base::FilePath& file_path,
      const GURL& file_system_url,
      ImageResolver image_resolver);

  // Creates a HoldingSpaceItem that's backed by a file system URL.
  // NOTE: `file_system_url` is expected to be non-empty.
  static std::unique_ptr<HoldingSpaceItem> CreateFileBackedItem(
      Type type,
      const base::FilePath& file_path,
      const GURL& file_system_url,
      const HoldingSpaceProgress& progress,
      ImageResolver image_resolver);

  // Returns `true` if `type` is a download type, `false` otherwise.
  static bool IsDownload(HoldingSpaceItem::Type type);

  // Deserializes from `base::DictionaryValue` to `HoldingSpaceItem`.
  // This creates a partially initialized item with an empty file system URL.
  // The item should be fully initialized using `Initialize()`.
  static std::unique_ptr<HoldingSpaceItem> Deserialize(
      const base::DictionaryValue& dict,
      ImageResolver image_resolver);

  // Deserializes `id_` from a serialized `HoldingSpaceItem`.
  static const std::string& DeserializeId(const base::DictionaryValue& dict);

  // Deserializes `file_path_` from a serialized `HoldingSpaceItem`.
  static base::FilePath DeserializeFilePath(const base::DictionaryValue& dict);

  // Serializes from `HoldingSpaceItem` to `base::DictionaryValue`.
  base::DictionaryValue Serialize() const;

  // Adds `callback` to be notified when `this` gets deleted.
  base::CallbackListSubscription AddDeletionCallback(
      base::RepeatingClosureList::CallbackType callback) const;

  // Indicates whether the item has been initialized. This will be false for
  // items created using `Deserialize()` for which `Initialize()` has not yet
  // been called. Non-initialized items should not be shown in holding space UI.
  bool IsInitialized() const;

  // Used to fully initialize partially initialized items created by
  // `Deserialize()`.
  void Initialize(const GURL& file_system_url);

  // Sets the file backing the item to `file_path` and `file_system_url`,
  // returning `true` if a change occurred or `false` to indicate no-op.
  bool SetBackingFile(const base::FilePath& file_path,
                      const GURL& file_system_url);

  // Returns `text_`, falling back to the lossy display name of the item's
  // backing file if absent.
  std::u16string GetText() const;

  // Sets the text that should be shown for the item, returning `true` if a
  // change occurred or `false` to indicate no-op. If absent, the lossy display
  // name of the item's backing file will be used.
  bool SetText(const absl::optional<std::u16string>& text);

  // Sets the secondary text that should be shown for the item, returning `true`
  // if a change occurred or `false` to indicate no-op.
  bool SetSecondaryText(const absl::optional<std::u16string>& secondary_text);

  // Returns `accessible_name_`, falling back to a concatenation of primary
  // and secondary text if absent.
  std::u16string GetAccessibleName() const;

  // Sets the accessible name that should be used for the item, returning `true`
  // if a change occurred or `false` to indicate no-op. Note that if the
  // accessible name is absent, `GetAccessibleName()` will fallback to a
  // concatenation of primary and secondary text.
  bool SetAccessibleName(const absl::optional<std::u16string>& accessible_name);

  // Sets the `progress_` of the item, returning `true` if a change occurred or
  // `false` to indicate no-op.
  // NOTE: Progress can only be updated for in progress items.
  bool SetProgress(const HoldingSpaceProgress& progress);

  // Invalidates the current holding space image, so fresh image representations
  // are loaded when the image is next needed.
  void InvalidateImage();

  // Returns true if this item is a screen capture.
  bool IsScreenCapture() const;

  // Returns true if progress of this item is paused.
  // NOTE: Only in-progress items can be paused.
  bool IsPaused() const;

  // Sets whether progress of this item is `paused_`, returning `true` if a
  // change occurred or `false` to indicate no-op.
  // NOTE: Only in-progress items can be paused.
  bool SetPaused(bool paused);

  const std::string& id() const { return id_; }

  Type type() const { return type_; }

  const absl::optional<std::u16string>& secondary_text() const {
    return secondary_text_;
  }

  const HoldingSpaceImage& image() const { return *image_; }

  const base::FilePath& file_path() const { return file_path_; }

  const GURL& file_system_url() const { return file_system_url_; }

  const HoldingSpaceProgress& progress() const { return progress_; }

  HoldingSpaceImage& image_for_testing() { return *image_; }

 private:
  // Constructor for file backed items.
  HoldingSpaceItem(Type type,
                   const std::string& id,
                   const base::FilePath& file_path,
                   const GURL& file_system_url,
                   std::unique_ptr<HoldingSpaceImage> image,
                   const HoldingSpaceProgress& progress);

  const Type type_;

  // The holding space item ID assigned to the item.
  std::string id_;

  // The file path by which the item is backed.
  base::FilePath file_path_;

  // The file system URL of the file that backs the item.
  GURL file_system_url_;

  // If set, the text that should be shown for the item.
  absl::optional<std::u16string> text_;

  // If set, the secondary text that should be shown for the item.
  absl::optional<std::u16string> secondary_text_;

  // If set, the accessible name that should be used for the item.
  absl::optional<std::u16string> accessible_name_;

  // The image representation of the item.
  std::unique_ptr<HoldingSpaceImage> image_;

  // The progress of the item.
  HoldingSpaceProgress progress_;

  // Whether or not progress of this item is paused.
  // NOTE: Only in-progress items can be paused.
  bool paused_ = false;

  // Mutable to allow const access from `AddDeletionCallback()`.
  mutable base::RepeatingClosureList deletion_callback_list_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
