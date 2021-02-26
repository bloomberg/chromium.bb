// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SHARED_STRUCTS_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SHARED_STRUCTS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace chromeos {
namespace local_search_service {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IndexId { kCrosSettings = 0, kHelpApp = 1, kMaxValue = kHelpApp };

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Backend {
  kLinearMap = 0,
  kInvertedIndex = 1,
  kMaxValue = kInvertedIndex
};

struct Content {
  // An identifier for the content in Data.
  std::string id;
  base::string16 content;
  // |weight| represents how important this Content is and is used in
  // calculating overall matching score of its enclosing Data item. When a query
  // matches a Data item it is matching some Content of the Data. If the
  // matching Content has a larger weight, the overall matching score will be
  // higher. The range is in [0,1].
  // TODO(jiameng): it will be used by kInvertedIndex only. We may consider
  // extending to kLinearMap.
  double weight = 1.0;
  Content(const std::string& id,
          const base::string16& content,
          double weight = 1.0);
  Content();
  Content(const Content& content);
  ~Content();
};

struct Data {
  // Identifier of the data item, should be unique across the registry. Clients
  // will decide what ids to use, they could be paths, urls or any opaque string
  // identifiers.
  // Ideally IDs should persist across sessions, but this is not strictly
  // required now because data is not persisted across sessions.
  std::string id;

  // Data item will be matched between its search tags and query term.
  std::vector<Content> contents;

  // Locale of the data. This is currently used by inverted index only.
  // If unset, we will use system configured locale.
  // TODO(jiameng): apply locale-dependent tokenization to linear map.
  std::string locale;
  Data(const std::string& id,
       const std::vector<Content>& contents,
       const std::string& locale = "");
  Data();
  Data(const Data& data);
  ~Data();
};

struct SearchParams {
  // |relevance_threshold| will be applicable if the backend is kLinearMap.
  // Relevance score will be calculated as a combination of prefix and fuzzy
  // matching. A Data item is relevant if the overall relevance score is above
  // this threshold. The threshold should be in [0,1].
  double relevance_threshold = 0.32;
  // |prefix_threshold| and |fuzzy_threshold| will be applicable if the backend
  // is kInvertedIndex. When a query term is matched against a Data item, it
  // will be considered relevant if either its prefix score is above
  // |prefix_threshold| or fuzzy score is above |fuzzy_threshold|. Both of these
  // thresholds should be in [0,1].
  // TODO(jiameng): revise default values.
  double prefix_threshold = 0.6;
  double fuzzy_threshold = 0.6;
};

struct Position {
  Position();
  Position(const Position& position);
  Position(const std::string& content_id, uint32_t start, uint32_t length);
  ~Position();
  std::string content_id;
  // TODO(jiameng): |start| and |end| will be implemented for inverted index
  // later.
  uint32_t start;
  uint32_t length;
};

// Result is one item that matches a given query. It contains the id of the item
// and its matching score.
struct Result {
  // Id of the data.
  std::string id;
  // Relevance score.
  // Currently only linear map is implemented with fuzzy matching and score will
  // always be in [0,1]. In the future, when an inverted index is implemented,
  // the score will not be in this range any more. Client will be able to select
  // a search backend to use (linear map vs inverted index) and hence client
  // will be able to expect the range of the scores.
  double score;
  // Position of the matching text.
  // We currently use linear map, which will return one matching content, hence
  // the vector has only one element. When we have inverted index, we will have
  // multiple matching contents.
  std::vector<Position> positions;
  Result();
  Result(const Result& result);
  Result(const std::string& id,
         double score,
         const std::vector<Position>& positions);
  ~Result();
};

// Status of the search attempt.
// These numbers are used for logging and should not be changed or reused. More
// will be added later.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ResponseStatus {
  kUnknownError = 0,
  // Search operation is successful. But there could be no matching item and
  // result list is empty.
  kSuccess = 1,
  // Query is empty.
  kEmptyQuery = 2,
  // Index is empty (i.e. no data).
  kEmptyIndex = 3,
  kMaxValue = kEmptyIndex
};

// Similar to Position but also contains weight from Content.
// This is used in ranking and is not meant to be returned as part of the search
// results.
struct WeightedPosition {
  double weight;
  Position position;
  WeightedPosition();
  WeightedPosition(const WeightedPosition& weighted_position);
  WeightedPosition(double weight, const Position& position);
  ~WeightedPosition();
};

// Stores the token (after processed). |positions| represents the token's
// positions in one document.
struct Token {
  Token();
  Token(const Token& token);
  Token(const base::string16& text, const std::vector<WeightedPosition>& pos);
  ~Token();
  base::string16 content;
  std::vector<WeightedPosition> positions;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SHARED_STRUCTS_H_
