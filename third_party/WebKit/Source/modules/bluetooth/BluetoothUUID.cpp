// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothUUID.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "platform/UUID.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HexNumber.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

typedef WTF::HashMap<String, unsigned> NameToAssignedNumberMap;

enum class GATTAttribute { kService, kCharacteristic, kDescriptor };

NameToAssignedNumberMap* GetAssignedNumberToServiceNameMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(NameToAssignedNumberMap, services_map, []() {
    // https://www.bluetooth.com/specifications/gatt/services
    NameToAssignedNumberMap* services = new NameToAssignedNumberMap();
    services->insert("generic_access", 0x1800);
    services->insert("generic_attribute", 0x1801);
    services->insert("immediate_alert", 0x1802);
    services->insert("link_loss", 0x1803);
    services->insert("tx_power", 0x1804);
    services->insert("current_time", 0x1805);
    services->insert("reference_time_update", 0x1806);
    services->insert("next_dst_change", 0x1807);
    services->insert("glucose", 0x1808);
    services->insert("health_thermometer", 0x1809);
    services->insert("device_information", 0x180A);
    services->insert("heart_rate", 0x180D);
    services->insert("phone_alert_status", 0x180E);
    services->insert("battery_service", 0x180F);
    services->insert("blood_pressure", 0x1810);
    services->insert("alert_notification", 0x1811);
    services->insert("human_interface_device", 0x1812);
    services->insert("scan_parameters", 0x1813);
    services->insert("running_speed_and_cadence", 0x1814);
    services->insert("automation_io", 0x1815);
    services->insert("cycling_speed_and_cadence", 0x1816);
    services->insert("cycling_power", 0x1818);
    services->insert("location_and_navigation", 0x1819);
    services->insert("environmental_sensing", 0x181A);
    services->insert("body_composition", 0x181B);
    services->insert("user_data", 0x181C);
    services->insert("weight_scale", 0x181D);
    services->insert("bond_management", 0x181E);
    services->insert("continuous_glucose_monitoring", 0x181F);
    services->insert("fitness_machine", 0x1826);
    services->insert("internet_protocol_support", 0x1820);
    services->insert("indoor_positioning", 0x1821);
    services->insert("pulse_oximeter", 0x1822);
    services->insert("http_proxy", 0x1823);
    services->insert("transport_discovery", 0x1824);
    services->insert("object_transfer", 0x1825);
    return services;
  }());

  return &services_map;
}

