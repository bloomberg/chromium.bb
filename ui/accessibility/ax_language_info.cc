// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_language_info.h"
#include <algorithm>
#include <functional>

#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

namespace {
// This is the maximum number of languages we assign per page, so only the top
// 3 languages on the top will be assigned to any node.
const auto kMaxDetectedLanguagesPerPage = 3;

// This is the maximum number of languages that cld3 will detect for each
// input we give it, 3 was recommended to us by the ML team as a good
// starting point.
const auto kMaxDetectedLanguagesPerSpan = 3;
}  // namespace

AXLanguageInfo::AXLanguageInfo() {}
AXLanguageInfo::~AXLanguageInfo() {}

AXLanguageInfoStats::AXLanguageInfoStats() : top_results_valid_(false) {}
AXLanguageInfoStats::~AXLanguageInfoStats() = default;

chrome_lang_id::NNetLanguageIdentifier&
AXLanguageInfoStats::GetLanguageIdentifier() {
  return language_identifier_;
}

void AXLanguageInfoStats::Add(const std::vector<std::string>& languages) {
  // Assign languages with higher probability a higher score.
  // TODO(chrishall): consider more complex scoring
  size_t score = kMaxDetectedLanguagesPerSpan;
  for (const auto& lang : languages) {
    lang_counts_[lang] += score;
    --score;
  }

  InvalidateTopResults();
}

int AXLanguageInfoStats::GetScore(const std::string& lang) const {
  const auto& lang_count_it = lang_counts_.find(lang);
  if (lang_count_it == lang_counts_.end()) {
    return 0;
  }
  return lang_count_it->second;
}

void AXLanguageInfoStats::InvalidateTopResults() {
  top_results_valid_ = false;
}

// Check if a given language is within the top results.
bool AXLanguageInfoStats::CheckLanguageWithinTop(const std::string& lang) {
  if (!top_results_valid_) {
    GenerateTopResults();
  }

  for (const auto& item : top_results_) {
    if (lang == item.second) {
      return true;
    }
  }

  return false;
}

void AXLanguageInfoStats::GenerateTopResults() {
  top_results_.clear();

  for (const auto& item : lang_counts_) {
    top_results_.emplace_back(item.second, item.first);
  }

  // Since we store the pair as (score, language) the default operator> on pairs
  // does our sort appropriately.
  // Sort in descending order.
  std::sort(top_results_.begin(), top_results_.end(),
            std::greater<std::pair<unsigned int, std::string>>());

  // Resize down to remove all values greater than the N we are considering.
  top_results_.resize(kMaxDetectedLanguagesPerPage);

  top_results_valid_ = true;
}

static void DetectLanguageForSubtreeInternal(AXNode* node, class AXTree* tree);

// Detect language for a subtree rooted at the given node.
void DetectLanguageForSubtree(AXNode* subtree_root, class AXTree* tree) {
  TRACE_EVENT0("accessibility", "AXLanguageInfo::DetectLanguageForSubtree");

  DCHECK(subtree_root);
  DCHECK(tree);
  if (!::switches::IsExperimentalAccessibilityLanguageDetectionEnabled()) {
    // If feature is not enabled we still return success as we were as
    // successful as we could have been.
    return;
  }

  if (!tree->language_info_stats) {
    tree->language_info_stats.reset(new AXLanguageInfoStats());
  }

  DetectLanguageForSubtreeInternal(subtree_root, tree);
}

