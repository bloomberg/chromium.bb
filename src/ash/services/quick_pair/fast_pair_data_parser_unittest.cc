// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/fast_pair_data_parser.h"

#include <stddef.h>
#include <iterator>

#include "ash/quick_pair/common/fast_pair/fast_pair_service_data_creator.h"
#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace {

constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kAccountKeyFilterNoNotificationHeader = 0b01100010;
constexpr int kSaltHeader = 0b00010001;
const std::vector<uint8_t> kSaltBytes = {0x01};
const std::vector<uint8_t> kInvalidSaltBytes = {0x01, 0x02};
const std::vector<uint8_t> kDeviceAddressBytes = {17, 18, 19, 20, 21, 22};
constexpr int kBatteryHeader = 0b00110011;
constexpr int kBatterHeaderNoNotification = 0b00110100;

const std::string kModelId = "112233";
const std::string kAccountKeyFilter = "112233445566";
const std::string kSalt = "01";
const std::string kInvalidSalt = "01020404050607";
const std::string kBattery = "01048F";
const std::string kDeviceAddress = "11:12:13:14:15:16";

std::vector<uint8_t> aes_key_bytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                      0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                      0x61, 0xC3, 0x32, 0x1D};

std::vector<uint8_t> EncryptBytes(const std::vector<uint8_t>& bytes) {
  AES_KEY aes_key;
  AES_set_encrypt_key(aes_key_bytes.data(), aes_key_bytes.size() * 8, &aes_key);
  uint8_t encrypted_bytes[16];
  AES_encrypt(bytes.data(), encrypted_bytes, &aes_key);
  return std::vector<uint8_t>(std::begin(encrypted_bytes),
                              std::end(encrypted_bytes));
}

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDataParserTest : public testing::Test {
 public:
  void SetUp() override {
    data_parser_ = std::make_unique<FastPairDataParser>(
        remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::FastPairDataParser> remote_;
  std::unique_ptr<FastPairDataParser> data_parser_;
};

TEST_F(FastPairDataParserTest, DecryptResponseUnsuccessfully) {
  std::vector<uint8_t> response_bytes = {/*message_type=*/0x02,
                                         /*address_bytes=*/0x02,
                                         0x03,
                                         0x04,
                                         0x05,
                                         0x06,
                                         0x07,
                                         /*salt=*/0x08,
                                         0x09,
                                         0x0A,
                                         0x0B,
                                         0x0C,
                                         0x0D,
                                         0x0E,
                                         0x0F,
                                         0x00};
  std::vector<uint8_t> encrypted_bytes = EncryptBytes(response_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](const absl::optional<DecryptedResponse>& response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedResponse(aes_key_bytes, encrypted_bytes,
                                       std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptResponseSuccessfully) {
  std::vector<uint8_t> response_bytes;

  // Message type.
  uint8_t message_type = 0x01;
  response_bytes.push_back(message_type);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::copy(address_bytes.begin(), address_bytes.end(),
            std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x0E, 0x0F, 0x00};
  std::copy(salt.begin(), salt.end(), std::back_inserter(response_bytes));

  std::vector<uint8_t> encrypted_bytes = EncryptBytes(response_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop, &address_bytes,
       &salt](const absl::optional<DecryptedResponse>& response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->message_type,
                  FastPairMessageType::kKeyBasedPairingResponse);
        EXPECT_EQ(response->address_bytes, address_bytes);
        EXPECT_EQ(response->salt, salt);
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedResponse(aes_key_bytes, encrypted_bytes,
                                       std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptPasskeyUnsuccessfully) {
  std::vector<uint8_t> passkey_bytes = {/*message_type=*/0x04,
                                        /*passkey=*/0x02,
                                        0x03,
                                        0x04,
                                        /*salt=*/0x05,
                                        0x06,
                                        0x07,
                                        0x08,
                                        0x09,
                                        0x0A,
                                        0x0B,
                                        0x0C,
                                        0x0D,
                                        0x0E,
                                        0x0F,
                                        0x0E};
  std::vector<uint8_t> encrypted_bytes = EncryptBytes(passkey_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](const absl::optional<DecryptedPasskey>& passkey) {
        EXPECT_FALSE(passkey.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes,
                                      std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptSeekerPasskeySuccessfully) {
  std::vector<uint8_t> passkey_bytes;
  // Message type.
  uint8_t message_type = 0x02;
  passkey_bytes.push_back(message_type);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(salt.begin(), salt.end(), std::back_inserter(passkey_bytes));

  std::vector<uint8_t> encrypted_bytes = EncryptBytes(passkey_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop, &passkey,
       &salt](const absl::optional<DecryptedPasskey>& decrypted_passkey) {
        EXPECT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey->message_type,
                  FastPairMessageType::kSeekersPasskey);
        EXPECT_EQ(decrypted_passkey->passkey, passkey);
        EXPECT_EQ(decrypted_passkey->salt, salt);
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes,
                                      std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptProviderPasskeySuccessfully) {
  std::vector<uint8_t> passkey_bytes;
  // Message type.
  uint8_t message_type = 0x03;
  passkey_bytes.push_back(message_type);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(salt.begin(), salt.end(), std::back_inserter(passkey_bytes));

  std::vector<uint8_t> encrypted_bytes = EncryptBytes(passkey_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop, &passkey,
       &salt](const absl::optional<DecryptedPasskey>& decrypted_passkey) {
        EXPECT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey->message_type,
                  FastPairMessageType::kProvidersPasskey);
        EXPECT_EQ(decrypted_passkey->passkey, passkey);
        EXPECT_EQ(decrypted_passkey->salt, salt);
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes,
                                      std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_Empty) {
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(
      std::vector<uint8_t>(), kDeviceAddress, std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_NoApplicibleData) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_AccountKeyFilter) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_FALSE(advertisement->battery_notification.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_AccountKeyFilterNoNotification) {
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterNoNotificationHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .Build()
          ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_FALSE(advertisement->show_ui);
        EXPECT_FALSE(advertisement->battery_notification.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_WrongVersion) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00100000)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_ZeroLengthExtraField) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField("")
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_WrongType) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(0b01100001)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_SaltTooLarge) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(0b01100001)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kInvalidSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_Battery) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .AddExtraFieldHeader(kBatteryHeader)
                                   .AddExtraField(kBattery)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_TRUE(advertisement->battery_notification->show_ui);
        EXPECT_FALSE(
            advertisement->battery_notification->left_bud_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->left_bud_info.percentage,
                  1);
        EXPECT_FALSE(
            advertisement->battery_notification->right_bud_info.is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->right_bud_info.percentage, 4);
        EXPECT_TRUE(advertisement->battery_notification->case_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->case_info.percentage,
                  15);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_MissingSalt) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kBatteryHeader)
                                   .AddExtraField(kBattery)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kDeviceAddressBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_TRUE(advertisement->battery_notification->show_ui);
        EXPECT_FALSE(
            advertisement->battery_notification->left_bud_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->left_bud_info.percentage,
                  1);
        EXPECT_FALSE(
            advertisement->battery_notification->right_bud_info.is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->right_bud_info.percentage, 4);
        EXPECT_TRUE(advertisement->battery_notification->case_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->case_info.percentage,
                  15);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_BatteryNoUi) {
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .AddExtraFieldHeader(kBatterHeaderNoNotification)
          .AddExtraField(kBattery)
          .Build()
          ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_FALSE(advertisement->battery_notification->show_ui);
        EXPECT_FALSE(
            advertisement->battery_notification->left_bud_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->left_bud_info.percentage,
                  1);
        EXPECT_FALSE(
            advertisement->battery_notification->right_bud_info.is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->right_bud_info.percentage, 4);
        EXPECT_TRUE(advertisement->battery_notification->case_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->case_info.percentage,
                  15);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_EnableSilenceMode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x01, /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_enable_silence_mode());
        EXPECT_TRUE(messages[0]->get_enable_silence_mode());
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_SilenceMode_AdditionalData) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x01, /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x08};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_DisableSilenceMode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x01, /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_enable_silence_mode());
        EXPECT_FALSE(messages[0]->get_enable_silence_mode());
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_BluetoothInvalidMessageCode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x01, /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_CompanionAppLogBufferFull) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x02, /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_companion_app_log_buffer_full());
        EXPECT_TRUE(messages[0]->get_companion_app_log_buffer_full());
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_CompanionAppInvalidMessageCode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x02, /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_CompanionAppLogBufferFull_AdditionalData) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x02, /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x08};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_ModelId) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x03,
                                /*additional_data=*/0xAA,
                                0xBB,
                                0xCC};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_model_id());
        EXPECT_EQ(messages[0]->get_model_id(), "AABBCC");
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_BleAddress) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03,
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00,
                                0x06,
                                /*additional_data=*/0xAA,
                                0xBB,
                                0xCC,
                                0xDD,
                                0xEE,
                                0xFF};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ble_address_update());
        EXPECT_EQ(messages[0]->get_ble_address_update(), "AA:BB:CC:DD:EE:FF");
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_WrongAdditionalDataSize) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03,
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00,
                                0x08,
                                /*additional_data=*/0xAA,
                                0xBB,
                                0xCC,
                                0xDD,
                                0xEE,
                                0xFF};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_BatteryNotification) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03,
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00,
                                0x03,
                                /*additional_data=*/0x57,
                                0x41,
                                0x7F};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_battery_update());
        EXPECT_EQ(messages[0]->get_battery_update()->left_bud_info->percentage,
                  87);
        EXPECT_EQ(messages[0]->get_battery_update()->right_bud_info->percentage,
                  65);
        EXPECT_EQ(messages[0]->get_battery_update()->case_info->percentage, -1);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RemainingBatteryTime) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x04,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0xF0};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_remaining_battery_time());
        EXPECT_EQ(messages[0]->get_remaining_battery_time(), 240);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_RemainingBatteryTime_2BytesAdditionalData) {
  std::vector<uint8_t> bytes = {
      /*mesage_group=*/0x03,           /*mesage_code=*/0x04,
      /*additional_data_length=*/0x00, 0x02,
      /*additional_data=*/0x01,        0x0F};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_remaining_battery_time());
        EXPECT_EQ(messages[0]->get_remaining_battery_time(), 271);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_DeviceInfoInvalidMessageCode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x09,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_ModelIdInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_BleAddressUpdateInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_BatteryUpdateInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_RemainingBatteryInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x04,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_ActiveComponentsInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x06,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_ActiveComponents) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x06,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x03};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_active_components_byte());
        EXPECT_EQ(messages[0]->get_active_components_byte(), 0x03);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_AndroidPlatform) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03,
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x01,
                                0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_sdk_version());
        EXPECT_EQ(messages[0]->get_sdk_version(), 28);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_PlatformInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03, /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_InvalidPlatform) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x03,
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x02,
                                0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RingDeviceNoTimeout) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04, /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, -1);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RingDeviceTimeout) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x01,
                                0x3C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, 60);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RingInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x03,
                                /*additional_data=*/0x02,
                                0x1C,
                                0x02};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_RingInvalidMessageCode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04,
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x02,
                                0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_Ack) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0xFF,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x04,
                                0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_acknowledgement());
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_code,
                  0x01);
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_group,
                  mojom::MessageGroup::kDeviceActionEvent);
        EXPECT_EQ(messages[0]->get_acknowledgement()->acknowledgement,
                  mojom::Acknowledgement::kAck);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_Nak) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0xFF,
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00,
                                0x03,
                                /*additional_data=*/0x00,
                                0x04,
                                0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_acknowledgement());
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_code,
                  0x01);
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_group,
                  mojom::MessageGroup::kDeviceActionEvent);
        EXPECT_EQ(messages[0]->get_acknowledgement()->acknowledgement,
                  mojom::Acknowledgement::kNotSupportedNak);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_AckInvalidMessageCode) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0xFF,
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x04,
                                0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_AckInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0xFF,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x03,
                                /*additional_data=*/0x04,
                                0x01,
                                0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_NakInvalidLength) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0xFF,
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x00,
                                0x04};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_NotEnoughBytes) {
  std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_MultipleMessages_Valid) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x01,
                                /*additional_data=*/0x01,
                                /*mesage_group=*/0x03,
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x01,
                                0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 2);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, -1);
        EXPECT_TRUE(messages[1]->is_sdk_version());
        EXPECT_EQ(messages[1]->get_sdk_version(), 28);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_MultipleMessages_ValidInvalid) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x01,
                                /*additional_data=*/0x01,
                                /*mesage_group=*/0x03,
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x02,
                                0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, -1);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_MultipleMessages_Invalid) {
  std::vector<uint8_t> bytes = {/*mesage_group=*/0x04,
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00,
                                0x00,
                                /*mesage_group=*/0x03,
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00,
                                0x02,
                                /*additional_data=*/0x02,
                                0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

}  // namespace quick_pair
}  // namespace ash
