// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_TRAFFIC_ANNOTATION_AUDITOR_TRAFFIC_ANNOTATION_EXPORTER_H_
#define TOOLS_TRAFFIC_ANNOTATION_AUDITOR_TRAFFIC_ANNOTATION_EXPORTER_H_

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "tools/traffic_annotation/auditor/instance.h"

class TrafficAnnotationExporter {
 public:
  TrafficAnnotationExporter(const base::FilePath& source_path);
  ~TrafficAnnotationExporter();
  TrafficAnnotationExporter(const TrafficAnnotationExporter&) = delete;
  TrafficAnnotationExporter(TrafficAnnotationExporter&&) = delete;

  // Loads annotations from annotations.xml file into |annotation_items_|.
  bool LoadAnnotationsXML();

  // Updates |annotation_items_| with current set of extracted annotations and
  // reserved ids. Sets the |modified_| flag if any item is updated.
  bool UpdateAnnotations(const std::vector<AnnotationInstance>& annotations,
                         const std::map<int, std::string>& reserved_ids);

  // Saves |annotation_items_| into annotations.xml.
  bool SaveAnnotationsXML();

  // Returns the required updates for annotations.xml.
  std::string GetRequiredUpdates();

  std::string GetXMLDifferencesForTesting(const std::string& old_xml,
                                          const std::string& new_xml) {
    return GetXMLDifferences(old_xml, new_xml);
  }

  // Produces the list of deprecated hash codes. Returns false if
  // annotations.xml is not and cannot be loaded.
  bool GetDeprecatedHashCodes(std::set<int>* hash_codes);

  bool modified() { return modified_; }

  // Runs tests on content of |annotation_items_|.
  bool CheckAnnotationItems();

  // Returns the number of items in annotations.xml for testing.
  unsigned GetXMLItemsCountForTesting();

 private:
  struct AnnotationItem {
    AnnotationItem();
    AnnotationItem(const AnnotationItem& other);
    ~AnnotationItem();

    AnnotationInstance::Type type;

    int unique_id_hash_code;
    int second_id_hash_code;
    int content_hash_code;

    std::string deprecation_date;
    std::vector<std::string> os_list;

    std::set<int> semantics_fields;
    std::set<int> policy_fields;
    std::string file_path;
  };

  // Generates a text serialized XML for current report items.
  std::string GenerateSerializedXML();

  // Returns the required updates to convert one serialized XML to another.
  std::string GetXMLDifferences(const std::string& old_xml,
                                const std::string& new_xml);

  std::vector<std::string> all_supported_platforms_;
  std::map<std::string, AnnotationItem> annotation_items_;
  const base::FilePath source_path_;
  bool modified_;
};

#endif  // TOOLS_TRAFFIC_ANNOTATION_AUDITOR_TRAFFIC_ANNOTATION_EXPORTER_H_