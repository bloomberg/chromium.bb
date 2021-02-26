// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FIELD_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FIELD_DATA_MANAGER_H_

#include <map>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/renderer_id.h"

namespace autofill {

// This class provides the methods to update and get the field data (the pair
// of user typed value and field properties mask).
class FieldDataManager : public base::RefCounted<FieldDataManager> {
 public:
  using FieldDataMap =
      std::map<FieldRendererId,
               std::pair<base::Optional<base::string16>, FieldPropertiesMask>>;

  FieldDataManager();

  void ClearData();
  bool HasFieldData(FieldRendererId id) const;

  // Updates the field value associated with the key |element| in
  // |field_value_and_properties_map_|.
  // Flags in |mask| are added with bitwise OR operation.
  // If |value| is empty, kUserTyped and kAutofilled should be cleared.
  void UpdateFieldDataMap(FieldRendererId id,
                          const base::string16& value,
                          FieldPropertiesMask mask);
  // Only update FieldPropertiesMask when value is null.
  void UpdateFieldDataMapWithNullValue(FieldRendererId id,
                                       FieldPropertiesMask mask);

  base::string16 GetUserTypedValue(FieldRendererId id) const;
  FieldPropertiesMask GetFieldPropertiesMask(FieldRendererId id) const;

  // Check if the string |value| is saved in |field_value_and_properties_map_|.
  bool FindMachedValue(const base::string16& value) const;

  bool DidUserType(FieldRendererId id) const;

  bool WasAutofilledOnUserTrigger(FieldRendererId id) const;

  const FieldDataMap& field_data_map() const {
    return field_value_and_properties_map_;
  }

  bool WasAutofilledOnPageLoad(FieldRendererId id) const;

  // Update data with autofilled value.
  void UpdateFieldDataWithAutofilledValue(FieldRendererId id,
                                          const base::string16& value,
                                          FieldPropertiesMask mask);

  base::Optional<base::string16> GetAutofilledValue(FieldRendererId id) const;

 private:
  friend class base::RefCounted<FieldDataManager>;

  ~FieldDataManager();

  FieldDataMap field_value_and_properties_map_;

  // Stores values autofilled either on page load or on user trigger.
  std::map<FieldRendererId, base::string16> autofilled_values_map_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FIELD_DATA_MANAGER_H_
