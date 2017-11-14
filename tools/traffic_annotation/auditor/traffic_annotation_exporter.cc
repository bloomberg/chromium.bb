// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/traffic_annotation_exporter.h"

#include <ctime>

#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_auditor.h"

namespace {

const char* kXmlComment =
    "<!--\n"
    "Copyright 2017 The Chromium Authors. All rights reserved.\n"
    "Use of this source code is governed by a BSD-style license that can be\n"
    "found in the LICENSE file.\n"
    "\nRefer to README.md for content description and update process.\n"
    "-->\n\n";

const base::FilePath kAnnotationsXmlPath =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation"))
        .Append(FILE_PATH_LITERAL("summary"))
        .Append(FILE_PATH_LITERAL("annotations.xml"));

// Extracts annotation id from a line of XML. Expects to have the line in the
// following format: <... id="..." .../>
// TODO(rhalavati): Use real XML parsing.
std::string GetAnnotationID(const std::string& xml_line) {
  std::string::size_type start = xml_line.find("id=\"");
  if (start == std::string::npos)
    return "";

  start += 4;
  std::string::size_type end = xml_line.find("\"", start);
  if (end == std::string::npos)
    return "";

  return xml_line.substr(start, end - start);
}

// Extracts a map of XML items, keyed by their 'id' tags, from a serialized XML.
void ExtractXMLItems(const std::string& serialized_xml,
                     std::map<std::string, std::string>* items) {
  std::vector<std::string> lines = base::SplitString(
      serialized_xml, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& line : lines) {
    std::string id = GetAnnotationID(line);
    if (!id.empty())
      items->insert(std::make_pair(id, line));
  }
}

}  // namespace

TrafficAnnotationExporter::ReportItem::ReportItem()
    : unique_id_hash_code(-1), content_hash_code(-1) {}

TrafficAnnotationExporter::ReportItem::ReportItem(
    const TrafficAnnotationExporter::ReportItem& other)
    : unique_id_hash_code(other.unique_id_hash_code),
      deprecation_date(other.deprecation_date),
      content_hash_code(other.content_hash_code),
      os_list(other.os_list) {}

TrafficAnnotationExporter::ReportItem::~ReportItem() {}

TrafficAnnotationExporter::TrafficAnnotationExporter(
    const base::FilePath& source_path)
    : source_path_(source_path), modified_(false) {
  all_supported_platforms_.push_back("linux");
  all_supported_platforms_.push_back("windows");
}

TrafficAnnotationExporter::~TrafficAnnotationExporter() {}

bool TrafficAnnotationExporter::LoadAnnotationsXML() {
  report_items_.clear();
  XmlReader reader;
  if (!reader.LoadFile(
          source_path_.Append(kAnnotationsXmlPath).MaybeAsASCII())) {
    LOG(ERROR) << "Could not load '"
               << source_path_.Append(kAnnotationsXmlPath).MaybeAsASCII()
               << "'.";
    return false;
  }

  bool all_ok = false;
  while (reader.Read()) {
    all_ok = true;
    if (reader.NodeName() != "item")
      continue;

    ReportItem item;
    std::string temp;
    std::string unique_id;

    all_ok &= reader.NodeAttribute("id", &unique_id);
    all_ok &= reader.NodeAttribute("hash_code", &temp) &&
              base::StringToInt(temp, &item.unique_id_hash_code);
    if (all_ok && reader.NodeAttribute("content_hash_code", &temp))
      all_ok &= base::StringToInt(temp, &item.content_hash_code);
    else
      item.content_hash_code = -1;

    reader.NodeAttribute("deprecated", &item.deprecation_date);

    if (reader.NodeAttribute("os_list", &temp)) {
      item.os_list = base::SplitString(temp, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY);
    }

    if (!all_ok) {
      LOG(ERROR) << "Unexpected format in annotations.xml.";
      break;
    }

    report_items_.insert(std::make_pair(unique_id, item));
  }

  modified_ = false;
  return all_ok;
}

