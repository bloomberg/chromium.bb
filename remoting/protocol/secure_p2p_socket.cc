// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/secure_p2p_socket.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "crypto/symmetric_key.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using net::OldCompletionCallback;
using net::IOBuffer;

namespace remoting {
namespace protocol {

namespace {
const uint8 kMaskSalt[16] = {0xDB, 0x68, 0xB5, 0xFD, 0x17, 0x0E, 0x15, 0x77,
                            0x56, 0xAF, 0x7A, 0x3A, 0x1A, 0x57, 0x75, 0x02};
const uint8 kHashSalt[16] = {0x4E, 0x2F, 0x96, 0xAB, 0x0A, 0x39, 0x92, 0xA2,
                            0x56, 0x94, 0x91, 0xF5, 0x7E, 0x58, 0x2E, 0xFA};
const uint8 kFrameType[4] = {0x0, 0x0, 0x0, 0x1};
const int kFrameTypeSize = sizeof(kFrameType);
const size_t kKeySize = 16;
const int kHeaderSize = 44;
const int kSeqNumberSize = 8;
const int kHashPosition = 0;
const int kNoncePosition = kKeySize;
const int kRawMessagePosition = kNoncePosition + kKeySize;
const int kSeqNumberPosition = kRawMessagePosition;
const int kFrameTypePosition = kSeqNumberPosition + kSeqNumberSize;
const int kMessagePosition = kFrameTypePosition + kFrameTypeSize;
const int kReadBufferSize = 65536;
const std::string kMaskSaltStr(
    reinterpret_cast<const char*>(kMaskSalt), kKeySize);
const std::string kHashSaltStr(
    reinterpret_cast<const char*>(kHashSalt), kKeySize);

inline void SetBE64(void* memory, uint64 v) {
  uint8* mem_ptr = reinterpret_cast<uint8*>(memory);

  mem_ptr[0] = static_cast<uint8>(v >> 56);
  mem_ptr[1] = static_cast<uint8>(v >> 48);
  mem_ptr[2] = static_cast<uint8>(v >> 40);
  mem_ptr[3] = static_cast<uint8>(v >> 32);
  mem_ptr[4] = static_cast<uint8>(v >> 24);
  mem_ptr[5] = static_cast<uint8>(v >> 16);
  mem_ptr[6] = static_cast<uint8>(v >>  8);
  mem_ptr[7] = static_cast<uint8>(v >>  0);
}

inline uint64 GetBE64(const void* memory) {
  const uint8* mem_ptr = reinterpret_cast<const uint8*>(memory);

  return (static_cast<uint64>(mem_ptr[0]) << 56) |
      (static_cast<uint64>(mem_ptr[1]) << 48) |
      (static_cast<uint64>(mem_ptr[2]) << 40) |
      (static_cast<uint64>(mem_ptr[3]) << 32) |
      (static_cast<uint64>(mem_ptr[4]) << 24) |
      (static_cast<uint64>(mem_ptr[5]) << 16) |
      (static_cast<uint64>(mem_ptr[6]) <<  8) |
      (static_cast<uint64>(mem_ptr[7]) <<  0);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
// SecureP2PSocket Implementation.
SecureP2PSocket::SecureP2PSocket(Socket* socket, const std::string& ice_key)
    : socket_(socket),
      write_seq_(0),
      read_seq_(0),
      user_read_callback_(NULL),
      user_read_buf_len_(0),
      user_write_callback_(NULL),
      user_write_buf_len_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(NewCallback(this, &SecureP2PSocket::ReadDone))),
      read_buf_(new net::IOBufferWithSize(kReadBufferSize)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(NewCallback(this, &SecureP2PSocket::WriteDone))),
      msg_hasher_(crypto::HMAC::SHA1) {
  // Make sure the key is valid.
  CHECK(ice_key.size() == kKeySize);

  // Create the mask key from ice key.
  crypto::HMAC mask_hasher(crypto::HMAC::SHA1);
  bool ret = mask_hasher.Init(
      reinterpret_cast<const unsigned char*>(ice_key.data()), kKeySize);
  DCHECK(ret) << "Initialize HMAC-SHA1 for mask failed.";
  scoped_array<uint8> mask_digest(new uint8[mask_hasher.DigestLength()]);
  ret = mask_hasher.Sign(kMaskSaltStr, mask_digest.get(),
                         mask_hasher.DigestLength());
  DCHECK(ret) << "Sign with HMAC-SHA1 for mask failed.";
  mask_key_.reset(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES,
      std::string(mask_digest.get(), mask_digest.get() + kKeySize)));
  DCHECK(mask_key_.get()) << "Import symmetric key failed.";

