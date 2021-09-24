// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/network_conditions.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/network_list.h"
#include "chrome/test/chromedriver/chrome/status.h"

NetworkConditions::NetworkConditions() {}
NetworkConditions::NetworkConditions(
    bool offline, double latency, double download_throughput,
    double upload_throughput)
  : offline(offline),
    latency(latency),
    download_throughput(download_throughput),
    upload_throughput(upload_throughput) {}
NetworkConditions::~NetworkConditions() {}

Status FindPresetNetwork(std::string network_name,
                         NetworkConditions* network_conditions) {
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          kNetworks, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.value)
    return Status(kUnknownError, "could not parse network list because " +
                                     parsed_json.error_message);

  if (!parsed_json.value->is_list())
    return Status(kUnknownError, "malformed networks list");

  for (const auto& entry : parsed_json.value->GetList()) {
    const base::DictionaryValue* network = nullptr;
    if (!entry.GetAsDictionary(&network)) {
      return Status(kUnknownError,
                    "malformed network in list: should be a dictionary");
    }

    if (network == NULL)
      continue;

    std::string title;
    if (!network->GetString("title", &title)) {
      return Status(kUnknownError,
                    "malformed network title: should be a string");
    }
    if (title != network_name)
      continue;

    absl::optional<double> maybe_latency = network->FindDoubleKey("latency");
    absl::optional<double> maybe_throughput =
        network->FindDoubleKey("throughput");

    if (!maybe_latency.has_value()) {
      return Status(kUnknownError,
                    "malformed network latency: should be a double");
    }
    // Preset list maintains a single "throughput" attribute for each network,
    // so we use that value for both |download_throughput| and
    // |upload_throughput| in the NetworkConditions (as does Chrome).
    if (!maybe_throughput.has_value()) {
      return Status(kUnknownError,
                    "malformed network throughput: should be a double");
    }

    network_conditions->latency = maybe_latency.value();
    // The throughputs of the network presets are listed in kbps, but
    // must be supplied to the OverrideNetworkConditions command as bps.
    network_conditions->download_throughput = maybe_throughput.value() * 1024;
    network_conditions->upload_throughput = maybe_throughput.value() * 1024;

    // |offline| is always false for now.
    network_conditions->offline = false;
    return Status(kOk);
  }

  return Status(kUnknownError, "must be a valid network");
}
