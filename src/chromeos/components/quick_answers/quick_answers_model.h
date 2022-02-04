// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace quick_answers {

// Interaction with the consent-view (used for logging).
enum class NoticeInteractionType {
  // When user clicks on the "grant-consent" button.
  kAccept = 0,
  // When user clicks on the "manage-settings" button.
  kManageSettings = 1,
  // When user otherwise dismisses or ignores the consent-view.
  kDismiss = 2
};

// The status of loading quick answers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersLoadStatus|.
enum class LoadStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kNoResult = 2,
  kMaxValue = kNoResult,
};

// The type of the result. Valid values are map to the search result types.
// Please see go/1ns-doc for more detail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersResultType|.
enum class ResultType {
  kNoResult = 0,
  kKnowledgePanelEntityResult = 3982,
  kDefinitionResult = 5493,
  kTranslationResult = 6613,
  kUnitConversionResult = 13668,
};

// The predicted intent of the request.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersIntentType|.
enum class IntentType {
  kUnknown = 0,
  kUnit = 1,
  kDictionary = 2,
  kTranslation = 3,
  kMaxValue = kTranslation
};

enum class QuickAnswerUiElementType {
  kUnknown = 0,
  kText = 1,
  kImage = 2,
};

// Enumeration of Quick Answers exit points. These values are persisted to logs.
// Entries should never be renumbered and numeric values should never be reused.
// Append to this enum is allowed only if the possible exit point grows.
enum class QuickAnswersExitPoint {
  // The exit point is unspecified. Might be used by tests, obsolete code or as
  // placeholders.
  kUnspecified = 0,
  KContextMenuDismiss = 1,
  kContextMenuClick = 2,
  kQuickAnswersClick = 3,
  kSettingsButtonClick = 4,
  kReportQueryButtonClick = 5,
  kMaxValue = kReportQueryButtonClick,
};

struct QuickAnswerUiElement {
  explicit QuickAnswerUiElement(QuickAnswerUiElementType type) : type(type) {}
  QuickAnswerUiElement(const QuickAnswerUiElement&) = default;
  QuickAnswerUiElement& operator=(const QuickAnswerUiElement&) = default;
  QuickAnswerUiElement(QuickAnswerUiElement&&) = default;

  QuickAnswerUiElementType type = QuickAnswerUiElementType::kUnknown;
};

// class to describe an answer text.
struct QuickAnswerText : public QuickAnswerUiElement {
  explicit QuickAnswerText(const std::string& text,
                           SkColor color = gfx::kGoogleGrey900)
      : QuickAnswerUiElement(QuickAnswerUiElementType::kText),
        text(base::UTF8ToUTF16(text)),
        color(color) {}

  std::u16string text;

  // Attributes for text style.
  SkColor color = SK_ColorBLACK;
};

struct QuickAnswerResultText : public QuickAnswerText {
 public:
  QuickAnswerResultText(const std::string& text,
                        SkColor color = gfx::kGoogleGrey700)
      : QuickAnswerText(text, color) {}
};

struct QuickAnswerImage : public QuickAnswerUiElement {
  explicit QuickAnswerImage(const gfx::Image& image)
      : QuickAnswerUiElement(QuickAnswerUiElementType::kImage), image(image) {}

  gfx::Image image;
};

// Structure to describe a quick answer.
struct QuickAnswer {
  QuickAnswer();
  ~QuickAnswer();

  ResultType result_type = ResultType::kNoResult;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> title;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> first_answer_row;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> second_answer_row;
  std::unique_ptr<QuickAnswerImage> image;

  // Phonetics audio URL for playing pronunciation of dictionary results.
  // For other type of results the URL will be empty.
  GURL phonetics_audio;
};

// Information of the device that used by the user to send the request.
struct DeviceProperties {
  // Device language code.
  std::string language;

  // List (separated by comma) of user preferred languages.
  std::string preferred_languages;

  // Whether the request is send by an internal user.
  bool is_internal = false;
};

struct IntentInfo {
  IntentInfo();
  IntentInfo(const IntentInfo& other);
  IntentInfo(const std::string& intent_text,
             IntentType intent_type,
             const std::string& source_language = std::string(),
             const std::string& target_language = std::string());
  ~IntentInfo();

  // The text extracted from the selected_text associated with the intent.
  std::string intent_text;

  // Predicted intent.
  IntentType intent_type = IntentType::kUnknown;

  // Source and target language for translation query.
  // These fields should only be used for translation intents.
  std::string source_language;
  std::string target_language;
};

// Extract information generated from |QuickAnswersRequest|.
struct PreprocessedOutput {
  PreprocessedOutput();
  PreprocessedOutput(const PreprocessedOutput& other);
  ~PreprocessedOutput();

  IntentInfo intent_info;

  // Rewritten query based on |intent_type| and |intent_text|.
  std::string query;
};

// Structure of quick answers request context, including device properties and
// surrounding text.
struct Context {
  // Device specific properties.
  DeviceProperties device_properties;

  std::string surrounding_text;
};

// Structure to describe an quick answer request including selected content and
// context.
struct QuickAnswersRequest {
  QuickAnswersRequest();
  QuickAnswersRequest(const QuickAnswersRequest& other);
  ~QuickAnswersRequest();

  // The selected Text.
  std::string selected_text;

  // Output of processed result.
  PreprocessedOutput preprocessed_output;

  // Context information.
  Context context;

  // TODO(b/169346016): Add context and other targeted objects (e.g: images,
  // links, etc).
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