  // Initialize the encryptor with mask key.
  encryptor_.Init(mask_key_.get(), crypto::Encryptor::CTR, "");

  // Create the hash key from ice key.
  crypto::HMAC hash_hasher(crypto::HMAC::SHA1);
  ret = hash_hasher.Init(
      reinterpret_cast<const unsigned char*>(ice_key.data()), kKeySize);
  DCHECK(ret) << "Initialize HMAC-SHA1 for hash failed.";
  scoped_array<uint8> hash_key(new uint8[hash_hasher.DigestLength()]);
  ret = hash_hasher.Sign(kHashSaltStr, hash_key.get(),
                         hash_hasher.DigestLength());
  DCHECK(ret) << "Sign with HMAC-SHA1 for hash failed.";
  // Create a hasher for message.
  ret = msg_hasher_.Init(hash_key.get(), kKeySize);
  DCHECK(ret) << "Initialize HMAC-SHA1 for message failed.";
}

SecureP2PSocket::~SecureP2PSocket() {
}

int SecureP2PSocket::Read(IOBuffer* buf, int buf_len,
                          OldCompletionCallback* callback) {
  DCHECK(!user_read_buf_);
  DCHECK(!user_read_buf_len_);
  DCHECK(!user_read_callback_);

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;
  user_read_callback_ = callback;
  return ReadInternal();
}

int SecureP2PSocket::Write(IOBuffer* buf, int buf_len,
                           OldCompletionCallback* callback) {
  // See the spec for the steps taken in this method:
  // http://www.whatwg.org/specs/web-apps/current-work/complete/video-conferencing-and-peer-to-peer-communication.html#peer-to-peer-connections
  // 4. Increment sequence number by one.
  ++write_seq_;

  const int encrypted_buffer_size = kHeaderSize + buf_len;
  scoped_refptr<net::IOBuffer> encrypted_buf =
      new net::IOBuffer(encrypted_buffer_size);

  // 6. Concatenate to form the raw message.
  const int kRawMessageSize = kSeqNumberSize + kFrameTypeSize + buf_len;
  std::string raw_message;
  raw_message.resize(kRawMessageSize);
  char* raw_message_buf = const_cast<char*>(raw_message.data());
  SetBE64(raw_message_buf, write_seq_);
  memcpy(raw_message_buf + kSeqNumberSize, kFrameType,
         kFrameTypeSize);
  memcpy(raw_message_buf + kSeqNumberSize + kFrameTypeSize,
         buf->data(), buf_len);

  // 7. Encrypt the message.
  std::string nonce = base::RandBytesAsString(kKeySize);
  CHECK(encryptor_.SetCounter(nonce));
  std::string encrypted_message;
  CHECK(encryptor_.Encrypt(raw_message, &encrypted_message));
  memcpy(encrypted_buf->data() + kRawMessagePosition,
         encrypted_message.data(), encrypted_message.size());

  // 8. Concatenate nonce and encrypted message to form masked message.
  memcpy(encrypted_buf->data() + kNoncePosition, nonce.data(), kKeySize);

  // 10. Create hash from masked message with nonce.
  scoped_array<uint8> msg_digest(new uint8[msg_hasher_.DigestLength()]);
  CHECK(msg_hasher_.Sign(
      base::StringPiece(encrypted_buf->data() + kNoncePosition,
                        kRawMessageSize + kKeySize),
      msg_digest.get(), msg_hasher_.DigestLength()));
  memcpy(encrypted_buf->data() + kHashPosition, msg_digest.get(), kKeySize);

  // Write to the socket.
  int ret = socket_->Write(encrypted_buf, encrypted_buffer_size,
                           write_callback_.get());
  if (ret == net::ERR_IO_PENDING) {
    DCHECK(callback);
    user_write_callback_ = callback;
    user_write_buf_len_ = buf_len;
    return ret;
  } else if (ret < 0) {
    return ret;
  }
  DCHECK_EQ(buf_len + kHeaderSize, ret);
  return buf_len;
}

bool SecureP2PSocket::SetReceiveBufferSize(int32 size) {
  return true;
}

bool SecureP2PSocket::SetSendBufferSize(int32 size) {
  return true;
}

int SecureP2PSocket::ReadInternal() {
  while (true) {
    int ret = socket_->Read(read_buf_, kReadBufferSize, read_callback_.get());
    if (ret == net::ERR_IO_PENDING || ret < 0)
      return ret;

    ret = DecryptBuffer(ret);

    // Can't decrypt the message so try again.
    if (ret == net::ERR_INVALID_RESPONSE)
      continue;

    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
    user_read_callback_ = NULL;
    return ret;
  }
}

void SecureP2PSocket::ReadDone(int err) {
  net::OldCompletionCallback* callback = user_read_callback_;
  user_read_callback_ = NULL;

  if (err < 0) {
    user_read_buf_len_ = 0;
    user_read_buf_ = NULL;
    callback->Run(err);
    return;
  }

  int ret = DecryptBuffer(err);
  if (ret == net::ERR_INVALID_RESPONSE)
    ret = ReadInternal();
  if (ret == net::ERR_IO_PENDING)
    return;

  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  callback->Run(ret);
}

void SecureP2PSocket::WriteDone(int err) {
  net::OldCompletionCallback* callback = user_write_callback_;
  int buf_len = user_write_buf_len_;

  user_write_callback_ = NULL;
  user_write_buf_len_ = 0;

  if (err >= 0) {
    DCHECK_EQ(buf_len + kHeaderSize, err);
    callback->Run(buf_len);
    return;
  }
  callback->Run(err);
}

int SecureP2PSocket::DecryptBuffer(int size) {
  if (size < kRawMessagePosition)
    return net::ERR_INVALID_RESPONSE;

  // See the spec for the steps taken in this method:
  // http://www.whatwg.org/specs/web-apps/current-work/complete/video-conferencing-and-peer-to-peer-communication.html#peer-to-peer-connections
  // 4-7: Verify that the HMAC-SHA1 of all but the first 16 bytes of the
  // masked message with nonce equals the first 16 bytes of the masked message
  // with nonce.
  if (!msg_hasher_.VerifyTruncated(
      base::StringPiece(read_buf_->data() + kNoncePosition,
                        size - kNoncePosition),
      base::StringPiece(read_buf_->data(), kKeySize))) {
    return net::ERR_INVALID_RESPONSE;
  }

  // 8-11. Decrypt the message.
  std::string nonce = std::string(
      read_buf_->data() + kNoncePosition, kKeySize);
  CHECK(encryptor_.SetCounter(nonce));
  const int raw_message_size = size - kRawMessagePosition;

  // TODO(hclam): Change Encryptor API to trim this memcpy.
  std::string encrypted_message(read_buf_->data() + kRawMessagePosition,
                                raw_message_size);
  std::string raw_message;
  CHECK(encryptor_.Decrypt(encrypted_message, &raw_message));

  if (raw_message_size < kSeqNumberSize)
    return net::ERR_INVALID_RESPONSE;

  // 12. Read the sequence number.
  uint64 seq_number = GetBE64(raw_message.data());

  // The spec says we reject the packet if it is out of order. We don't do
  // this so allow upper levels to do reordering.

  // 14. Save the most recent sequence number.
  read_seq_ = seq_number;

  // 15. Parse the frame type.
  if (raw_message_size < kSeqNumberSize + kFrameTypeSize)
    return net::ERR_INVALID_RESPONSE;
  if (memcmp(raw_message.data() + kSeqNumberSize, kFrameType,
             kFrameTypeSize) != 0) {
    return net::ERR_INVALID_RESPONSE;
  }

  // 16. Read the message.
  const int kMessageSize = raw_message_size - kSeqNumberSize - kFrameTypeSize;
  memcpy(user_read_buf_->data(),
         raw_message.data() + kSeqNumberSize + kFrameTypeSize, kMessageSize);
  return kMessageSize;
}

}  // namespace protocol
}  // namespace remoting
