// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/proxy_decryptor.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::IsNull;
using ::testing::NotNull;

using media::DecoderBuffer;
using media::DecryptConfig;
using media::Decryptor;

namespace webkit_media {

static const uint8 kFakeKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
static const uint8 kFakeIv[DecryptConfig::kDecryptionKeySize] = { 0 };
static const char kFakeKeySystem[] = "system.key.fake";
static const char kFakeSessionId[] = "FakeSessionId";
static const uint8 kFakeKey[] = { 0x4b, 0x65, 0x79 };
static const uint8 kEncryptedData[] = { 0x65, 0x6E, 0x63, 0x72, 0x79 };
static const uint8 kDecryptedData[] = { 0x64, 0x65, 0x63, 0x72, 0x79 };

// Creates a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int encrypted_frame_offset = 1;  // This should be non-zero.
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(kEncryptedData, arraysize(kEncryptedData));
  encrypted_buffer->SetDecryptConfig(scoped_ptr<DecryptConfig>(
      new DecryptConfig(
          std::string(reinterpret_cast<const char*>(kFakeKeyId),
                      arraysize(kFakeKeyId)),
          std::string(reinterpret_cast<const char*>(kFakeIv),
                      DecryptConfig::kDecryptionKeySize),
          encrypted_frame_offset,
          std::vector<media::SubsampleEntry>())));
  return encrypted_buffer;
}

ACTION_P2(RunDecryptCB, status, buffer) {
  arg1.Run(status, buffer);
}

ACTION_P(ScheduleMessageLoopToStop, message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

// Tests the interaction between external Decryptor calls and concrete Decryptor
// implementations. This test is not interested in any specific Decryptor
// implementation. A MockDecryptor is used here to serve this purpose.
class ProxyDecryptorTest : public testing::Test {
 public:
  ProxyDecryptorTest()
      : proxy_decryptor_(&client_, NULL, NULL),
        real_decryptor_(new media::MockDecryptor()),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decrypted_buffer_(DecoderBuffer::CopyFrom(kDecryptedData,
                                                  arraysize(kDecryptedData))),
        null_buffer_(scoped_refptr<DecoderBuffer>()),
        decrypt_cb_(base::Bind(&ProxyDecryptorTest::BufferDecrypted,
                               base::Unretained(this))) {
  }

  // Instead of calling ProxyDecryptor::GenerateKeyRequest() here to create a
  // real Decryptor, inject a MockDecryptor for testing purpose.
  void GenerateKeyRequest() {
    proxy_decryptor_.set_decryptor_for_testing(
        scoped_ptr<Decryptor>(real_decryptor_));
  }

  // Since we are using the MockDecryptor, we can simulate any decryption
  // behavior we want. Therefore, we do not care which key is really added,
  // hence always use fake key IDs and keys.
  void AddKey() {
    EXPECT_CALL(*real_decryptor_, AddKey(kFakeKeySystem,
                                         kFakeKeyId, arraysize(kFakeKeyId),
                                         kFakeKey, arraysize(kFakeKey),
                                         kFakeSessionId));
    proxy_decryptor_.AddKey(kFakeKeySystem,
                            kFakeKeyId, arraysize(kFakeKeyId),
                            kFakeKey, arraysize(kFakeKey),
                            kFakeSessionId);
  }

  MOCK_METHOD2(BufferDecrypted, void(Decryptor::Status,
                                     const scoped_refptr<DecoderBuffer>&));

 protected:
  MessageLoop message_loop_;
  media::MockDecryptorClient client_;
  ProxyDecryptor proxy_decryptor_;
  media::MockDecryptor* real_decryptor_;
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<DecoderBuffer> decrypted_buffer_;
  scoped_refptr<DecoderBuffer> null_buffer_;
  Decryptor::DecryptCB decrypt_cb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptorTest);
};

// Tests a typical use case: GKR(), AddKey() and Decrypt() succeeds.
TEST_F(ProxyDecryptorTest, NormalDecryption_Success) {
  GenerateKeyRequest();
  AddKey();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, BufferDecrypted(Decryptor::kSuccess, decrypted_buffer_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
}

// Tests the case where Decrypt() fails.
TEST_F(ProxyDecryptorTest, NormalDecryption_Error) {
  GenerateKeyRequest();
  AddKey();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kError, null_buffer_));
  EXPECT_CALL(*this, BufferDecrypted(Decryptor::kError, IsNull()));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
}

// Tests the case where no key is available for decryption.
TEST_F(ProxyDecryptorTest, NormalDecryption_NoKey) {
  GenerateKeyRequest();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
}

// Tests the case where Decrypt() is called before GKR() is called and the right
// key is added.
TEST_F(ProxyDecryptorTest, DecryptBeforeGenerateKeyRequest) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, BufferDecrypted(Decryptor::kSuccess, decrypted_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  GenerateKeyRequest();
  AddKey();
  message_loop_.Run();
}

// Tests the case where multiple AddKey() is called to add some irrelevant keys
// before the real key that can decrypt |encrypted_buffer_| is added.
TEST_F(ProxyDecryptorTest, MultipleAddKeys) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)))
      .Times(AtLeast(1));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  GenerateKeyRequest();
  const int number_of_irrelevant_addkey = 5;
  for (int i = 0; i < number_of_irrelevant_addkey; ++i)
    AddKey();  // Some irrelevant keys are added.

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, BufferDecrypted(Decryptor::kSuccess, decrypted_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  AddKey();  // Correct key added.
  message_loop_.Run();
}

// Tests the case where Decrypt() is called multiple times (e.g. from multiple
// stream) before the right key is added via AddKey().
TEST_F(ProxyDecryptorTest, MultiplePendingDecryptions) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)))
      .Times(AtLeast(1));
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  GenerateKeyRequest();
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  AddKey();  // An irrelevant key is added.
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, BufferDecrypted(Decryptor::kSuccess, decrypted_buffer_))
      .Times(3);
  AddKey();  // Correct key added.

  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  message_loop_.Run();
}

TEST_F(ProxyDecryptorTest, StopWhenDecryptionsPending) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)))
      .Times(AtLeast(1));
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  GenerateKeyRequest();
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  AddKey();  // An irrelevant key is added.
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*real_decryptor_, Stop());
  EXPECT_CALL(*this, BufferDecrypted(Decryptor::kError, null_buffer_))
      .Times(3);
  proxy_decryptor_.Stop();

  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  message_loop_.Run();
}

}  // namespace webkit_media