// Detect language for a subtree rooted at the given node
// will not check feature flag.
static void DetectLanguageForSubtreeInternal(AXNode* node, class AXTree* tree) {
  if (node->IsText()) {
    AXLanguageInfoStats* lang_info_stats = tree->language_info_stats.get();
    AXLanguageInfo* lang_info = node->GetLanguageInfo();
    if (!lang_info) {
      // TODO(chrishall): consider space optimisations.
      // Currently we keep these language info instances around until
      // destruction of the containing node, this is due to us treating AXNode
      // as otherwise read-only and so we store any detected language
      // information on lang info.

      node->SetLanguageInfo(std::make_unique<AXLanguageInfo>());
      lang_info = node->GetLanguageInfo();
    } else {
      lang_info->detected_languages.clear();
    }

    chrome_lang_id::NNetLanguageIdentifier& language_identifier =
        tree->language_info_stats->GetLanguageIdentifier();

    // TODO(chrishall): implement strategy for nodes which are too small to get
    // reliable language detection results. Consider combination of
    // concatenation and bubbling up results.
    auto text = node->GetStringAttribute(ax::mojom::StringAttribute::kName);

    const auto results = language_identifier.FindTopNMostFreqLangs(
        text, kMaxDetectedLanguagesPerSpan);

    for (const auto res : results) {
      // The output of FindTopNMostFreqLangs is already sorted by byte count,
      // this seems good enough for now.
      // Only consider results which are 'reliable', this will also remove
      // 'unknown'.
      if (res.is_reliable) {
        lang_info->detected_languages.push_back(res.language);
      }
    }
    lang_info_stats->Add(lang_info->detected_languages);
  }

  // TODO(chrishall): refactor this as textnodes only ever have inline text
  // boxen as children. This means we don't need to recurse except for
  // inheritance which can be handled elsewhere.
  for (AXNode* child : node->children()) {
    DetectLanguageForSubtreeInternal(child, tree);
  }
}

static void LabelLanguageForSubtreeInternal(AXNode* node, class AXTree* tree);

// Label language for each node in the subtree rooted at the given node.
// This relies on DetectLanguageForSubtree having already been run.
bool LabelLanguageForSubtree(AXNode* subtree_root, class AXTree* tree) {
  TRACE_EVENT0("accessibility", "AXLanguageInfo::LabelLanguageForSubtree");

  DCHECK(subtree_root);
  DCHECK(tree);

  if (!::switches::IsExperimentalAccessibilityLanguageDetectionEnabled()) {
    // If feature is not enabled we still return success as we were as
    // successful as we could have been.
    return true;
  }

  if (!tree->language_info_stats) {
    // Detection has not been performed, error, the user is holding this wrong.
    // DetectLanguageForSubtree must always be called on a given subtree before
    // LabelLanguageForSubtree is called.
    LOG(FATAL) << "LabelLanguageForSubtree run before DetectLanguageForSubtree";
    return false;
  }

  LabelLanguageForSubtreeInternal(subtree_root, tree);
  return true;
}

static void LabelLanguageForSubtreeInternal(AXNode* node, class AXTree* tree) {
  AXLanguageInfo* lang_info = node->GetLanguageInfo();

  // lang_info is only attached by Detect when it thinks a node is interesting,
  // the presence of lang_info means that Detect expects the node to end up with
  // a language specified.
  //
  // If the lang_info->language is already set then we have no more work to do
  // for this node.
  if (lang_info && lang_info->language.empty()) {
    AXLanguageInfoStats* lang_info_stats = tree->language_info_stats.get();
    for (const auto& lang : lang_info->detected_languages) {
      if (lang_info_stats->CheckLanguageWithinTop(lang)) {
        lang_info->language = lang;
        break;
      }
    }

    // TODO(chrishall): consider obeying the author declared lang tag in some
    // cases, either based on proximity or based on common language detection
    // error cases.

    // If language is still empty then we failed to detect a language from
    // this node, we will instead try construct a language from other sources
    // including any lang attribute and any language from the parent tree.
    if (lang_info->language.empty()) {
      const auto& lang_attr =
          node->GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
      if (!lang_attr.empty()) {
        lang_info->language = lang_attr;
      } else {
        // We call GetLanguage() on our parent which will return a detected
        // language if it has one, otherwise it will search up the tree for a
        // kLanguage attribute.
        //
        // This means that lang attributes are inherited indefinitely but
        // detected language is only inherited one level.
        //
        // Currently we only attach detected language to text nodes, once we
        // start attaching detected language on other nodes we need to rethink
        // this. We may want to attach detected language information once we
        // consider combining multiple smaller text nodes into one larger one.
        //
        // TODO(chrishall): reconsider detected language inheritance.
        AXNode* parent = node->parent();
        if (parent) {
          const auto& parent_lang = parent->GetLanguage();
          if (!parent_lang.empty()) {
            lang_info->language = parent_lang;
          }
        }
      }
    }
  }

  for (AXNode* child : node->children()) {
    LabelLanguageForSubtreeInternal(child, tree);
  }
}

}  // namespace ui
