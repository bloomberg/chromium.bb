// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_H_

#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/common/content_export.h"

#include <ft2build.h>
#include FT_SYSTEM_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H

#include <string>

namespace blink {
class FontUniqueNameTable;
}

namespace content {

// Scans a set of font files for the full font name and postscript name
// information in the name table and builds a Protobuf lookup structure from
// it. The protobuf can be persisted to disk to the Android cache directory, and
// it can be read from disk as well. Provides the lookup structure as a
// ReadOnlySharedMemoryRegion. Performing lookup on it is done through
// FontTableMatcher.
class CONTENT_EXPORT FontUniqueNameLookup {
 public:
  FontUniqueNameLookup() = delete;

  // Retrieve an initialized instance of FontUniqueNameLookup that has read the
  // table from cache if there was one, updated the lookup table if needed
  // (i.e. if there was an Android firmware update) from the standard Android
  // font directories, and written the updated lookup table back to file. It is
  // ready to use with FontTableMatcher.
  static FontUniqueNameLookup& GetInstance();

  // Construct a FontUniqueNameLookup given a cache directory path
  // |cache_directory| to persist the internal lookup table, a
  // FontFilesCollector to enumerate font files and a BuildFingerprintProvider
  // to access the Android build fingerprint.
  FontUniqueNameLookup(const base::FilePath& cache_directory);
  ~FontUniqueNameLookup();

  // Default move contructor.
  FontUniqueNameLookup(FontUniqueNameLookup&&);
  // Default move assigment operator.
  FontUniqueNameLookup& operator=(FontUniqueNameLookup&&) = default;

  // Return a ReadOnlySharedMemoryRegion to access the serialized form of the
  // current lookup table. To be used with FontTableMatcher.
  base::ReadOnlySharedMemoryRegion GetUniqueNameTableAsSharedMemoryRegion()
      const;

  // Returns true if an up-to-date, consistent font table is present.
  bool IsValid();

  // If an Android firmware update was detected by checking
  // BuildFingerprintProvider, call UpdateTable(). Do not use this method.
  // Instead, call GetInstance() to get an initialized instance. Publicly
  // exposed for testing.
  bool UpdateTableIfNeeded();
  // Rescan the files returned by the FontFilesCollector and rebuild the lookup
  // table by indexing them. Do not use this method. Instead, call GetInstance()
  // to get an initialized instance. Returns true if instance is valid after
  // updating, returns false if an error occured in acquiring memory or
  // serializing the scanned files to the shared memory region. Publicly exposed
  // for testing.
  bool UpdateTable();
  // Try to find a serialized lookup table in the directory specified at
  // construction and load it into memory. Do not use this method. Instead, call
  // GetInstance() to get an initialized instance. Publicly exposed for testing.
  bool LoadFromFile();
  // Serialize the current lookup table into a file in the the cache directory
  // specified at construction time. If an up to date table is present and
  // persisting fails, discard the internal table, as it might be that we were
  // not able to update the file the previous time. Do not use this
  // method. Instead, call GetInstance() to get an initialized
  // instance. Publicly exposed for testing.
  bool PersistToFile();

  // Override the internal font files enumeration with an explicit set of fonts
  // to be scanned in |font_file_paths|. Only used for testing.
  void SetFontFilePathsForTesting(
      const std::vector<std::string> font_file_paths) {
    font_file_paths_for_testing_ = font_file_paths;
  }

  // Override the Android build fingerprint for testing.
  void SetAndroidBuildFingerprintForTesting(
      const std::string& build_fingerprint_override) {
    android_build_fingerprint_for_testing_ = build_fingerprint_override;
  }

  // Returns the storage location of the table cache protobuf file.
  base::FilePath TableCacheFilePathForTesting() { return TableCacheFilePath(); }

 private:
  // Scan the font file at |font_file_path| and given |ttc_index| and extract
  // full font name and postscript name from the font and store it into the
  // font_index_entry protobuf object.
  void IndexFile(blink::FontUniqueNameTable* font_table,
                 const std::string& font_file_path,
                 uint32_t ttc_index);
  // For a TrueType font collection, determine how many font faces are
  // available in a file.
  int32_t NumberOfFacesInFontFile(const std::string& font_filename) const;

  // If an Android build fingerprint override is set through
  // SetAndroidBuildFingerprint() return that, otherwise return the actual
  // platform's Android build fingerprint.
  std::string GetAndroidBuildFingerprint() const;

  // If an override is set through SetFontFilePathsForTesting() return those
  // fonts, otherwise enumerate font files in the the Android platform font
  // directories.
  std::vector<std::string> GetFontFilePaths() const;

  base::FilePath TableCacheFilePath();

  base::FilePath cache_directory_;
  FT_Library ft_library_;
  base::MappedReadOnlyRegion proto_storage_;

  std::string android_build_fingerprint_for_testing_ = "";
  std::vector<std::string> font_file_paths_for_testing_ =
      std::vector<std::string>();
};
}  // namespace content

#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_H_
