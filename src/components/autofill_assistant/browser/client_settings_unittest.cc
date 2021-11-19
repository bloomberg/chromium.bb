// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/mock_client.h"

namespace autofill_assistant {

namespace {

using testing::Field;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

class ClientSettingsTest : public testing::Test {
 protected:
  ClientSettingsTest() {}
  ~ClientSettingsTest() override {}

  void AddDisplayStringToProto(ClientSettingsProto::DisplayStringId id,
                               const std::string str,
                               ClientSettingsProto& proto) {
    ClientSettingsProto::DisplayString* display_str =
        proto.add_display_strings();
    display_str->set_id(id);
    display_str->set_value(str);
  }
};

TEST_F(ClientSettingsTest, CheckLegacyOverlayImage) {
  ClientSettingsProto proto;
  proto.mutable_overlay_image()->set_image_url(
      "https://www.example.com/favicon.ico");
  proto.mutable_overlay_image()->mutable_image_size()->set_dp(32);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ASSERT_TRUE(settings.overlay_image.has_value());
  EXPECT_EQ(settings.overlay_image->image_drawable().bitmap().url(),
            "https://www.example.com/favicon.ico");
}

TEST_F(ClientSettingsTest, NoDisplayStringsNoLocale) {
  ClientSettingsProto proto;
  ClientSettings settings;
  settings.UpdateFromProto(proto);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, IsEmpty()),
                    Field(&ClientSettings::display_strings, IsEmpty())));
}

TEST_F(ClientSettingsTest, DisplayStringsSetWithValidLocale) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "en-US"),
                    Field(&ClientSettings::display_strings,
                          UnorderedElementsAre(
                              Pair(ClientSettingsProto::GIVE_UP, "give_up"),
                              Pair(ClientSettingsProto::MAYBE_GIVE_UP,
                                   "maybe_give_up")))));
}

TEST_F(ClientSettingsTest, DisplayStringsDoesNotMergeWhenLocaleEmpty) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  AddDisplayStringToProto(ClientSettingsProto::DEFAULT_ERROR, "default_error",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "new_give_up",
                          proto_new);
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(
      settings,
      AllOf(Field(&ClientSettings::display_strings_locale, "en-US"),
            Field(&ClientSettings::display_strings,
                  UnorderedElementsAre(
                      Pair(ClientSettingsProto::GIVE_UP, "give_up"),
                      Pair(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up"),
                      Pair(ClientSettingsProto::DEFAULT_ERROR,
                           "default_error")))));
}

TEST_F(ClientSettingsTest, DisplayStringsMergedWithSameLocale) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  AddDisplayStringToProto(ClientSettingsProto::DEFAULT_ERROR, "default_error",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "new_give_up",
                          proto_new);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP,
                          "new_maybe_give_up", proto_new);
  proto_new.set_display_strings_locale("en-US");
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(
      settings,
      AllOf(
          Field(&ClientSettings::display_strings_locale, "en-US"),
          Field(
              &ClientSettings::display_strings,
              UnorderedElementsAre(
                  Pair(ClientSettingsProto::GIVE_UP, "new_give_up"),
                  Pair(ClientSettingsProto::MAYBE_GIVE_UP, "new_maybe_give_up"),
                  Pair(ClientSettingsProto::DEFAULT_ERROR, "default_error")))));
}

TEST_F(ClientSettingsTest,
       DisplayStringsClearedForLocaleSwitchWithEmptyStrings) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  proto_new.set_display_strings_locale("fr-FR");
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "fr-FR"),
                    Field(&ClientSettings::display_strings, IsEmpty())));
}

TEST_F(ClientSettingsTest, DisplayStringsReplacedForLocaleSwitch) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  proto_new.set_display_strings_locale("fr-FR");
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "fr_give_up",
                          proto_new);
  AddDisplayStringToProto(ClientSettingsProto::DEFAULT_ERROR,
                          "fr_default_error", proto_new);
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "fr-FR"),
                    Field(&ClientSettings::display_strings,
                          UnorderedElementsAre(
                              Pair(ClientSettingsProto::GIVE_UP, "fr_give_up"),
                              Pair(ClientSettingsProto::DEFAULT_ERROR,
                                   "fr_default_error")))));
}

TEST_F(ClientSettingsTest, EmptyUpdateDoesNotResetDisplayStrings) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "en-US"),
                    Field(&ClientSettings::display_strings,
                          UnorderedElementsAre(
                              Pair(ClientSettingsProto::GIVE_UP, "give_up"),
                              Pair(ClientSettingsProto::MAYBE_GIVE_UP,
                                   "maybe_give_up")))));
}
}  // namespace
}  // namespace autofill_assistant