bool TrafficAnnotationExporter::UpdateAnnotations(
    const std::vector<AnnotationInstance>& annotations,
    const std::map<int, std::string>& reserved_ids) {
  std::string platform;
#if defined(OS_LINUX)
  platform = "linux";
#elif defined(OS_WIN)
  platform = "windows";
#else
  NOTREACHED() << "Other platforms are not supported yet.";
#endif

  if (report_items_.empty() && !LoadAnnotationsXML())
    return false;

  std::set<int> current_platform_hashcodes;

  // Iterate annotations extracted from the code, and add/update them in the
  // reported list, if required.
  for (AnnotationInstance annotation : annotations) {
    // Compute a hashcode for the annotation. Source field is not used in this
    // computation as we don't need sensitivity to changes in source location,
    // i.e. filepath, line number and function.
    std::string content;
    annotation.proto.clear_source();
    google::protobuf::TextFormat::PrintToString(annotation.proto, &content);
    int content_hash_code = TrafficAnnotationAuditor::ComputeHashValue(content);

    // If annotation unique id is already in the reported list, just check if
    // platform is correct and content is not changed.
    if (base::ContainsKey(report_items_, annotation.proto.unique_id())) {
      ReportItem* current = &report_items_[annotation.proto.unique_id()];
      if (!base::ContainsValue(current->os_list, platform)) {
        current->os_list.push_back(platform);
        modified_ = true;
      }
      if (current->content_hash_code != content_hash_code) {
        current->content_hash_code = content_hash_code;
        modified_ = true;
      }
    } else {
      // If annotation is new, add it and assume it is on all platforms. Tests
      // running on other platforms will request updating this if required.
      ReportItem new_item;
      new_item.unique_id_hash_code = annotation.unique_id_hash_code;
      new_item.content_hash_code = content_hash_code;
      new_item.os_list = all_supported_platforms_;
      report_items_[annotation.proto.unique_id()] = new_item;
      modified_ = true;
    }
    current_platform_hashcodes.insert(annotation.unique_id_hash_code);
  }

  // If a none-reserved annotation is removed from current platform, update it.
  for (auto& item : report_items_) {
    if (base::ContainsValue(item.second.os_list, platform) &&
        item.second.content_hash_code != -1 &&
        !base::ContainsKey(current_platform_hashcodes,
                           item.second.unique_id_hash_code)) {
      base::Erase(item.second.os_list, platform);
      modified_ = true;
    }
  }

  // If there is a new reserved id, add it.
  for (const auto& item : reserved_ids) {
    if (!base::ContainsKey(report_items_, item.second)) {
      ReportItem new_item;
      new_item.unique_id_hash_code = item.first;
      new_item.os_list = all_supported_platforms_;
      report_items_[item.second] = new_item;
      modified_ = true;
    }
  }

  // If there are annotations that are not used in any OS, set the deprecation
  // flag.
  for (auto& item : report_items_) {
    if (item.second.os_list.empty() && item.second.deprecation_date.empty()) {
      base::Time::Exploded now;
      base::Time::Now().UTCExplode(&now);
      item.second.deprecation_date = base::StringPrintf(
          "%i-%02i-%02i", now.year, now.month, now.day_of_month);
      modified_ = true;
    }
  }

  return CheckReportItems();
}

std::string TrafficAnnotationExporter::GenerateSerializedXML() {
  XmlWriter writer;
  writer.StartWriting();
  writer.StartElement("annotations");

  for (const auto& item : report_items_) {
    writer.StartElement("item");
    writer.AddAttribute("id", item.first);
    writer.AddAttribute(
        "hash_code", base::StringPrintf("%i", item.second.unique_id_hash_code));
    if (!item.second.deprecation_date.empty())
      writer.AddAttribute("deprecated", item.second.deprecation_date);
    if (item.second.content_hash_code == -1)
      writer.AddAttribute("reserved", "1");
    else
      writer.AddAttribute(
          "content_hash_code",
          base::StringPrintf("%i", item.second.content_hash_code));
    std::string os_list;
    for (const std::string& platform : item.second.os_list)
      os_list += platform + ",";
    if (!os_list.empty()) {
      os_list.pop_back();
      writer.AddAttribute("os_list", os_list);
    }
    writer.EndElement();
  }
  writer.EndElement();

  writer.StopWriting();
  std::string xml_content = writer.GetWrittenString();
  // Add comment before annotation tag (and after xml version).
  xml_content.insert(xml_content.find("<annotations>"), kXmlComment);

  return xml_content;
}

