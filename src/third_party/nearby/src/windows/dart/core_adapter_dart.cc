// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/dart_lang/v2/runtime/include/dart_api_dl.h"
#include "third_party/dart_lang/v2/runtime/include/dart_native_api.h"
#include "core/core.h"
#include "third_party/nearby/windows/dart/core_adapter_dart.h"

namespace location {
namespace nearby {
namespace connections {
namespace windows {

Strategy GetStrategy(StrategyDart strategy) {
  switch (strategy) {
    case StrategyDart::P2P_CLUSTER:
      return Strategy::kP2pCluster;
    case StrategyDart::P2P_POINT_TO_POINT:
      return Strategy::kP2pPointToPoint;
    case StrategyDart::P2P_STAR:
      return Strategy::kP2pStar;
  }
  return Strategy::kNone;
}

ByteArray ConvertBluetoothMacAddress(absl::string_view address) {
  return ByteArray(address.data());
}

void SetResultCallback(ResultCallback &callback, Dart_Port &port) {
  callback.result_cb = [port](Status status) {
    Dart_CObject dart_object_result_callback;
    dart_object_result_callback.type = Dart_CObject_kInt64;
    dart_object_result_callback.value.as_int64 = status.value;
    const bool result = Dart_PostCObject_DL(port, &dart_object_result_callback);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
}

void PostResult(Dart_Port &result_cb, Status::Value value) {
  Dart_CObject dart_object_result_callback;
  dart_object_result_callback.type = Dart_CObject_kInt64;
  dart_object_result_callback.value.as_int64 = value;
  const bool result =
      Dart_PostCObject_DL(result_cb, &dart_object_result_callback);
  if (!result) {
    NEARBY_LOG(INFO, "Returning error to port failed.");
  }
}

void StartAdvertisingDart(Core *pCore, const char *service_id,
                          ConnectionOptionsDart options_dart,
                          ConnectionRequestInfoDart info_dart,
                          Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  AdvertisingOptions options;
  options.strategy = GetStrategy(options_dart.strategy);
  options.auto_upgrade_bandwidth = options_dart.auto_upgrade_bandwidth;
  options.enforce_topology_constraints =
      options_dart.enforce_topology_constraints;
  options.allowed.bluetooth = options_dart.enable_bluetooth;
  options.allowed.ble = options_dart.enable_ble;
  options.low_power = options_dart.use_low_power_mode;
  options.fast_advertisement_service_uuid =
      options_dart.discover_fast_advertisements
          ? "0000FE2C-0000-1000-8000-00805F9B34FB"
          : "";
  options.allowed.wifi_lan = options_dart.enable_wifi_lan;
  options.allowed.web_rtc = options_dart.enable_web_rtc;

  ConnectionRequestInfo info;
  info.endpoint_info = ByteArray(info_dart.endpoint_info);
  info.listener.initiated_cb =
      [info_dart](const std::string &endpoint_id,
                  const ConnectionResponseInfo &connection_info) {
        NEARBY_LOG(INFO, "Advertising initiated: id=%s", endpoint_id.c_str());
        Dart_CObject dart_object_initiated;
        dart_object_initiated.type = Dart_CObject_kString;
        dart_object_initiated.value.as_string =
            (char *)connection_info.remote_endpoint_info.data();
        const bool result =
            Dart_PostCObject_DL(info_dart.initiated_cb, &dart_object_initiated);
        if (!result) {
          NEARBY_LOG(INFO, "Posting message to port failed.");
        }
      };
  info.listener.accepted_cb = [info_dart](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Advertising accepted: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_accepted;
    dart_object_accepted.type = Dart_CObject_kString;
    dart_object_accepted.value.as_string = (char *)endpoint_id.c_str();
    const bool result =
        Dart_PostCObject_DL(info_dart.accepted_cb, &dart_object_accepted);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  info.listener.rejected_cb = [info_dart](const std::string &endpoint_id,
                                          Status status) {
    NEARBY_LOG(INFO, "Advertising rejected: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_rejected;
    dart_object_rejected.type = Dart_CObject_kString;
    dart_object_rejected.value.as_string = (char *)endpoint_id.c_str();
    const bool result =
        Dart_PostCObject_DL(info_dart.rejected_cb, &dart_object_rejected);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  info.listener.disconnected_cb = [info_dart](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Advertising disconnected: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_disconnected;
    dart_object_disconnected.type = Dart_CObject_kString;
    dart_object_disconnected.value.as_string = (char *)endpoint_id.c_str();
    const bool result = Dart_PostCObject_DL(info_dart.disconnected_cb,
                                            &dart_object_disconnected);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  info.listener.bandwidth_changed_cb =
      [info_dart](const std::string &endpoint_id, Medium medium) {
        NEARBY_LOG(INFO, "Advertising bandwidth changed: id=%s",
                   endpoint_id.c_str());
        Dart_CObject dart_object_bandwidth_changed;

        dart_object_bandwidth_changed.type = Dart_CObject_kString;
        dart_object_bandwidth_changed.value.as_string =
            (char *)endpoint_id.c_str();
        const bool result = Dart_PostCObject_DL(info_dart.bandwidth_changed_cb,
                                                &dart_object_bandwidth_changed);
        if (!result) {
          NEARBY_LOG(INFO, "Posting message to port failed.");
        }
      };

  ResultCallback callback;
  SetResultCallback(callback, result_cb);
  StartAdvertising(pCore, service_id, std::move(options), info, callback);
}

void StopAdvertisingDart(Core *pCore, Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  ResultCallback callback;
  SetResultCallback(callback, result_cb);
  StopAdvertising(pCore, callback);
}

void StartDiscoveryDart(Core *pCore, const char *service_id,
                        ConnectionOptionsDart options_dart,
                        DiscoveryListenerDart listener_dart,
                        Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  DiscoveryOptions options;
  options.strategy = GetStrategy(options_dart.strategy);
  options.allowed.bluetooth = options_dart.enable_bluetooth;
  options.allowed.ble = options_dart.enable_ble;
  options.allowed.wifi_lan = options_dart.enable_wifi_lan;
  options.allowed.web_rtc = false;
  options.low_power = options_dart.use_low_power_mode;
  options.enable_bluetooth_listening = options_dart.enable_bluetooth;
  options.enforce_topology_constraints = true;
  options.fast_advertisement_service_uuid =
      options_dart.discover_fast_advertisements
          ? "0000FE2C-0000-1000-8000-00805F9B34FB"
          : "";

  DiscoveryListener listener;
  listener.endpoint_found_cb = [listener_dart](
                                   const std::string &endpoint_id,
                                   const ByteArray &endpoint_info,
                                   const std::string &str_service_id) {
    NEARBY_LOG(INFO, "Device discovered: id=%s", endpoint_id.c_str());
    NEARBY_LOG(INFO, "Device discovered: service_id=%s",
               str_service_id.c_str());
    NEARBY_LOG(INFO, "Device discovered: info=%s",
               ((string)endpoint_info).c_str());

    Dart_CObject dart_object_endpoint_id;
    dart_object_endpoint_id.type = Dart_CObject_kString;
    dart_object_endpoint_id.value.as_string = (char *)endpoint_id.data();

    Dart_CObject dart_object_endpoint_info;
    dart_object_endpoint_info.type = Dart_CObject_kString;
    dart_object_endpoint_info.value.as_string = (char *)endpoint_info.data();

    Dart_CObject *elements[2];
    elements[0] = &dart_object_endpoint_id;
    elements[1] = &dart_object_endpoint_info;

    Dart_CObject dart_object_found;
    dart_object_found.type = Dart_CObject_kArray;
    dart_object_found.value.as_array.length = 2;
    dart_object_found.value.as_array.values = elements;
    const bool result =
        Dart_PostCObject_DL(listener_dart.found_cb, &dart_object_found);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  listener.endpoint_lost_cb = [listener_dart](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Device lost: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_lost;
    dart_object_lost.type = Dart_CObject_kString;
    dart_object_lost.value.as_string = (char *)endpoint_id.c_str();
    const bool result =
        Dart_PostCObject_DL(listener_dart.lost_cb, &dart_object_lost);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  listener.endpoint_distance_changed_cb =
      [listener_dart](const std::string &endpoint_id, DistanceInfo info) {
        NEARBY_LOG(INFO, "Device distance changed: id=%s", endpoint_id.c_str());
        Dart_CObject dart_object_distance_changed;
        dart_object_distance_changed.type = Dart_CObject_kString;
        dart_object_distance_changed.value.as_string =
            (char *)(endpoint_id.c_str());
        const bool result = Dart_PostCObject_DL(
            listener_dart.distance_changed_cb, &dart_object_distance_changed);
        if (!result) {
          NEARBY_LOG(INFO, "Posting message to port failed.");
        }
      };

  ResultCallback callback;
  SetResultCallback(callback, result_cb);
  StartDiscovery(pCore, service_id, std::move(options), listener, callback);
}

void StopDiscoveryDart(Core *pCore, Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  ResultCallback callback;
  SetResultCallback(callback, result_cb);
  StopDiscovery(pCore, callback);
}

void RequestConnectionDart(Core *pCore, const char *endpoint_id,
                           ConnectionOptionsDart options_dart,
                           ConnectionRequestInfoDart info_dart,
                           Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  ConnectionOptions options;
  options.enforce_topology_constraints = false;
  options.allowed.bluetooth = options_dart.enable_bluetooth;
  options.allowed.ble = options_dart.enable_ble;
  options.allowed.wifi_lan = options_dart.enable_wifi_lan;
  options.allowed.web_rtc = options_dart.enable_web_rtc;

  ConnectionRequestInfo info;
  info.endpoint_info = ByteArray(info_dart.endpoint_info);
  info.listener.initiated_cb =
      [info_dart](const std::string &endpoint_id,
                  const ConnectionResponseInfo &connection_info) {
        NEARBY_LOG(INFO, "Advertising initiated: id=%s", endpoint_id.c_str());
        Dart_CObject dart_object_initiated;
        dart_object_initiated.type = Dart_CObject_kString;
        dart_object_initiated.value.as_string =
            (char *)connection_info.remote_endpoint_info.data();
        const bool result =
            Dart_PostCObject_DL(info_dart.initiated_cb, &dart_object_initiated);
        if (!result) {
          NEARBY_LOG(INFO, "Posting message to port failed.");
        }
      };
  info.listener.accepted_cb = [info_dart](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Advertising accepted: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_accepted;
    dart_object_accepted.type = Dart_CObject_kString;
    dart_object_accepted.value.as_string = (char *)endpoint_id.c_str();
    const bool result =
        Dart_PostCObject_DL(info_dart.accepted_cb, &dart_object_accepted);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  info.listener.rejected_cb = [info_dart](const std::string &endpoint_id,
                                          Status status) {
    NEARBY_LOG(INFO, "Advertising rejected: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_rejected;
    dart_object_rejected.type = Dart_CObject_kString;
    dart_object_rejected.value.as_string = (char *)endpoint_id.c_str();
    const bool result =
        Dart_PostCObject_DL(info_dart.rejected_cb, &dart_object_rejected);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  info.listener.disconnected_cb = [info_dart](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Advertising disconnected: id=%s", endpoint_id.c_str());
    Dart_CObject dart_object_disconnected;
    dart_object_disconnected.type = Dart_CObject_kString;
    dart_object_disconnected.value.as_string = (char *)endpoint_id.c_str();
    const bool result = Dart_PostCObject_DL(info_dart.disconnected_cb,
                                            &dart_object_disconnected);
    if (!result) {
      NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  info.listener.bandwidth_changed_cb =
      [info_dart](const std::string &endpoint_id, Medium medium) {
        NEARBY_LOG(INFO, "Advertising bandwidth changed: id=%s",
                   endpoint_id.c_str());
        Dart_CObject dart_object_bandwidth_changed;

        dart_object_bandwidth_changed.type = Dart_CObject_kString;
        dart_object_bandwidth_changed.value.as_string =
            (char *)endpoint_id.c_str();
        const bool result = Dart_PostCObject_DL(info_dart.bandwidth_changed_cb,
                                                &dart_object_bandwidth_changed);
        if (!result) {
          NEARBY_LOG(INFO, "Posting message to port failed.");
        }
      };

  ResultCallback callback;
  SetResultCallback(callback, result_cb);
  RequestConnection(pCore, endpoint_id, info, std::move(options), callback);
}

void AcceptConnectionDart(Core *pCore,
                          const char *endpoint_id,
                          PayloadListenerDart listener_dart,
                          Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  PayloadListener listener;
  listener.payload_cb = [listener_dart](const std::string &endpoint_id,
    Payload payload) {
      NEARBY_LOG(INFO, "Payload callback called. id: %s, "
                 "payload_id: %d, type: %d, offset: %d",
                 endpoint_id.c_str(),
                 payload.GetId(),
                 payload.GetType(),
                 payload.GetOffset());

      Dart_CObject dart_object_endpoint_id;
      dart_object_endpoint_id.type = Dart_CObject_kString;
      dart_object_endpoint_id.value.as_string =
          (char *)endpoint_id.data();

      Dart_CObject dart_object_payload_id;
      dart_object_payload_id.type = Dart_CObject_kInt64;
      dart_object_payload_id.value.as_int64 = payload.GetId();

      Dart_CObject dart_object_payload_type;
      dart_object_payload_type.type = Dart_CObject_kInt64;
      dart_object_payload_type.value.as_int64 = (int)payload.GetType();

      Dart_CObject dart_object_offset;
      dart_object_offset.type = Dart_CObject_kInt64;
      dart_object_offset.value.as_int64 = payload.GetOffset();

      Dart_CObject* elements[4];
      elements[0] = &dart_object_endpoint_id;
      elements[1] = &dart_object_payload_id;
      elements[2] = &dart_object_payload_type;
      elements[3] = &dart_object_offset;

      Dart_CObject dart_object_payload;
      dart_object_payload.type = Dart_CObject_kArray;
      dart_object_payload.value.as_array.length = 4;
      dart_object_payload.value.as_array.values = elements;

      const bool result = Dart_PostCObject_DL(listener_dart.payload_cb,
                            &dart_object_payload);
      if (!result) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
    }
  };
  listener.payload_progress_cb = [listener_dart](
    const std::string &endpoint_id,
    const PayloadProgressInfo& info) {
      NEARBY_LOG(INFO, "Payload progress callback called. id: %s, "
               "payload_id: %d, bytes transferred: %d, total: %d, status: %d",
               endpoint_id.c_str(),
               info.payload_id,
               info.bytes_transferred,
               info.total_bytes,
               info.status);
      Dart_CObject dart_object_endpoint_id;
      dart_object_endpoint_id.type = Dart_CObject_kString;
      dart_object_endpoint_id.value.as_string =
          (char *)endpoint_id.data();

      Dart_CObject dart_object_payload_id;
      dart_object_payload_id.type = Dart_CObject_kInt64;
      dart_object_payload_id.value.as_int64 = info.payload_id;

      Dart_CObject dart_object_bytes_transferred;
      dart_object_bytes_transferred.type = Dart_CObject_kInt64;
      dart_object_bytes_transferred.value.as_int64 = info.bytes_transferred;

      Dart_CObject dart_object_total_bytes;
      dart_object_total_bytes.type = Dart_CObject_kInt64;
      dart_object_total_bytes.value.as_int64 = info.total_bytes;

      Dart_CObject dart_object_status;
      dart_object_status.type = Dart_CObject_kInt64;
      dart_object_status.value.as_int64 = (int64_t)info.status;

      Dart_CObject* elements[5];
      elements[0] = &dart_object_endpoint_id;
      elements[1] = &dart_object_payload_id;
      elements[2] = &dart_object_bytes_transferred;
      elements[3] = &dart_object_total_bytes;
      elements[4] = &dart_object_status;

      Dart_CObject dart_object_payload_progress;
      dart_object_payload_progress.type = Dart_CObject_kArray;
      dart_object_payload_progress.value.as_array.length = 5;
      dart_object_payload_progress.value.as_array.values = elements;

      const bool result =
          Dart_PostCObject_DL(listener_dart.payload_progress_cb,
                              &dart_object_payload_progress);
      if (!result) {
        NEARBY_LOG(INFO,
                   "Posting message to port failed.");
      }
    };

  ResultCallback callback;
  SetResultCallback(callback, result_cb);
  AcceptConnection(pCore, endpoint_id, listener, callback);
}

void SendPayloadDart(Core *pCore, const char *endpoint_id,
                     PayloadDart payload_dart, Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  ResultCallback callback;
  std::vector<string> endpoint_ids = {string(endpoint_id)};

  Payload payload(Payload::GenerateId(), ByteArray(payload_dart.data));
  SetResultCallback(callback, result_cb);
  SendPayload(pCore, absl::Span<const std::string>(endpoint_ids),
              std::move(payload), callback);
}
}  // namespace windows
}  // namespace connections
}  // namespace nearby
}  // namespace location