NameToAssignedNumberMap* GetAssignedNumberForCharacteristicNameMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      NameToAssignedNumberMap, characteristics_map, []() {
        // https://www.bluetooth.com/specifications/gatt/characteristics
        NameToAssignedNumberMap* characteristics =
            new NameToAssignedNumberMap();
        characteristics->insert("gap.device_name", 0x2A00);
        characteristics->insert("gap.appearance", 0x2A01);
        characteristics->insert("gap.peripheral_privacy_flag", 0x2A02);
        characteristics->insert("gap.reconnection_address", 0x2A03);
        characteristics->insert(
            "gap.peripheral_preferred_connection_parameters", 0x2A04);
        characteristics->insert("gatt.service_changed", 0x2A05);
        characteristics->insert("alert_level", 0x2A06);
        characteristics->insert("tx_power_level", 0x2A07);
        characteristics->insert("date_time", 0x2A08);
        characteristics->insert("day_of_week", 0x2A09);
        characteristics->insert("day_date_time", 0x2A0A);
        characteristics->insert("exact_time_256", 0x2A0C);
        characteristics->insert("dst_offset", 0x2A0D);
        characteristics->insert("time_zone", 0x2A0E);
        characteristics->insert("local_time_information", 0x2A0F);
        characteristics->insert("time_with_dst", 0x2A11);
        characteristics->insert("time_accuracy", 0x2A12);
        characteristics->insert("time_source", 0x2A13);
        characteristics->insert("reference_time_information", 0x2A14);
        characteristics->insert("time_update_control_point", 0x2A16);
        characteristics->insert("time_update_state", 0x2A17);
        characteristics->insert("glucose_measurement", 0x2A18);
        characteristics->insert("battery_level", 0x2A19);
        characteristics->insert("temperature_measurement", 0x2A1C);
        characteristics->insert("temperature_type", 0x2A1D);
        characteristics->insert("intermediate_temperature", 0x2A1E);
        characteristics->insert("measurement_interval", 0x2A21);
        characteristics->insert("boot_keyboard_input_report", 0x2A22);
        characteristics->insert("system_id", 0x2A23);
        characteristics->insert("model_number_string", 0x2A24);
        characteristics->insert("serial_number_string", 0x2A25);
        characteristics->insert("firmware_revision_string", 0x2A26);
        characteristics->insert("hardware_revision_string", 0x2A27);
        characteristics->insert("software_revision_string", 0x2A28);
        characteristics->insert("manufacturer_name_string", 0x2A29);
        characteristics->insert(
            "ieee_11073-20601_regulatory_certification_data_list", 0x2A2A);
        characteristics->insert("current_time", 0x2A2B);
        characteristics->insert("magnetic_declination", 0x2A2C);
        characteristics->insert("scan_refresh", 0x2A31);
        characteristics->insert("boot_keyboard_output_report", 0x2A32);
        characteristics->insert("boot_mouse_input_report", 0x2A33);
        characteristics->insert("glucose_measurement_context", 0x2A34);
        characteristics->insert("blood_pressure_measurement", 0x2A35);
        characteristics->insert("intermediate_cuff_pressure", 0x2A36);
        characteristics->insert("heart_rate_measurement", 0x2A37);
        characteristics->insert("body_sensor_location", 0x2A38);
        characteristics->insert("heart_rate_control_point", 0x2A39);
        characteristics->insert("alert_status", 0x2A3F);
        characteristics->insert("ringer_control_point", 0x2A40);
        characteristics->insert("ringer_setting", 0x2A41);
        characteristics->insert("alert_category_id_bit_mask", 0x2A42);
        characteristics->insert("alert_category_id", 0x2A43);
        characteristics->insert("alert_notification_control_point", 0x2A44);
        characteristics->insert("unread_alert_status", 0x2A45);
        characteristics->insert("new_alert", 0x2A46);
        characteristics->insert("supported_new_alert_category", 0x2A47);
        characteristics->insert("supported_unread_alert_category", 0x2A48);
        characteristics->insert("blood_pressure_feature", 0x2A49);
        characteristics->insert("hid_information", 0x2A4A);
        characteristics->insert("report_map", 0x2A4B);
        characteristics->insert("hid_control_point", 0x2A4C);
        characteristics->insert("report", 0x2A4D);
        characteristics->insert("protocol_mode", 0x2A4E);
        characteristics->insert("scan_interval_window", 0x2A4F);
        characteristics->insert("pnp_id", 0x2A50);
        characteristics->insert("glucose_feature", 0x2A51);
        characteristics->insert("record_access_control_point", 0x2A52);
        characteristics->insert("rsc_measurement", 0x2A53);
        characteristics->insert("rsc_feature", 0x2A54);
        characteristics->insert("sc_control_point", 0x2A55);
        characteristics->insert("digital", 0x2A56);
        characteristics->insert("analog", 0x2A58);
        characteristics->insert("aggregate", 0x2A5A);
        characteristics->insert("csc_measurement", 0x2A5B);
        characteristics->insert("csc_feature", 0x2A5C);
        characteristics->insert("sensor_location", 0x2A5D);
        characteristics->insert("plx_spot_check_measurement", 0x2A5E);
        characteristics->insert("plx_continuous_measurement", 0x2A5F);
        characteristics->insert("plx_features", 0x2A60);
        characteristics->insert("cycling_power_measurement", 0x2A63);
        characteristics->insert("cycling_power_vector", 0x2A64);
        characteristics->insert("cycling_power_feature", 0x2A65);
        characteristics->insert("cycling_power_control_point", 0x2A66);
        characteristics->insert("location_and_speed", 0x2A67);
        characteristics->insert("navigation", 0x2A68);
        characteristics->insert("position_quality", 0x2A69);
        characteristics->insert("ln_feature", 0x2A6A);
        characteristics->insert("ln_control_point", 0x2A6B);
        characteristics->insert("elevation", 0x2A6C);
        characteristics->insert("pressure", 0x2A6D);
        characteristics->insert("temperature", 0x2A6E);
        characteristics->insert("humidity", 0x2A6F);
        characteristics->insert("true_wind_speed", 0x2A70);
        characteristics->insert("true_wind_direction", 0x2A71);
        characteristics->insert("apparent_wind_speed", 0x2A72);
        characteristics->insert("apparent_wind_direction", 0x2A73);
        characteristics->insert("gust_factor", 0x2A74);
        characteristics->insert("pollen_concentration", 0x2A75);
        characteristics->insert("uv_index", 0x2A76);
        characteristics->insert("irradiance", 0x2A77);
        characteristics->insert("rainfall", 0x2A78);
        characteristics->insert("wind_chill", 0x2A79);
        characteristics->insert("heat_index", 0x2A7A);
        characteristics->insert("dew_point", 0x2A7B);
        characteristics->insert("descriptor_value_changed", 0x2A7D);
        characteristics->insert("aerobic_heart_rate_lower_limit", 0x2A7E);
        characteristics->insert("aerobic_threshold", 0x2A7F);
        characteristics->insert("age", 0x2A80);
        characteristics->insert("anaerobic_heart_rate_lower_limit", 0x2A81);
        characteristics->insert("anaerobic_heart_rate_upper_limit", 0x2A82);
        characteristics->insert("anaerobic_threshold", 0x2A83);
        characteristics->insert("aerobic_heart_rate_upper_limit", 0x2A84);
        characteristics->insert("date_of_birth", 0x2A85);
        characteristics->insert("date_of_threshold_assessment", 0x2A86);
        characteristics->insert("email_address", 0x2A87);
        characteristics->insert("fat_burn_heart_rate_lower_limit", 0x2A88);
        characteristics->insert("fat_burn_heart_rate_upper_limit", 0x2A89);
        characteristics->insert("first_name", 0x2A8A);
        characteristics->insert("five_zone_heart_rate_limits", 0x2A8B);
        characteristics->insert("gender", 0x2A8C);
        characteristics->insert("heart_rate_max", 0x2A8D);
        characteristics->insert("height", 0x2A8E);
        characteristics->insert("hip_circumference", 0x2A8F);
        characteristics->insert("last_name", 0x2A90);
        characteristics->insert("maximum_recommended_heart_rate", 0x2A91);
        characteristics->insert("resting_heart_rate", 0x2A92);
        characteristics->insert(
            "sport_type_for_aerobic_and_anaerobic_thresholds", 0x2A93);
        characteristics->insert("three_zone_heart_rate_limits", 0x2A94);
        characteristics->insert("two_zone_heart_rate_limit", 0x2A95);
        characteristics->insert("vo2_max", 0x2A96);
        characteristics->insert("waist_circumference", 0x2A97);
        characteristics->insert("weight", 0x2A98);
        characteristics->insert("database_change_increment", 0x2A99);
        characteristics->insert("user_index", 0x2A9A);
        characteristics->insert("body_composition_feature", 0x2A9B);
        characteristics->insert("body_composition_measurement", 0x2A9C);
        characteristics->insert("weight_measurement", 0x2A9D);
        characteristics->insert("weight_scale_feature", 0x2A9E);
        characteristics->insert("user_control_point", 0x2A9F);
        characteristics->insert("magnetic_flux_density_2D", 0x2AA0);
        characteristics->insert("magnetic_flux_density_3D", 0x2AA1);
        characteristics->insert("language", 0x2AA2);
        characteristics->insert("barometric_pressure_trend", 0x2AA3);
        characteristics->insert("bond_management_control_point", 0x2AA4);
        characteristics->insert("bond_management_feature", 0x2AA5);
        characteristics->insert("gap.central_address_resolution_support",
                                0x2AA6);
        characteristics->insert("cgm_measurement", 0x2AA7);
        characteristics->insert("cgm_feature", 0x2AA8);
        characteristics->insert("cgm_status", 0x2AA9);
        characteristics->insert("cgm_session_start_time", 0x2AAA);
        characteristics->insert("cgm_session_run_time", 0x2AAB);
        characteristics->insert("cgm_specific_ops_control_point", 0x2AAC);
        characteristics->insert("indoor_positioning_configuration", 0x2AAD);
        characteristics->insert("latitude", 0x2AAE);
        characteristics->insert("longitude", 0x2AAF);
        characteristics->insert("local_north_coordinate", 0x2AB0);
        characteristics->insert("local_east_coordinate.xml", 0x2AB1);
        characteristics->insert("floor_number", 0x2AB2);
        characteristics->insert("altitude", 0x2AB3);
        characteristics->insert("uncertainty", 0x2AB4);
        characteristics->insert("location_name", 0x2AB5);
        characteristics->insert("uri", 0x2AB6);
        characteristics->insert("http_headers", 0x2AB7);
        characteristics->insert("http_status_code", 0x2AB8);
        characteristics->insert("http_entity_body", 0x2AB9);
        characteristics->insert("http_control_point", 0x2ABA);
        characteristics->insert("https_security", 0x2ABB);
        characteristics->insert("tds_control_point", 0x2ABC);
        characteristics->insert("ots_feature", 0x2ABD);
        characteristics->insert("object_name", 0x2ABE);
        characteristics->insert("object_type", 0x2ABF);
        characteristics->insert("object_size", 0x2AC0);
        characteristics->insert("object_first_created", 0x2AC1);
        characteristics->insert("object_last_modified", 0x2AC2);
        characteristics->insert("object_id", 0x2AC3);
        characteristics->insert("object_properties", 0x2AC4);
        characteristics->insert("object_action_control_point", 0x2AC5);
        characteristics->insert("object_list_control_point", 0x2AC6);
        characteristics->insert("object_list_filter", 0x2AC7);
        characteristics->insert("object_changed", 0x2AC8);
        characteristics->insert("resolvable_private_address_only", 0x2AC9);
        characteristics->insert("fitness_machine_feature", 0x2ACC);
        characteristics->insert("treadmill_data", 0x2ACD);
        characteristics->insert("cross_trainer_data", 0x2ACE);
        characteristics->insert("step_climber_data", 0x2ACF);
        characteristics->insert("stair_climber_data", 0x2AD0);
        characteristics->insert("rower_data", 0x2AD1);
        characteristics->insert("indoor_bike_data", 0x2AD2);
        characteristics->insert("training_status", 0x2AD3);
        characteristics->insert("supported_speed_range", 0x2AD4);
        characteristics->insert("supported_inclination_range", 0x2AD5);
        characteristics->insert("supported_resistance_level_range", 0x2AD6);
        characteristics->insert("supported_heart_rate_range", 0x2AD7);
        characteristics->insert("supported_power_range", 0x2AD8);
        characteristics->insert("fitness_machine_control_point", 0x2AD9);
        characteristics->insert("fitness_machine_status", 0x2ADA);
        return characteristics;
      }());

  return &characteristics_map;
}