bool TrafficAnnotationExporter::SaveAnnotationsXML() {
  std::string xml_content = GenerateSerializedXML();

  return base::WriteFile(source_path_.Append(kAnnotationsXmlPath),
                         xml_content.c_str(), xml_content.length()) != -1;
}

bool TrafficAnnotationExporter::GetDeprecatedHashCodes(
    std::set<int>* hash_codes) {
  if (report_items_.empty() && !LoadAnnotationsXML())
    return false;

  hash_codes->clear();
  for (const auto& item : report_items_) {
    if (!item.second.deprecation_date.empty())
      hash_codes->insert(item.second.unique_id_hash_code);
  }
  return true;
}

bool TrafficAnnotationExporter::CheckReportItems() {
  // Check for annotation hash code duplications.
  std::set<int> used_codes;
  for (auto& item : report_items_) {
    if (base::ContainsKey(used_codes, item.second.unique_id_hash_code)) {
      LOG(ERROR) << "Unique id hash code " << item.second.unique_id_hash_code
                 << " is used more than once.";
      return false;
    } else {
      used_codes.insert(item.second.unique_id_hash_code);
    }
  }

  // Check for coexistence of OS(es) and deprecation date.
  for (auto& item : report_items_) {
    if (!item.second.deprecation_date.empty() && !item.second.os_list.empty()) {
      LOG(ERROR) << "Annotation " << item.first
                 << " has a deprecation date and at least one active OS.";
      return false;
    }
  }
  return true;
}

unsigned TrafficAnnotationExporter::GetXMLItemsCountForTesting() {
  std::string xml_content;
  if (!base::ReadFileToString(
          base::MakeAbsoluteFilePath(source_path_.Append(kAnnotationsXmlPath)),
          &xml_content)) {
    LOG(ERROR) << "Could not read 'annotations.xml'.";
    return 0;
  }

  std::map<std::string, std::string> lines;
  ExtractXMLItems(xml_content, &lines);
  return lines.size();
}

std::string TrafficAnnotationExporter::GetRequiredUpdates() {
  std::string old_xml;
  if (!base::ReadFileToString(
          base::MakeAbsoluteFilePath(source_path_.Append(kAnnotationsXmlPath)),
          &old_xml)) {
    return "Could not generate required changes.";
  }

  return GetXMLDifferences(old_xml, GenerateSerializedXML());
}

std::string TrafficAnnotationExporter::GetXMLDifferences(
    const std::string& old_xml,
    const std::string& new_xml) {
  std::map<std::string, std::string> old_items;
  ExtractXMLItems(old_xml, &old_items);
  std::set<std::string> old_keys;
  std::transform(old_items.begin(), old_items.end(),
                 std::inserter(old_keys, old_keys.end()),
                 [](auto pair) { return pair.first; });

  std::map<std::string, std::string> new_items;
  ExtractXMLItems(new_xml, &new_items);
  std::set<std::string> new_keys;
  std::transform(new_items.begin(), new_items.end(),
                 std::inserter(new_keys, new_keys.end()),
                 [](auto pair) { return pair.first; });

  std::set<std::string> added_items;
  std::set<std::string> removed_items;

  std::set_difference(new_keys.begin(), new_keys.end(), old_keys.begin(),
                      old_keys.end(),
                      std::inserter(added_items, added_items.begin()));
  std::set_difference(old_keys.begin(), old_keys.end(), new_keys.begin(),
                      new_keys.end(),
                      std::inserter(removed_items, removed_items.begin()));

  std::string message;

  for (const std::string& id : added_items) {
    message += base::StringPrintf("\n\tAdd line: '%s'", new_items[id].c_str());
  }

  for (const std::string& id : removed_items) {
    message +=
        base::StringPrintf("\n\tRemove line: '%s'", old_items[id].c_str());
  }

  for (const std::string& id : old_keys) {
    if (base::ContainsKey(new_items, id) && old_items[id] != new_items[id]) {
      message +=
          base::StringPrintf("\n\tUpdate line: '%s' --> '%s'",
                             old_items[id].c_str(), new_items[id].c_str());
    }
  }

  return message;
}