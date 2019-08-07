// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LANGUAGE_INFO_H_
#define UI_ACCESSIBILITY_AX_LANGUAGE_INFO_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "ui/accessibility/ax_export.h"

#include "third_party/cld_3/src/src/nnet_language_identifier.h"

namespace ui {

class AXNode;
class AXTree;

// This module implements language detection enabling Chrome to automatically
// detect the language for spans of text within the page without relying on any
// declared attributes.
//
// Language detection relies on two key data structures:
//   AXLanguageInfo represents the local language detection data for an AXNode.
//   AXLanguageInfoStats represents the 'global' (tree-level) language detection
//                       data for all nodes within an AXTree.
//
// Language detection is implemented as a two-pass process to reduce the
// assignment of spurious languages.
//
// After the first pass no languages have been assigned to AXNode(s), this is
// left to the second pass so that we can take use tree-level statistics to
// better inform the local language assigned.
//
// The first pass 'Detect' (entry point DetectLanguageForSubtree) walks the
// subtree from a given AXNode and attempts to detect the language of any text
// found. It records results in an instance of AXLanguageInfo which it stores on
// the AXNode, it also records statistics on the languages found in the
// AXLanguageInfoStats instance associated with each AXTree.
//
// The second pass 'Label' (entry point LabelLanguageForSubtree) walks the
// subtree from a given AXNode and attempts to find an appropriate language to
// associate with each AXNode based on a combination of the local detection
// results (AXLanguageInfo) and the global stats (AXLanguageInfoStats).

// An instance of AXLanguageInfo is used to record the detected and assigned
// languages for a single AXNode, this data is entirely local to the AXNode.
struct AX_EXPORT AXLanguageInfo {
  AXLanguageInfo();
  ~AXLanguageInfo();

  // This is the final language we have assigned for this node during the
  // 'label' step, it is the result of merging:
  //  a) The detected language for this node
  //  b) The declared lang attribute on this node
  //  c) the (recursive) language of the parent (detected or declared).
  //
  // This will be the empty string if no language was assigned during label
  // phase.
  //
  // IETF BCP 47 Language code (rfc5646).
  // examples:
  //  'de'
  //  'de-DE'
  //  'en'
  //  'en-US'
  //  'es-ES'
  //
  // This should not be read directly by clients of AXNode, instead clients
  // should call AXNode::GetLanguage().
  std::string language;

  // Detected languages for this node sorted as returned by
  // FindTopNMostFreqLangs, which sorts in decreasing order of probability,
  // filtered to remove any unreliable results.
  std::vector<std::string> detected_languages;
};

// A single AXLanguageInfoStats instance is stored on each AXTree and represents
// the language detection statistics for every AXNode within that AXTree.
//
// We rely on these tree-level statistics to avoid spurious language detection
// assignments.
//
// The Label step will only assign a detected language to a node if that
// language is one of the dominant languages on the page.
class AX_EXPORT AXLanguageInfoStats {
 public:
  AXLanguageInfoStats();
  ~AXLanguageInfoStats();

  // Adjust our statistics to add provided detected languages.
  void Add(const std::vector<std::string>& languages);

  // Fetch the score for a given language.
  int GetScore(const std::string& lang) const;

  // Check if a given language is within the top results.
  bool CheckLanguageWithinTop(const std::string& lang);

  chrome_lang_id::NNetLanguageIdentifier& GetLanguageIdentifier();

 private:
  // Store a count of the occurrences of a given language.
  std::unordered_map<std::string, unsigned int> lang_counts_;

  // Cache of last calculated top language results.
  // A vector of pairs of (score, language) sorted by descending score.
  std::vector<std::pair<unsigned int, std::string>> top_results_;
  // Boolean recording that we have not mutated the statistics since last
  // calculating top results, setting this to false will cause recalculation
  // when the results are next fetched.
  bool top_results_valid_;

  void InvalidateTopResults();

  // Populate top_results_.
  void GenerateTopResults();

  chrome_lang_id::NNetLanguageIdentifier language_identifier_;

  DISALLOW_COPY_AND_ASSIGN(AXLanguageInfoStats);
};

// Detect language for each node in the subtree rooted at the given node.
// This is the first pass in detection and labelling.
// This only detects the language, it does not label it, for that see
//  LabelLanguageForSubtree.
AX_EXPORT void DetectLanguageForSubtree(AXNode* subtree_root,
                                        class AXTree* tree);

// Label language for each node in the subtree rooted at the given node.
// This is the second pass in detection and labelling.
// This will label the language, but relies on the earlier detection phase
// having already completed.
//
// returns boolean indicating success.
AX_EXPORT bool LabelLanguageForSubtree(AXNode* subtree_root,
                                       class AXTree* tree);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LANGUAGE_INFO