NameToAssignedNumberMap* GetAssignedNumberForDescriptorNameMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      NameToAssignedNumberMap, descriptors_map, []() {
        // https://www.bluetooth.com/specifications/gatt/descriptors
        NameToAssignedNumberMap* descriptors = new NameToAssignedNumberMap();
        descriptors->insert("gatt.characteristic_extended_properties", 0x2900);
        descriptors->insert("gatt.characteristic_user_description", 0x2901);
        descriptors->insert("gatt.client_characteristic_configuration", 0x2902);
        descriptors->insert("gatt.server_characteristic_configuration", 0x2903);
        descriptors->insert("gatt.characteristic_presentation_format", 0x2904);
        descriptors->insert("gatt.characteristic_aggregate_format", 0x2905);
        descriptors->insert("valid_range", 0x2906);
        descriptors->insert("external_report_reference", 0x2907);
        descriptors->insert("report_reference", 0x2908);
        descriptors->insert("number_of_digitals", 0x2909);
        descriptors->insert("value_trigger_setting", 0x290A);
        descriptors->insert("es_configuration", 0x290B);
        descriptors->insert("es_measurement", 0x290C);
        descriptors->insert("es_trigger_setting", 0x290D);
        descriptors->insert("time_trigger_setting", 0x290E);
        return descriptors;
      }());

  return &descriptors_map;
}

