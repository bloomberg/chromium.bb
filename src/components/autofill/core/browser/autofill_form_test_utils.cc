// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_form_test_utils.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

namespace test {

testing::Message DescribeFormData(const FormData& form_data) {
  testing::Message result;
  result << "Form contains " << form_data.fields.size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields) {
    result << "type=" << field.form_control_type << ", name=" << field.name
           << ", label=" << field.label << "\n";
  }
  return result;
}

FormFieldData CreateFieldByRole(ServerFieldType role) {
  FormFieldData field;
  switch (role) {
    case ServerFieldType::USERNAME:
      field.label = u"Username";
      field.name = u"username";
      break;
    case ServerFieldType::NAME_FULL:
      field.label = u"Full name";
      field.name = u"fullname";
      break;
    case ServerFieldType::NAME_FIRST:
      field.label = u"First Name";
      field.name = u"firstName";
      break;
    case ServerFieldType::NAME_LAST:
      field.label = u"Last Name";
      field.name = u"lastName";
      break;
    case ServerFieldType::EMAIL_ADDRESS:
      field.label = u"E-mail address";
      field.name = u"email";
      break;
    case ServerFieldType::ADDRESS_HOME_LINE1:
      field.label = u"Address";
      field.name = u"home_line_one";
      break;
    case ServerFieldType::ADDRESS_HOME_CITY:
      field.label = u"City";
      field.name = u"city";
      break;
    case ServerFieldType::ADDRESS_HOME_STATE:
      field.label = u"State";
      field.name = u"state";
      break;
    case ServerFieldType::ADDRESS_HOME_COUNTRY:
      field.label = u"Country";
      field.name = u"country";
      break;
    case ServerFieldType::ADDRESS_HOME_ZIP:
      field.label = u"Zip Code";
      field.name = u"zipCode";
      break;
    case ServerFieldType::PHONE_HOME_NUMBER:
      field.label = u"Phone";
      field.name = u"phone";
      break;
    case ServerFieldType::COMPANY_NAME:
      field.label = u"Company";
      field.name = u"company";
      break;
    case ServerFieldType::CREDIT_CARD_NUMBER:
      field.label = u"Card Number";
      field.name = u"cardNumber";
      break;
    case ServerFieldType::EMPTY_TYPE:
    default:
      break;
  }
  return field;
}

FormData GetFormData(const FormDataDescription& d) {
  FormData f;
  f.url = GURL(d.url);
  f.action = GURL(d.action);
  f.name = d.name;
  f.host_frame = d.host_frame.value_or(MakeLocalFrameToken());
  f.unique_renderer_id = d.unique_renderer_id.value_or(MakeFormRendererId());
  if (d.main_frame_origin)
    f.main_frame_origin = *d.main_frame_origin;
  f.is_form_tag = d.is_form_tag;
  for (const FieldDataDescription& dd : d.fields) {
    FormFieldData ff = CreateFieldByRole(dd.role);
    ff.form_control_type = dd.form_control_type;
    if (ff.form_control_type == "select-one" && !dd.select_options.empty())
      ff.options = dd.select_options;
    ff.host_frame = dd.host_frame.value_or(f.host_frame);
    ff.unique_renderer_id =
        dd.unique_renderer_id.value_or(MakeFieldRendererId());
    ff.is_focusable = dd.is_focusable;
    if (!dd.autocomplete_attribute.empty())
      ff.autocomplete_attribute = dd.autocomplete_attribute;
    if (dd.label)
      ff.label = *dd.label;
    if (dd.name)
      ff.name = *dd.name;
    if (dd.value)
      ff.value = *dd.value;
    ff.is_autofilled = dd.is_autofilled.value_or(false);
    ff.origin = dd.origin.value_or(f.main_frame_origin);
    ff.should_autocomplete = dd.should_autocomplete;
    f.fields.push_back(ff);
  }
  return f;
}

// static
void FormStructureTest::CheckFormStructureTestData(
    const std::vector<FormStructureTestCase>& test_cases) {
  for (const FormStructureTestCase& test_case : test_cases) {
    const FormData form = GetFormData(test_case.form_attributes);
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.form_attributes.description_for_logging);

    auto form_structure = std::make_unique<FormStructure>(form);

    if (test_case.form_flags.determine_heuristic_type)
      form_structure->DetermineHeuristicTypes(nullptr, nullptr);

    if (test_case.form_flags.is_autofillable)
      EXPECT_TRUE(form_structure->IsAutofillable());
    if (test_case.form_flags.should_be_parsed)
      EXPECT_TRUE(form_structure->ShouldBeParsed());
    if (test_case.form_flags.should_be_queried)
      EXPECT_TRUE(form_structure->ShouldBeQueried());
    if (test_case.form_flags.should_be_uploaded)
      EXPECT_TRUE(form_structure->ShouldBeUploaded());
    if (test_case.form_flags.has_author_specified_types)
      EXPECT_TRUE(form_structure->has_author_specified_types());
    if (test_case.form_flags.has_author_specified_upi_vpa_hint)
      EXPECT_TRUE(form_structure->has_author_specified_upi_vpa_hint());

    if (test_case.form_flags.is_complete_credit_card_form.has_value()) {
      EXPECT_EQ(form_structure->IsCompleteCreditCardForm(),
                *test_case.form_flags.is_complete_credit_card_form);
    }
    if (test_case.form_flags.field_count) {
      ASSERT_EQ(*test_case.form_flags.field_count,
                static_cast<int>(form_structure->field_count()));
    }
    if (test_case.form_flags.autofill_count) {
      ASSERT_EQ(*test_case.form_flags.autofill_count,
                static_cast<int>(form_structure->autofill_count()));
    }
    if (test_case.form_flags.section_count) {
      std::set<std::string> section_names;
      for (const auto& field : *form_structure)
        section_names.insert(field->section);
      EXPECT_EQ(*test_case.form_flags.section_count,
                static_cast<int>(section_names.size()));
    }

    for (size_t i = 0;
         i < test_case.expected_field_types.expected_html_type.size(); i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_html_type[i],
                form_structure->field(i)->html_type());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_phone_part.size(); i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_phone_part[i],
                form_structure->field(i)->phone_part());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_heuristic_type.size();
         i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_heuristic_type[i],
                form_structure->field(i)->heuristic_type());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_overall_type.size(); i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_overall_type[i],
                form_structure->field(i)->Type().GetStorableType());
    }
  }
}

}  // namespace test

}  // namespace autofill
