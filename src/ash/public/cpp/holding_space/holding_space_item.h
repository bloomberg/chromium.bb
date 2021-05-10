// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace ash {

class HoldingSpaceImage;

// Contains data needed to display a single item in the temporary holding space
// UI.
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
    kMaxValue = kScreenRecording,
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

  // Deserializes from `base::DictionaryValue` to `HoldingSpaceItem`.
  // This creates a partially initialized item with an empty file system URL.
  // The item should be finalized using `Finalize()`.
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

  // Indicates whether the item has been finalized. This will be false for items
  // created using `Deserialize()` for which `Finalize()` has not yet been
  // called.
  // Non-finalized items should not be shown in the holding space UI.
  bool IsFinalized() const;

  // Used to finalize partially initialized items created by `Deserialize()`.
  void Finalize(const GURL& file_system_url);

  // Updates the file backing the item to `file_path` and `file_system_url`.
  void UpdateBackingFile(const base::FilePath& file_path,
                         const GURL& file_system_url);

  // Invalidates the current holding space image, so fresh image representations
  // are loaded when the image is next needed.
  void InvalidateImage();

  // Returns true if this item is a screen capture.
  bool IsScreenCapture() const;

  const std::string& id() const { return id_; }

  Type type() const { return type_; }

  const base::string16& text() const { return text_; }

  const HoldingSpaceImage& image() const { return *image_; }

  const base::FilePath& file_path() const { return file_path_; }

  const GURL& file_system_url() const { return file_system_url_; }

  HoldingSpaceImage& image_for_testing() { return *image_; }

 private:
  // Constructor for file backed items.
  HoldingSpaceItem(Type type,
                   const std::string& id,
                   const base::FilePath& file_path,
                   const GURL& file_system_url,
                   const base::string16& text,
                   std::unique_ptr<HoldingSpaceImage> image);

  const Type type_;

  // The holding space item ID assigned to the item.
  std::string id_;

  // The file path by which the item is backed.
  base::FilePath file_path_;

  // The file system URL of the file that backs the item.
  GURL file_system_url_;

  // If set, the text that should be shown for the item.
  base::string16 text_;

  // The image representation of the item.
  std::unique_ptr<HoldingSpaceImage> image_;

  // Mutable to allow const access from `AddDeletionCallback()`.
  mutable base::RepeatingClosureList deletion_callback_list_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
