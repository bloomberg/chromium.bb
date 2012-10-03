// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/proxy_decryptor.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
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
using ::testing::SaveArg;
using ::testing::StrictMock;

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

ACTION_P3(ResetAndRunDecryptCB, decrypt_cb, status, buffer) {
  base::ResetAndReturn(decrypt_cb).Run(status, buffer);
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
      : proxy_decryptor_(message_loop_.message_loop_proxy(),
                         &client_, NULL, NULL),
        real_decryptor_(new media::MockDecryptor()),
        scoped_real_decryptor_(real_decryptor_),
        decrypt_cb_(base::Bind(&ProxyDecryptorTest::DeliverBuffer,
                               base::Unretained(this))),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decrypted_buffer_(DecoderBuffer::CopyFrom(kDecryptedData,
                                                  arraysize(kDecryptedData))),
        null_buffer_(scoped_refptr<DecoderBuffer>()) {
  }

  // Instead of calling ProxyDecryptor::GenerateKeyRequest() here to create a
  // real Decryptor, inject a MockDecryptor for testing purpose.
  void GenerateKeyRequest() {
    proxy_decryptor_.set_decryptor_for_testing(scoped_real_decryptor_.Pass());
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

  // The DecryptCB passed to proxy_decryptor_.Decrypt().
  MOCK_METHOD2(DeliverBuffer, void(Decryptor::Status,
                                   const scoped_refptr<DecoderBuffer>&));

 protected:
  MessageLoop message_loop_;
  StrictMock<media::MockDecryptorClient> client_;
  ProxyDecryptor proxy_decryptor_;

  // Raw pointer to the real decryptor so that we can call EXPECT_CALL on the
  // mock decryptor.
  media::MockDecryptor* real_decryptor_;

  // Scoped pointer that takes ownership of |real_decryptor_|. When
  // GenerateKeyRequest() is called, the ownership of |real_decryptor_| is
  // passed from |scoped_real_decryptor_| to |proxy_decryptor_|. We need this
  // to manage life time of |real_decryptor_| because not all tests call
  // GenerateKeyRequest().
  scoped_ptr<Decryptor> scoped_real_decryptor_;

  Decryptor::DecryptCB decrypt_cb_;

  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<DecoderBuffer> decrypted_buffer_;
  scoped_refptr<DecoderBuffer> null_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptorTest);
};

// Tests a typical use case: GKR(), AddKey() and Decrypt() succeeds.
TEST_F(ProxyDecryptorTest, NormalDecryption_Success) {
  GenerateKeyRequest();
  AddKey();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, decrypted_buffer_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
}

// Tests the case where Decrypt() fails.
TEST_F(ProxyDecryptorTest, NormalDecryption_Error) {
  GenerateKeyRequest();
  AddKey();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kError, null_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kError, IsNull()));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
}

// Tests the case where no key is available for decryption.
TEST_F(ProxyDecryptorTest, NormalDecryption_NoKey) {
  GenerateKeyRequest();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, null_buffer_));
  proxy_decryptor_.CancelDecrypt();
}

// Tests the case where Decrypt() is called after the right key is added.
TEST_F(ProxyDecryptorTest, DecryptBeforeAddKey) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)));
  GenerateKeyRequest();
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, decrypted_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  AddKey();

  message_loop_.Run();
}

// Tests the case where Decrypt() is called before GKR() and the right key is
// added.
TEST_F(ProxyDecryptorTest, DecryptBeforeGenerateKeyRequest) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, decrypted_buffer_))
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
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, decrypted_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  AddKey();  // Correct key added.

  message_loop_.Run();
}

// Test the case where a new key is added before the |real_decryptor_| returns
// kNoKey. The exact call sequence is:
// 1, Decrypt an encrypted buffer.
// 2, The correct key is added.
// 3, The previously decrypt call returned kNoKey.
// In this case, the ProxyDecryptor is smart enough to try the decryption again
// and get the buffer decrypted!
TEST_F(ProxyDecryptorTest, AddKeyAfterDecryptButBeforeNoKeyReturned) {
  Decryptor::DecryptCB decrypt_cb;
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(SaveArg<1>(&decrypt_cb));

  GenerateKeyRequest();
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  EXPECT_FALSE(decrypt_cb.is_null());

  AddKey();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, decrypted_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  base::ResetAndReturn(&decrypt_cb).Run(Decryptor::kNoKey, null_buffer_);

  message_loop_.Run();
}

// Test the case where we cancel the pending decryption callback even before
// GenerateKeyRequest is called. In this case, the decryptor was not even
// created!
TEST_F(ProxyDecryptorTest, CancelDecryptWithoutGenerateKeyRequestCalled) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)))
      .Times(AtLeast(1));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, null_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  proxy_decryptor_.CancelDecrypt();

  message_loop_.Run();
}

// Test the case where we cancel the pending decryption callback when it's
// stored in the ProxyDecryptor.
TEST_F(ProxyDecryptorTest, CancelDecryptWhenDecryptPendingInProxyDecryptor) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)))
      .Times(AtLeast(1));
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  GenerateKeyRequest();
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, null_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  proxy_decryptor_.CancelDecrypt();

  message_loop_.Run();
}

// Test the case where we cancel the pending decryption callback when it's
// pending at the |real_decryptor_|.
TEST_F(ProxyDecryptorTest, CancelDecryptWhenDecryptPendingInRealDecryptor) {
  Decryptor::DecryptCB decrypt_cb;
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(SaveArg<1>(&decrypt_cb));
  GenerateKeyRequest();
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  EXPECT_FALSE(decrypt_cb.is_null());

  // Even though the real decryptor returns kError, DeliverBuffer() should
  // still be called with kSuccess and NULL because we are canceling the
  // decryption.
  EXPECT_CALL(*real_decryptor_, CancelDecrypt())
      .WillOnce(ResetAndRunDecryptCB(&decrypt_cb,
                                     Decryptor::kError, null_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, null_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  proxy_decryptor_.CancelDecrypt();

  message_loop_.Run();
}

// Test the case where we try to decrypt again after the previous decrypt was
// canceled.
TEST_F(ProxyDecryptorTest, DecryptAfterCancelDecrypt) {
  EXPECT_CALL(client_, NeedKeyMock("", "", NotNull(), arraysize(kFakeKeyId)))
      .Times(AtLeast(1));
  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillRepeatedly(RunDecryptCB(Decryptor::kNoKey, null_buffer_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);
  GenerateKeyRequest();

  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, null_buffer_));
  proxy_decryptor_.CancelDecrypt();

  AddKey();

  EXPECT_CALL(*real_decryptor_, Decrypt(encrypted_buffer_, _))
      .WillOnce(RunDecryptCB(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, DeliverBuffer(Decryptor::kSuccess, decrypted_buffer_))
      .WillOnce(ScheduleMessageLoopToStop(&message_loop_));
  proxy_decryptor_.Decrypt(encrypted_buffer_, decrypt_cb_);

  message_loop_.Run();
}

}  // namespace webkit_media
