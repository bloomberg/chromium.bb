// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class AddressFieldTest : public FormFieldTest {
 public:
  AddressFieldTest() = default;
  AddressFieldTest(const AddressFieldTest&) = delete;
  AddressFieldTest& operator=(const AddressFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                   const LanguageCode& page_language) override {
    return AddressField::Parse(scanner, page_language, nullptr);
  }
};

TEST_F(AddressFieldTest, Empty) {
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_F(AddressFieldTest, NonParse) {
  AddTextFormFieldData("", "", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_F(AddressFieldTest, ParseOneLineAddress) {
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE1);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseTwoLineAddress) {
  AddTextFormFieldData("address", "Address", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("address2", "Address", ADDRESS_HOME_LINE2);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseThreeLineAddress) {
  AddTextFormFieldData("Address1", "Address Line 1", ADDRESS_HOME_LINE1);
  AddTextFormFieldData("Address1", "Address Line 2", ADDRESS_HOME_LINE2);
  AddTextFormFieldData("Address1", "Address Line 3", ADDRESS_HOME_LINE3);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseStreetAddressFromTextArea) {
  AddFormFieldData("textarea", "address", "Address",
                   ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify();
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME| and
// |ADDRESS_HOME_HOUSE_NUMBER| when they are labeled accordingly and
// both are present.
TEST_F(AddressFieldTest, ParseStreetNameAndHouseNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House number",
                       ADDRESS_HOME_HOUSE_NUMBER);
  ClassifyAndVerify();
}

// Tests that fields are classified as |ADDRESS_HOME_STREET_NAME|, and
// |ADDRESS_HOME_HOUSE_NUMBER| |ADDRESS_HOME_APT_NUM| when they are labeled
// accordingly and all are present.
TEST_F(AddressFieldTest, ParseStreetNameAndHouseNumberAndApartmentNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures(
      {features::kAutofillEnableSupportForMoreStructureInAddresses,
       features::kAutofillEnableSupportForApartmentNumbers},
      {});

  AddTextFormFieldData("street", "Street", ADDRESS_HOME_STREET_NAME);
  AddTextFormFieldData("house-number", "House number",
                       ADDRESS_HOME_HOUSE_NUMBER);
  AddTextFormFieldData("apartment", "apartment", ADDRESS_HOME_APT_NUM);
  ClassifyAndVerify();
}

// Tests that the field is not classified as |ADDRESS_HOME_STREET_NAME| when
// it is labeled accordingly but an adjacent field classified as
// |ADDRESS_HOME_HOUSE_NUMBER| is absent.
TEST_F(AddressFieldTest, NotParseStreetNameWithoutHouseNumber) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);
  AddTextFormFieldData("street", "Street", ADDRESS_HOME_LINE1);
  ClassifyAndVerify();
}

// Tests that the field is not classified as |ADDRESS_HOME_HOUSE_NUMBER| when
// it is labeled accordingly but adjacent field classified as
// |ADDRESS_HOME_STREET_NAME| is absent.
TEST_F(AddressFieldTest, NotParseHouseNumberWithoutStreetName) {
  // TODO(crbug.com/1125978): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  AddTextFormFieldData("house-number", "House number", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

// Tests that the dependent locality is correctly classified with
// an unambiguous field name and label.
TEST_F(AddressFieldTest, ParseDependentLocality) {
  // TODO(crbug.com/1157405): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  AddTextFormFieldData("neighborhood", "Neighborhood",
                       ADDRESS_HOME_DEPENDENT_LOCALITY);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseCity) {
  AddTextFormFieldData("city", "City", ADDRESS_HOME_CITY);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseState) {
  AddTextFormFieldData("state", "State", ADDRESS_HOME_STATE);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseZip) {
  AddTextFormFieldData("zip", "Zip", ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseZipFileExtension) {
  AddTextFormFieldData("filename", "Supported formats: .zip, .rar",
                       UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_F(AddressFieldTest, ParseStateAndZipOneLabel) {
  AddTextFormFieldData("state", "State/Province, Zip/Postal Code",
                       ADDRESS_HOME_STATE);
  AddTextFormFieldData("zip", "State/Province, Zip/Postal Code",
                       ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseCountry) {
  AddTextFormFieldData("country", "Country", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

TEST_F(AddressFieldTest, ParseCompany) {
  AddTextFormFieldData("company", "Company", COMPANY_NAME);
  ClassifyAndVerify();
}

// Tests that the dependent locality, city, state, country and zip-code
// fields are correctly classfied with unambiguous field names and labels.
TEST_F(AddressFieldTest,
       ParseDependentLocalityCityStateCountryZipcodeTogether) {
  // TODO(crbug.com/1157405): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

  AddTextFormFieldData("neighborhood", "Neighborhood",
                       ADDRESS_HOME_DEPENDENT_LOCALITY);
  AddTextFormFieldData("city", "City", ADDRESS_HOME_CITY);
  AddTextFormFieldData("state", "State", ADDRESS_HOME_STATE);
  AddTextFormFieldData("country", "Country", ADDRESS_HOME_COUNTRY);
  AddTextFormFieldData("zip", "Zip", ADDRESS_HOME_ZIP);
  ClassifyAndVerify();
}

// Tests that the field is classified as |ADDRESS_HOME_COUNTRY| when the field
// label contains 'Region'.
TEST_F(AddressFieldTest, ParseCountryLabelRegion) {
  AddTextFormFieldData("country", "Country/Region", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

// Tests that the field is classified as |ADDRESS_HOME_COUNTRY| when the field
// name contains 'region'.
TEST_F(AddressFieldTest, ParseCountryNameRegion) {
  AddTextFormFieldData("client_region", "Land", ADDRESS_HOME_COUNTRY);
  ClassifyAndVerify();
}

// Tests that city and state fields are classified correctly when their names
// contain keywords for different types. This is achieved by giving the priority
// to the label over the name for pages in Turkish.
TEST_F(AddressFieldTest, ParseTurkishCityStateWithLabelPrecedence) {
  // TODO(crbug.com/1156315): Remove once launched.
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableLabelPrecedenceForTurkishAddresses);

  AddTextFormFieldData("city", "Il", ADDRESS_HOME_STATE);
  AddTextFormFieldData("county", "Ilce", ADDRESS_HOME_CITY);
  ClassifyAndVerify(ParseResult::PARSED, LanguageCode("tr"));
}

// Tests that address name is not misclassified as address.
TEST_F(AddressFieldTest, NotParseAddressName) {
  AddTextFormFieldData("address", "Adres Başlığı", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED, LanguageCode("tr"));
}

// Tests that the address components sequence in a label is classified
// as |ADDRESS_HOME_LINE1|.
TEST_F(AddressFieldTest, ParseAddressComponentsSequenceAsAddressLine1) {
  AddTextFormFieldData("detail", "Улица, дом, квартира", ADDRESS_HOME_LINE1);
  ClassifyAndVerify(ParseResult::PARSED, LanguageCode("ru"));
}

// Tests that the address components sequence in a label is classified
// as |ADDRESS_HOME_STREET_ADDRESS|.
TEST_F(AddressFieldTest, ParseAddressComponentsSequenceAsStreetAddress) {
  AddFormFieldData("textarea", "detail",
                   "Mahalle, sokak, cadde ve diğer bilgilerinizi girin",
                   ADDRESS_HOME_STREET_ADDRESS);
  ClassifyAndVerify(ParseResult::PARSED, LanguageCode("tr"));
}

}  // namespace autofill
