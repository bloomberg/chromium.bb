// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_test_utils.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_constants.h"

namespace ash {
namespace enhanced_network_tts {
namespace {

bool HasOneDecimalDigit(absl::optional<double> rate) {
  if (!rate.has_value())
    return false;
  return std::abs(static_cast<int>(rate.value() * 10) - rate.value() * 10) <
         kDoubleCompareAccuracy;
}

}  // namespace

const char kFullRequestTemplate[] =
    R"({
        "advanced_options": {
          "audio_generation_options": {"speed_factor": %.1f},
          "force_language": "%s"
        },
        "text": {
          "text_parts": ["%s"]
        },
        "voice_settings": {
          "voice_criteria_and_selections": [{
            "criteria": {"language": "%s"},
            "selection": {"default_voice": "%s"}
          }]
        }
      })";

extern const char kSimpleRequestTemplate[] =
    R"({"advanced_options": {
          "audio_generation_options": {"speed_factor": %.1f}
        },
        "text": {"text_parts": ["%s"]}})";

extern const char kTemplateResponse[] =
    R"([
        {"metadata": {}},
        {"text": {
          "timingInfo": [
            {
              "text": "test1",
              "location": {
                "textLocation": {"length": 5},
                "timeLocation": {
                  "timeOffset": "0.01s",
                  "duration": "0.14s"
                }
              }
            },
            {
              "text": "test2",
              "location": {
                "textLocation": {"length": 5, "offset": 6},
                "timeLocation": {
                  "timeOffset": "0.16s",
                  "duration": "0.17s"
                }
              }
            }
          ]}
        },
        {"audio": {"bytes": "%s"}}
      ])";

std::string CreateCorrectRequest(const std::string& input_text,
                                 float rate,
                                 const std::string& voice_name,
                                 const std::string& lang) {
  return base::StringPrintf(kFullRequestTemplate, rate, lang.c_str(),
                            input_text.c_str(), lang.c_str(),
                            voice_name.c_str());
}

std::string CreateCorrectRequest(const std::string& input_text, float rate) {
  return base::StringPrintf(kSimpleRequestTemplate, rate, input_text.c_str());
}

std::string CreateServerResponse(const std::vector<uint8_t>& expected_output) {
  std::string encoded_output(expected_output.begin(), expected_output.end());
  base::Base64Encode(encoded_output, &encoded_output);
  return base::StringPrintf(kTemplateResponse, encoded_output.c_str());
}

bool AreRequestsEqual(const std::string& json_a, const std::string& json_b) {
  const std::unique_ptr<base::Value> dict_a =
      base::JSONReader::ReadDeprecated(json_a);
  const std::unique_ptr<base::Value> dict_b =
      base::JSONReader::ReadDeprecated(json_b);

  const absl::optional<double> rate_a =
      dict_a->FindDoublePath(kSpeechFactorPath);
  const absl::optional<double> rate_b =
      dict_b->FindDoublePath(kSpeechFactorPath);
  // Speech rates should have only one decimal digit.
  if (!HasOneDecimalDigit(rate_a) || !HasOneDecimalDigit(rate_b))
    return false;
  // Compare the speech rates with |kDoubleCompareAccuracy|.
  if (std::abs(rate_a.value() - rate_b.value()) > kDoubleCompareAccuracy)
    return false;

  // Compare the dicts without the speech rates.
  dict_a->RemovePath(kSpeechFactorPath);
  dict_b->RemovePath(kSpeechFactorPath);
  return *dict_a == *dict_b;
}

}  // namespace enhanced_network_tts
}  // namespace ash