String GetUUIDForGATTAttribute(GATTAttribute attribute,
                               StringOrUnsignedLong name,
                               ExceptionState& exception_state) {
  // Implementation of BluetoothUUID.getService, BluetoothUUID.getCharacteristic
  // and BluetoothUUID.getDescriptor algorithms:
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothuuid-getservice
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothuuid-getcharacteristic
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothuuid-getdescriptor

  // If name is an unsigned long, return BluetoothUUID.cannonicalUUI(name) and
  // abort this steps.
  if (name.isUnsignedLong())
    return BluetoothUUID::canonicalUUID(name.getAsUnsignedLong());

  String name_str = name.getAsString();

  // If name is a valid UUID, return name and abort these steps.
  if (IsValidUUID(name_str))
    return name_str;

  // If name is in the corresponding attribute map return
  // BluetoothUUID.cannonicalUUID(alias).
  NameToAssignedNumberMap* map = nullptr;
  const char* attribute_type = nullptr;
  switch (attribute) {
    case GATTAttribute::kService:
      map = GetAssignedNumberToServiceNameMap();
      attribute_type = "Service";
      break;
    case GATTAttribute::kCharacteristic:
      map = GetAssignedNumberForCharacteristicNameMap();
      attribute_type = "Characteristic";
      break;
    case GATTAttribute::kDescriptor:
      map = GetAssignedNumberForDescriptorNameMap();
      attribute_type = "Descriptor";
      break;
  }

  if (map->Contains(name_str))
    return BluetoothUUID::canonicalUUID(map->at(name_str));

  StringBuilder error_message;
  error_message.Append("Invalid ");
  error_message.Append(attribute_type);
  error_message.Append(" name: '");
  error_message.Append(name_str);
  error_message.Append(
      "'. It must be a valid UUID alias (e.g. 0x1234), "
      "UUID (lowercase hex characters e.g. "
      "'00001234-0000-1000-8000-00805f9b34fb'), "
      "or recognized standard name from ");
  switch (attribute) {
    case GATTAttribute::kService:
      error_message.Append(
          "https://www.bluetooth.com/specifications/gatt/services"
          " e.g. 'alert_notification'.");
      break;
    case GATTAttribute::kCharacteristic:
      error_message.Append(
          "https://www.bluetooth.com/specifications/gatt/characteristics"
          " e.g. 'aerobic_heart_rate_lower_limit'.");
      break;
    case GATTAttribute::kDescriptor:
      error_message.Append(
          "https://www.bluetooth.com/specifications/gatt/descriptors"
          " e.g. 'gatt.characteristic_presentation_format'.");
      break;
  }
  // Otherwise, throw a TypeError.
  exception_state.ThrowDOMException(kV8TypeError, error_message.ToString());
  return String();
}

}  // namespace

// static
String BluetoothUUID::getService(StringOrUnsignedLong name,
                                 ExceptionState& exception_state) {
  return GetUUIDForGATTAttribute(GATTAttribute::kService, name,
                                 exception_state);
}

// static
String BluetoothUUID::getCharacteristic(StringOrUnsignedLong name,
                                        ExceptionState& exception_state) {
  return GetUUIDForGATTAttribute(GATTAttribute::kCharacteristic, name,
                                 exception_state);
}

// static
String BluetoothUUID::getDescriptor(StringOrUnsignedLong name,
                                    ExceptionState& exception_state) {
  return GetUUIDForGATTAttribute(GATTAttribute::kDescriptor, name,
                                 exception_state);
}

// static
String BluetoothUUID::canonicalUUID(unsigned alias) {
  StringBuilder builder;
  builder.ReserveCapacity(36 /* 36 chars or 128 bits, length of a UUID */);
  HexNumber::AppendUnsignedAsHexFixedSize(
      alias, builder, 8 /* 8 chars or 32 bits, prefix length */,
      HexNumber::kLowercase);

  builder.Append("-0000-1000-8000-00805f9b34fb");
  return builder.ToString();
}

}  // namespace blink
