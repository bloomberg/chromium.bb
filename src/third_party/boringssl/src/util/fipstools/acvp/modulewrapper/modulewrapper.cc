/* Copyright (c) 2019, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <map>
#include <string>
#include <vector>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdarg>

#include <openssl/aead.h>
#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/cipher.h>
#include <openssl/cmac.h>
#include <openssl/digest.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/obj.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/span.h>

#include "../../../../crypto/fipsmodule/rand/internal.h"

static constexpr size_t kMaxArgs = 8;
static constexpr size_t kMaxArgLength = (1 << 20);
static constexpr size_t kMaxNameLength = 30;

static_assert((kMaxArgs - 1 * kMaxArgLength) + kMaxNameLength > (1 << 30),
              "Argument limits permit excessive messages");

using namespace bssl;

static bool ReadAll(int fd, void *in_data, size_t data_len) {
  uint8_t *data = reinterpret_cast<uint8_t *>(in_data);
  size_t done = 0;

  while (done < data_len) {
    ssize_t r;
    do {
      r = read(fd, &data[done], data_len - done);
    } while (r == -1 && errno == EINTR);

    if (r <= 0) {
      return false;
    }

    done += r;
  }

  return true;
}

template <typename... Args>
static bool WriteReply(int fd, Args... args) {
  std::vector<Span<const uint8_t>> spans = {args...};
  if (spans.empty() || spans.size() > kMaxArgs) {
    abort();
  }

  uint32_t nums[1 + kMaxArgs];
  iovec iovs[kMaxArgs + 1];
  nums[0] = spans.size();
  iovs[0].iov_base = nums;
  iovs[0].iov_len = sizeof(uint32_t) * (1 + spans.size());

  size_t num_iov = 1;
  for (size_t i = 0; i < spans.size(); i++) {
    const auto &span = spans[i];
    nums[i + 1] = span.size();
    if (span.empty()) {
      continue;
    }

    iovs[num_iov].iov_base = const_cast<uint8_t *>(span.data());
    iovs[num_iov].iov_len = span.size();
    num_iov++;
  }

  size_t iov_done = 0;
  while (iov_done < num_iov) {
    ssize_t r;
    do {
      r = writev(fd, &iovs[iov_done], num_iov - iov_done);
    } while (r == -1 && errno == EINTR);

    if (r <= 0) {
      return false;
    }

    size_t written = r;
    for (size_t i = iov_done; i < num_iov && written > 0; i++) {
      iovec &iov = iovs[i];

      size_t done = written;
      if (done > iov.iov_len) {
        done = iov.iov_len;
      }

      iov.iov_base = reinterpret_cast<uint8_t *>(iov.iov_base) + done;
      iov.iov_len -= done;
      written -= done;

      if (iov.iov_len == 0) {
        iov_done++;
      }
    }

    assert(written == 0);
  }

  return true;
}

static bool GetConfig(const Span<const uint8_t> args[]) {
  static constexpr char kConfig[] =
      R"([
      {
        "algorithm": "SHA2-224",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA2-256",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA2-384",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA2-512",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "SHA-1",
        "revision": "1.0",
        "messageLength": [{
          "min": 0, "max": 65528, "increment": 8
        }]
      },
      {
        "algorithm": "ACVP-AES-ECB",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [128, 192, 256]
      },
      {
        "algorithm": "ACVP-AES-CTR",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [128, 192, 256],
        "payloadLen": [{
          "min": 8, "max": 128, "increment": 8
        }],
        "incrementalCounter": true,
        "overflowCounter": true,
        "performCounterTests": true
      },
      {
        "algorithm": "ACVP-AES-CBC",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [128, 192, 256]
      },
      {
        "algorithm": "ACVP-AES-GCM",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [128, 192, 256],
        "payloadLen": [{
          "min": 0, "max": 256, "increment": 8
        }],
        "aadLen": [{
          "min": 0, "max": 256, "increment": 8
        }],
        "tagLen": [128],
        "ivLen": [96],
        "ivGen": "external"
      },
      {
        "algorithm": "ACVP-AES-KW",
        "revision": "1.0",
        "direction": [
            "encrypt",
            "decrypt"
        ],
        "kwCipher": [
            "cipher"
        ],
        "keyLen": [
            128, 192, 256
        ],
        "payloadLen": [{"min": 128, "max": 1024, "increment": 64}]
      },
      {
        "algorithm": "ACVP-AES-KWP",
        "revision": "1.0",
        "direction": [
            "encrypt",
            "decrypt"
        ],
        "kwCipher": [
            "cipher"
        ],
        "keyLen": [
            128, 192, 256
        ],
        "payloadLen": [{"min": 8, "max": 1024, "increment": 8}]
      },
      {
        "algorithm": "ACVP-AES-CCM",
        "revision": "1.0",
        "direction": [
            "encrypt",
            "decrypt"
        ],
        "keyLen": [
            128
        ],
        "payloadLen": [{"min": 0, "max": 256, "increment": 8}],
        "ivLen": [104],
        "tagLen": [32],
        "aadLen": [{"min": 0, "max": 1024, "increment": 8}]
      },
      {
        "algorithm": "ACVP-TDES-ECB",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [192],
        "keyingOption": [1]
      },
      {
        "algorithm": "ACVP-TDES-CBC",
        "revision": "1.0",
        "direction": ["encrypt", "decrypt"],
        "keyLen": [192],
        "keyingOption": [1]
      },
      {
        "algorithm": "HMAC-SHA-1",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 160, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-224",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 224, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-256",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 256, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-384",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 384, "increment": 8
        }]
      },
      {
        "algorithm": "HMAC-SHA2-512",
        "revision": "1.0",
        "keyLen": [{
          "min": 8, "max": 2048, "increment": 8
        }],
        "macLen": [{
          "min": 32, "max": 512, "increment": 8
        }]
      },
      {
        "algorithm": "ctrDRBG",
        "revision": "1.0",
        "predResistanceEnabled": [false],
        "reseedImplemented": false,
        "capabilities": [{
          "mode": "AES-256",
          "derFuncEnabled": false,
          "entropyInputLen": [384],
          "nonceLen": [0],
          "persoStringLen": [{"min": 0, "max": 384, "increment": 16}],
          "additionalInputLen": [
            {"min": 0, "max": 384, "increment": 16}
          ],
          "returnedBitsLen": 2048
        }]
      },
      {
        "algorithm": "ECDSA",
        "mode": "keyGen",
        "revision": "1.0",
        "curve": [
          "P-224",
          "P-256",
          "P-384",
          "P-521"
        ],
        "secretGenerationMode": [
          "testing candidates"
        ]
      },
      {
        "algorithm": "ECDSA",
        "mode": "keyVer",
        "revision": "1.0",
        "curve": [
          "P-224",
          "P-256",
          "P-384",
          "P-521"
        ]
      },
      {
        "algorithm": "ECDSA",
        "mode": "sigGen",
        "revision": "1.0",
        "capabilities": [{
          "curve": [
            "P-224",
            "P-256",
            "P-384",
            "P-521"
          ],
          "hashAlg": [
            "SHA2-224",
            "SHA2-256",
            "SHA2-384",
            "SHA2-512"
          ]
        }]
      },
      {
        "algorithm": "ECDSA",
        "mode": "sigVer",
        "revision": "1.0",
        "capabilities": [{
          "curve": [
            "P-224",
            "P-256",
            "P-384",
            "P-521"
          ],
          "hashAlg": [
            "SHA2-224",
            "SHA2-256",
            "SHA2-384",
            "SHA2-512"
          ]
        }]
      },
      {
        "algorithm": "RSA",
        "mode": "keyGen",
        "revision": "FIPS186-4",
        "infoGeneratedByServer": true,
        "pubExpMode": "fixed",
        "fixedPubExp": "010001",
        "keyFormat": "standard",
        "capabilities": [{
          "randPQ": "B.3.3",
          "properties": [{
            "modulo": 2048,
            "primeTest": [
              "tblC2"
            ]
          },{
            "modulo": 3072,
            "primeTest": [
              "tblC2"
            ]
          },{
            "modulo": 4096,
            "primeTest": [
              "tblC2"
            ]
          }]
        }]
      },
      {
        "algorithm": "RSA",
        "mode": "sigGen",
        "revision": "FIPS186-4",
        "capabilities": [{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 2048,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 3072,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 4096,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pss",
          "properties": [{
            "modulo": 2048,
            "hashPair": [{
              "hashAlg": "SHA2-224",
              "saltLen": 28
            }, {
              "hashAlg": "SHA2-256",
              "saltLen": 32
            }, {
              "hashAlg": "SHA2-384",
              "saltLen": 48
            }, {
              "hashAlg": "SHA2-512",
              "saltLen": 64
            }, {
              "hashAlg": "SHA-1",
              "saltLen": 20
            }]
          }]
        },{
          "sigType": "pss",
          "properties": [{
            "modulo": 3072,
            "hashPair": [{
              "hashAlg": "SHA2-224",
              "saltLen": 28
            }, {
              "hashAlg": "SHA2-256",
              "saltLen": 32
            }, {
              "hashAlg": "SHA2-384",
              "saltLen": 48
            }, {
              "hashAlg": "SHA2-512",
              "saltLen": 64
            }, {
              "hashAlg": "SHA-1",
              "saltLen": 20
            }]
          }]
        },{
          "sigType": "pss",
          "properties": [{
            "modulo": 4096,
            "hashPair": [{
              "hashAlg": "SHA2-224",
              "saltLen": 28
            }, {
              "hashAlg": "SHA2-256",
              "saltLen": 32
            }, {
              "hashAlg": "SHA2-384",
              "saltLen": 48
            }, {
              "hashAlg": "SHA2-512",
              "saltLen": 64
            }, {
              "hashAlg": "SHA-1",
              "saltLen": 20
            }]
          }]
        }]
      },
      {
        "algorithm": "RSA",
        "mode": "sigVer",
        "revision": "FIPS186-4",
        "pubExpMode": "fixed",
        "fixedPubExp": "010001",
        "capabilities": [{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 1024,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 2048,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 3072,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pkcs1v1.5",
          "properties": [{
            "modulo": 4096,
            "hashPair": [{
              "hashAlg": "SHA2-224"
            }, {
              "hashAlg": "SHA2-256"
            }, {
              "hashAlg": "SHA2-384"
            }, {
              "hashAlg": "SHA2-512"
            }, {
              "hashAlg": "SHA-1"
            }]
          }]
        },{
          "sigType": "pss",
          "properties": [{
            "modulo": 2048,
            "hashPair": [{
              "hashAlg": "SHA2-224",
              "saltLen": 28
            }, {
              "hashAlg": "SHA2-256",
              "saltLen": 32
            }, {
              "hashAlg": "SHA2-384",
              "saltLen": 48
            }, {
              "hashAlg": "SHA2-512",
              "saltLen": 64
            }, {
              "hashAlg": "SHA-1",
              "saltLen": 20
            }]
          }]
        },{
          "sigType": "pss",
          "properties": [{
            "modulo": 3072,
            "hashPair": [{
              "hashAlg": "SHA2-224",
              "saltLen": 28
            }, {
              "hashAlg": "SHA2-256",
              "saltLen": 32
            }, {
              "hashAlg": "SHA2-384",
              "saltLen": 48
            }, {
              "hashAlg": "SHA2-512",
              "saltLen": 64
            }, {
              "hashAlg": "SHA-1",
              "saltLen": 20
            }]
          }]
        },{
          "sigType": "pss",
          "properties": [{
            "modulo": 4096,
            "hashPair": [{
              "hashAlg": "SHA2-224",
              "saltLen": 28
            }, {
              "hashAlg": "SHA2-256",
              "saltLen": 32
            }, {
              "hashAlg": "SHA2-384",
              "saltLen": 48
            }, {
              "hashAlg": "SHA2-512",
              "saltLen": 64
            }, {
              "hashAlg": "SHA-1",
              "saltLen": 20
            }]
          }]
        }]
      },
      {
        "algorithm": "CMAC-AES",
        "revision": "1.0",
        "capabilities": [{
          "direction": ["gen", "ver"],
          "msgLen": [{
            "min": 0,
            "max": 65536,
            "increment": 8
          }],
          "keyLen": [128, 256],
          "macLen": [{
            "min": 32,
            "max": 128,
            "increment": 8
          }]
        }]
      }
    ])";
  return WriteReply(
      STDOUT_FILENO,
      Span<const uint8_t>(reinterpret_cast<const uint8_t *>(kConfig),
                          sizeof(kConfig) - 1));
}

template <uint8_t *(*OneShotHash)(const uint8_t *, size_t, uint8_t *),
          size_t DigestLength>
static bool Hash(const Span<const uint8_t> args[]) {
  uint8_t digest[DigestLength];
  OneShotHash(args[0].data(), args[0].size(), digest);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(digest));
}

static uint32_t GetIterations(const Span<const uint8_t> iterations_bytes) {
  uint32_t iterations;
  if (iterations_bytes.size() != sizeof(iterations)) {
    fprintf(stderr,
            "Expected %u-byte input for number of iterations, but found %u "
            "bytes.\n",
            static_cast<unsigned>(sizeof(iterations)),
            static_cast<unsigned>(iterations_bytes.size()));
    abort();
  }

  memcpy(&iterations, iterations_bytes.data(), sizeof(iterations));
  if (iterations == 0 || iterations == UINT32_MAX) {
    fprintf(stderr, "Invalid number of iterations: %x.\n",
            static_cast<unsigned>(iterations));
    abort();
  }

  return iterations;
}

template <int (*SetKey)(const uint8_t *key, unsigned bits, AES_KEY *out),
          void (*Block)(const uint8_t *in, uint8_t *out, const AES_KEY *key)>
static bool AES(const Span<const uint8_t> args[]) {
  AES_KEY key;
  if (SetKey(args[0].data(), args[0].size() * 8, &key) != 0) {
    return false;
  }
  if (args[1].size() % AES_BLOCK_SIZE != 0) {
    return false;
  }
  std::vector<uint8_t> result(args[1].begin(), args[1].end());
  const uint32_t iterations = GetIterations(args[2]);

  std::vector<uint8_t> prev_result;
  for (uint32_t j = 0; j < iterations; j++) {
    if (j == iterations - 1) {
      prev_result = result;
    }

    for (size_t i = 0; i < args[1].size(); i += AES_BLOCK_SIZE) {
      Block(result.data() + i, result.data() + i, &key);
    }
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(result),
                    Span<const uint8_t>(prev_result));
}

template <int (*SetKey)(const uint8_t *key, unsigned bits, AES_KEY *out),
          int Direction>
static bool AES_CBC(const Span<const uint8_t> args[]) {
  AES_KEY key;
  if (SetKey(args[0].data(), args[0].size() * 8, &key) != 0) {
    return false;
  }
  if (args[1].size() % AES_BLOCK_SIZE != 0 || args[1].empty() ||
      args[2].size() != AES_BLOCK_SIZE) {
    return false;
  }
  std::vector<uint8_t> input(args[1].begin(), args[1].end());
  std::vector<uint8_t> iv(args[2].begin(), args[2].end());
  const uint32_t iterations = GetIterations(args[3]);

  std::vector<uint8_t> result(input.size());
  std::vector<uint8_t> prev_result, prev_input;

  for (uint32_t j = 0; j < iterations; j++) {
    prev_result = result;
    if (j > 0) {
      if (Direction == AES_ENCRYPT) {
        iv = result;
      } else {
        iv = prev_input;
      }
    }

    // AES_cbc_encrypt will mutate the given IV, but we need it later.
    uint8_t iv_copy[AES_BLOCK_SIZE];
    memcpy(iv_copy, iv.data(), sizeof(iv_copy));
    AES_cbc_encrypt(input.data(), result.data(), input.size(), &key, iv_copy,
                    Direction);

    if (Direction == AES_DECRYPT) {
      prev_input = input;
    }

    if (j == 0) {
      input = iv;
    } else {
      input = prev_result;
    }
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(result),
                    Span<const uint8_t>(prev_result));
}

static bool AES_CTR(const Span<const uint8_t> args[]) {
  AES_KEY key;
  if (AES_set_encrypt_key(args[0].data(), args[0].size() * 8, &key) != 0) {
    return false;
  }
  if (args[2].size() != AES_BLOCK_SIZE) {
    return false;
  }
  uint8_t iv[AES_BLOCK_SIZE];
  memcpy(iv, args[2].data(), AES_BLOCK_SIZE);
  if (GetIterations(args[3]) != 1) {
    fprintf(stderr, "Multiple iterations of AES-CTR is not supported.\n");
    return false;
  }

  std::vector<uint8_t> out;
  out.resize(args[1].size());
  unsigned num = 0;
  uint8_t ecount_buf[AES_BLOCK_SIZE];
  AES_ctr128_encrypt(args[1].data(), out.data(), args[1].size(), &key, iv,
                     ecount_buf, &num);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

static bool AESGCMSetup(EVP_AEAD_CTX *ctx, Span<const uint8_t> tag_len_span,
                        Span<const uint8_t> key) {
  uint32_t tag_len_32;
  if (tag_len_span.size() != sizeof(tag_len_32)) {
    fprintf(stderr, "Tag size value is %u bytes, not an uint32_t\n",
            static_cast<unsigned>(tag_len_span.size()));
    return false;
  }
  memcpy(&tag_len_32, tag_len_span.data(), sizeof(tag_len_32));

  const EVP_AEAD *aead;
  switch (key.size()) {
    case 16:
      aead = EVP_aead_aes_128_gcm();
      break;
    case 24:
      aead = EVP_aead_aes_192_gcm();
      break;
    case 32:
      aead = EVP_aead_aes_256_gcm();
      break;
    default:
      fprintf(stderr, "Bad AES-GCM key length %u\n",
              static_cast<unsigned>(key.size()));
      return false;
  }

  if (!EVP_AEAD_CTX_init(ctx, aead, key.data(), key.size(), tag_len_32,
                         nullptr)) {
    fprintf(stderr, "Failed to setup AES-GCM with tag length %u\n",
            static_cast<unsigned>(tag_len_32));
    return false;
  }

  return true;
}

static bool AESCCMSetup(EVP_AEAD_CTX *ctx, Span<const uint8_t> tag_len_span,
                        Span<const uint8_t> key) {
  uint32_t tag_len_32;
  if (tag_len_span.size() != sizeof(tag_len_32)) {
    fprintf(stderr, "Tag size value is %u bytes, not an uint32_t\n",
            static_cast<unsigned>(tag_len_span.size()));
    return false;
  }
  memcpy(&tag_len_32, tag_len_span.data(), sizeof(tag_len_32));
  if (tag_len_32 != 4) {
    fprintf(stderr, "AES-CCM only supports 4-byte tags, but %u was requested\n",
            static_cast<unsigned>(tag_len_32));
    return false;
  }

  if (key.size() != 16) {
    fprintf(stderr,
            "AES-CCM only supports 128-bit keys, but %u bits were given\n",
            static_cast<unsigned>(key.size() * 8));
    return false;
  }

  if (!EVP_AEAD_CTX_init(ctx, EVP_aead_aes_128_ccm_bluetooth(), key.data(),
                         key.size(), tag_len_32, nullptr)) {
    fprintf(stderr, "Failed to setup AES-CCM with tag length %u\n",
            static_cast<unsigned>(tag_len_32));
    return false;
  }

  return true;
}

template <bool (*SetupFunc)(EVP_AEAD_CTX *ctx, Span<const uint8_t> tag_len_span,
                            Span<const uint8_t> key)>
static bool AEADSeal(const Span<const uint8_t> args[]) {
  Span<const uint8_t> tag_len_span = args[0];
  Span<const uint8_t> key = args[1];
  Span<const uint8_t> plaintext = args[2];
  Span<const uint8_t> nonce = args[3];
  Span<const uint8_t> ad = args[4];

  bssl::ScopedEVP_AEAD_CTX ctx;
  if (!SetupFunc(ctx.get(), tag_len_span, key)) {
    return false;
  }

  if (EVP_AEAD_MAX_OVERHEAD + plaintext.size() < EVP_AEAD_MAX_OVERHEAD) {
    return false;
  }
  std::vector<uint8_t> out(EVP_AEAD_MAX_OVERHEAD + plaintext.size());

  size_t out_len;
  if (!EVP_AEAD_CTX_seal(ctx.get(), out.data(), &out_len, out.size(),
                         nonce.data(), nonce.size(), plaintext.data(),
                         plaintext.size(), ad.data(), ad.size())) {
    return false;
  }

  out.resize(out_len);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

template <bool (*SetupFunc)(EVP_AEAD_CTX *ctx, Span<const uint8_t> tag_len_span,
                            Span<const uint8_t> key)>
static bool AEADOpen(const Span<const uint8_t> args[]) {
  Span<const uint8_t> tag_len_span = args[0];
  Span<const uint8_t> key = args[1];
  Span<const uint8_t> ciphertext = args[2];
  Span<const uint8_t> nonce = args[3];
  Span<const uint8_t> ad = args[4];

  bssl::ScopedEVP_AEAD_CTX ctx;
  if (!SetupFunc(ctx.get(), tag_len_span, key)) {
    return false;
  }

  std::vector<uint8_t> out(ciphertext.size());
  size_t out_len;
  uint8_t success_flag[1] = {0};

  if (!EVP_AEAD_CTX_open(ctx.get(), out.data(), &out_len, out.size(),
                         nonce.data(), nonce.size(), ciphertext.data(),
                         ciphertext.size(), ad.data(), ad.size())) {
    return WriteReply(STDOUT_FILENO, Span<const uint8_t>(success_flag),
                      Span<const uint8_t>());
  }

  out.resize(out_len);
  success_flag[0] = 1;
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(success_flag),
                    Span<const uint8_t>(out));
}

static bool AESPaddedKeyWrapSetup(AES_KEY *out, bool decrypt,
                                  Span<const uint8_t> key) {
  if ((decrypt ? AES_set_decrypt_key : AES_set_encrypt_key)(
          key.data(), key.size() * 8, out) != 0) {
    fprintf(stderr, "Invalid AES key length for AES-KW(P): %u\n",
            static_cast<unsigned>(key.size()));
    return false;
  }
  return true;
}

static bool AESKeyWrapSetup(AES_KEY *out, bool decrypt, Span<const uint8_t> key,
                            Span<const uint8_t> input) {
  if (!AESPaddedKeyWrapSetup(out, decrypt, key)) {
    return false;
  }

  if (input.size() % 8) {
    fprintf(stderr, "Invalid AES-KW input length: %u\n",
            static_cast<unsigned>(input.size()));
    return false;
  }

  return true;
}

static bool AESKeyWrapSeal(const Span<const uint8_t> args[]) {
  Span<const uint8_t> key = args[1];
  Span<const uint8_t> plaintext = args[2];

  AES_KEY aes;
  if (!AESKeyWrapSetup(&aes, /*decrypt=*/false, key, plaintext) ||
      plaintext.size() > INT_MAX - 8) {
    return false;
  }

  std::vector<uint8_t> out(plaintext.size() + 8);
  if (AES_wrap_key(&aes, /*iv=*/nullptr, out.data(), plaintext.data(),
                   plaintext.size()) != static_cast<int>(out.size())) {
    fprintf(stderr, "AES-KW failed\n");
    return false;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

static bool AESKeyWrapOpen(const Span<const uint8_t> args[]) {
  Span<const uint8_t> key = args[1];
  Span<const uint8_t> ciphertext = args[2];

  AES_KEY aes;
  if (!AESKeyWrapSetup(&aes, /*decrypt=*/true, key, ciphertext) ||
      ciphertext.size() < 8 ||
      ciphertext.size() > INT_MAX) {
    return false;
  }

  std::vector<uint8_t> out(ciphertext.size() - 8);
  uint8_t success_flag[1] = {0};
  if (AES_unwrap_key(&aes, /*iv=*/nullptr, out.data(), ciphertext.data(),
                     ciphertext.size()) != static_cast<int>(out.size())) {
    return WriteReply(STDOUT_FILENO, Span<const uint8_t>(success_flag),
                      Span<const uint8_t>());
  }

  success_flag[0] = 1;
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(success_flag),
                    Span<const uint8_t>(out));
}

static bool AESPaddedKeyWrapSeal(const Span<const uint8_t> args[]) {
  Span<const uint8_t> key = args[1];
  Span<const uint8_t> plaintext = args[2];

  AES_KEY aes;
  if (!AESPaddedKeyWrapSetup(&aes, /*decrypt=*/false, key) ||
      plaintext.size() + 15 < 15) {
    return false;
  }

  std::vector<uint8_t> out(plaintext.size() + 15);
  size_t out_len;
  if (!AES_wrap_key_padded(&aes, out.data(), &out_len, out.size(),
                           plaintext.data(), plaintext.size())) {
    fprintf(stderr, "AES-KWP failed\n");
    return false;
  }

  out.resize(out_len);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

static bool AESPaddedKeyWrapOpen(const Span<const uint8_t> args[]) {
  Span<const uint8_t> key = args[1];
  Span<const uint8_t> ciphertext = args[2];

  AES_KEY aes;
  if (!AESPaddedKeyWrapSetup(&aes, /*decrypt=*/true, key) ||
      ciphertext.size() % 8) {
    return false;
  }

  std::vector<uint8_t> out(ciphertext.size());
  size_t out_len;
  uint8_t success_flag[1] = {0};
  if (!AES_unwrap_key_padded(&aes, out.data(), &out_len, out.size(),
                             ciphertext.data(), ciphertext.size())) {
    return WriteReply(STDOUT_FILENO, Span<const uint8_t>(success_flag),
                      Span<const uint8_t>());
  }

  success_flag[0] = 1;
  out.resize(out_len);
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(success_flag),
                    Span<const uint8_t>(out));
}

template <bool Encrypt>
static bool TDES(const Span<const uint8_t> args[]) {
  const EVP_CIPHER *cipher = EVP_des_ede3();

  if (args[0].size() != 24) {
    fprintf(stderr, "Bad key length %u for 3DES.\n",
            static_cast<unsigned>(args[0].size()));
    return false;
  }
  bssl::ScopedEVP_CIPHER_CTX ctx;
  if (!EVP_CipherInit_ex(ctx.get(), cipher, nullptr, args[0].data(), nullptr,
                         Encrypt ? 1 : 0) ||
      !EVP_CIPHER_CTX_set_padding(ctx.get(), 0)) {
    return false;
  }

  if (args[1].size() % 8) {
    fprintf(stderr, "Bad input length %u for 3DES.\n",
            static_cast<unsigned>(args[1].size()));
    return false;
  }
  std::vector<uint8_t> result(args[1].begin(), args[1].end());

  const uint32_t iterations = GetIterations(args[2]);
  std::vector<uint8_t> prev_result, prev_prev_result;

  for (uint32_t j = 0; j < iterations; j++) {
    if (j == iterations - 1) {
      prev_result = result;
    } else if (iterations >= 2 && j == iterations - 2) {
      prev_prev_result = result;
    }

    int out_len;
    if (!EVP_CipherUpdate(ctx.get(), result.data(), &out_len, result.data(),
                          result.size()) ||
        out_len != static_cast<int>(result.size())) {
      return false;
    }
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(result),
                    Span<const uint8_t>(prev_result),
                    Span<const uint8_t>(prev_prev_result));
}

template <bool Encrypt>
static bool TDES_CBC(const Span<const uint8_t> args[]) {
  const EVP_CIPHER *cipher = EVP_des_ede3_cbc();

  if (args[0].size() != 24) {
    fprintf(stderr, "Bad key length %u for 3DES.\n",
            static_cast<unsigned>(args[0].size()));
    return false;
  }

  if (args[1].size() % 8 || args[1].size() == 0) {
    fprintf(stderr, "Bad input length %u for 3DES.\n",
            static_cast<unsigned>(args[1].size()));
    return false;
  }
  std::vector<uint8_t> input(args[1].begin(), args[1].end());

  if (args[2].size() != EVP_CIPHER_iv_length(cipher)) {
    fprintf(stderr, "Bad IV length %u for 3DES.\n",
            static_cast<unsigned>(args[2].size()));
    return false;
  }
  std::vector<uint8_t> iv(args[2].begin(), args[2].end());
  const uint32_t iterations = GetIterations(args[3]);

  std::vector<uint8_t> result(input.size());
  std::vector<uint8_t> prev_result, prev_prev_result;
  bssl::ScopedEVP_CIPHER_CTX ctx;
  if (!EVP_CipherInit_ex(ctx.get(), cipher, nullptr, args[0].data(), iv.data(),
                         Encrypt ? 1 : 0) ||
      !EVP_CIPHER_CTX_set_padding(ctx.get(), 0)) {
    return false;
  }

  for (uint32_t j = 0; j < iterations; j++) {
    prev_prev_result = prev_result;
    prev_result = result;

    int out_len, out_len2;
    if (!EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, nullptr, iv.data(),
                           -1) ||
        !EVP_CipherUpdate(ctx.get(), result.data(), &out_len, input.data(),
                          input.size()) ||
        !EVP_CipherFinal_ex(ctx.get(), result.data() + out_len, &out_len2) ||
        (out_len + out_len2) != static_cast<int>(result.size())) {
      return false;
    }

    if (Encrypt) {
      if (j == 0) {
        input = iv;
      } else {
        input = prev_result;
      }
      iv = result;
    } else {
      iv = input;
      input = result;
    }
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(result),
                    Span<const uint8_t>(prev_result),
                    Span<const uint8_t>(prev_prev_result));
}

template <const EVP_MD *HashFunc()>
static bool HMAC(const Span<const uint8_t> args[]) {
  const EVP_MD *const md = HashFunc();
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (::HMAC(md, args[1].data(), args[1].size(), args[0].data(), args[0].size(),
             digest, &digest_len) == nullptr) {
    return false;
  }
  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(digest, digest_len));
}

static bool DRBG(const Span<const uint8_t> args[]) {
  const auto out_len_bytes = args[0];
  const auto entropy = args[1];
  const auto personalisation = args[2];
  const auto additional_data1 = args[3];
  const auto additional_data2 = args[4];
  const auto nonce = args[5];

  uint32_t out_len;
  if (out_len_bytes.size() != sizeof(out_len) ||
      entropy.size() != CTR_DRBG_ENTROPY_LEN ||
      // nonces are not supported
      nonce.size() != 0) {
    return false;
  }
  memcpy(&out_len, out_len_bytes.data(), sizeof(out_len));
  if (out_len > (1 << 24)) {
    return false;
  }
  std::vector<uint8_t> out(out_len);

  CTR_DRBG_STATE drbg;
  if (!CTR_DRBG_init(&drbg, entropy.data(), personalisation.data(),
                     personalisation.size()) ||
      !CTR_DRBG_generate(&drbg, out.data(), out_len, additional_data1.data(),
                         additional_data1.size()) ||
      !CTR_DRBG_generate(&drbg, out.data(), out_len, additional_data2.data(),
                         additional_data2.size())) {
    return false;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(out));
}

static bool StringEq(Span<const uint8_t> a, const char *b) {
  const size_t len = strlen(b);
  return a.size() == len && memcmp(a.data(), b, len) == 0;
}

static bssl::UniquePtr<EC_KEY> ECKeyFromName(Span<const uint8_t> name) {
  int nid;
  if (StringEq(name, "P-224")) {
    nid = NID_secp224r1;
  } else if (StringEq(name, "P-256")) {
    nid = NID_X9_62_prime256v1;
  } else if (StringEq(name, "P-384")) {
    nid = NID_secp384r1;
  } else if (StringEq(name, "P-521")) {
    nid = NID_secp521r1;
  } else {
    return nullptr;
  }

  return bssl::UniquePtr<EC_KEY>(EC_KEY_new_by_curve_name(nid));
}

static std::vector<uint8_t> BIGNUMBytes(const BIGNUM *bn) {
  const size_t len = BN_num_bytes(bn);
  std::vector<uint8_t> ret(len);
  BN_bn2bin(bn, ret.data());
  return ret;
}

static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GetPublicKeyBytes(
    const EC_KEY *key) {
  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(key),
                                           EC_KEY_get0_public_key(key), x.get(),
                                           y.get(), /*ctx=*/nullptr)) {
    abort();
  }

  std::vector<uint8_t> x_bytes = BIGNUMBytes(x.get());
  std::vector<uint8_t> y_bytes = BIGNUMBytes(y.get());

  return std::make_pair(std::move(x_bytes), std::move(y_bytes));
}

static bool ECDSAKeyGen(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  if (!key || !EC_KEY_generate_key_fips(key.get())) {
    return false;
  }

  const auto pub_key = GetPublicKeyBytes(key.get());
  std::vector<uint8_t> d_bytes =
      BIGNUMBytes(EC_KEY_get0_private_key(key.get()));

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(d_bytes),
                    Span<const uint8_t>(pub_key.first),
                    Span<const uint8_t>(pub_key.second));
}

static bssl::UniquePtr<BIGNUM> BytesToBIGNUM(Span<const uint8_t> bytes) {
  bssl::UniquePtr<BIGNUM> bn(BN_new());
  BN_bin2bn(bytes.data(), bytes.size(), bn.get());
  return bn;
}

static bool ECDSAKeyVer(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  if (!key) {
    return false;
  }

  bssl::UniquePtr<BIGNUM> x(BytesToBIGNUM(args[1]));
  bssl::UniquePtr<BIGNUM> y(BytesToBIGNUM(args[2]));

  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(EC_KEY_get0_group(key.get())));
  uint8_t reply[1];
  if (!EC_POINT_set_affine_coordinates_GFp(EC_KEY_get0_group(key.get()),
                                           point.get(), x.get(), y.get(),
                                           /*ctx=*/nullptr) ||
      !EC_KEY_set_public_key(key.get(), point.get()) ||
      !EC_KEY_check_fips(key.get())) {
    reply[0] = 0;
  } else {
    reply[0] = 1;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(reply));
}

static const EVP_MD *HashFromName(Span<const uint8_t> name) {
  if (StringEq(name, "SHA2-224")) {
    return EVP_sha224();
  } else if (StringEq(name, "SHA2-256")) {
    return EVP_sha256();
  } else if (StringEq(name, "SHA2-384")) {
    return EVP_sha384();
  } else if (StringEq(name, "SHA2-512")) {
    return EVP_sha512();
  } else {
    return nullptr;
  }
}

static bool ECDSASigGen(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  bssl::UniquePtr<BIGNUM> d = BytesToBIGNUM(args[1]);
  const EVP_MD *hash = HashFromName(args[2]);
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!key || !hash ||
      !EVP_Digest(args[3].data(), args[3].size(), digest, &digest_len, hash,
                  /*impl=*/nullptr) ||
      !EC_KEY_set_private_key(key.get(), d.get())) {
    return false;
  }

  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_do_sign(digest, digest_len, key.get()));
  if (!sig) {
    return false;
  }

  std::vector<uint8_t> r_bytes(BIGNUMBytes(sig->r));
  std::vector<uint8_t> s_bytes(BIGNUMBytes(sig->s));

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(r_bytes),
                    Span<const uint8_t>(s_bytes));
}

static bool ECDSASigVer(const Span<const uint8_t> args[]) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromName(args[0]);
  const EVP_MD *hash = HashFromName(args[1]);
  auto msg = args[2];
  bssl::UniquePtr<BIGNUM> x(BytesToBIGNUM(args[3]));
  bssl::UniquePtr<BIGNUM> y(BytesToBIGNUM(args[4]));
  bssl::UniquePtr<BIGNUM> r(BytesToBIGNUM(args[5]));
  bssl::UniquePtr<BIGNUM> s(BytesToBIGNUM(args[6]));
  ECDSA_SIG sig;
  sig.r = r.get();
  sig.s = s.get();

  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!key || !hash ||
      !EVP_Digest(msg.data(), msg.size(), digest, &digest_len, hash,
                  /*impl=*/nullptr)) {
    return false;
  }

  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(EC_KEY_get0_group(key.get())));
  uint8_t reply[1];
  if (!EC_POINT_set_affine_coordinates_GFp(EC_KEY_get0_group(key.get()),
                                           point.get(), x.get(), y.get(),
                                           /*ctx=*/nullptr) ||
      !EC_KEY_set_public_key(key.get(), point.get()) ||
      !EC_KEY_check_fips(key.get()) ||
      !ECDSA_do_verify(digest, digest_len, &sig, key.get())) {
    reply[0] = 0;
  } else {
    reply[0] = 1;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(reply));
}

static bool CMAC_AES(const Span<const uint8_t> args[]) {
  uint8_t mac[16];
  if (!AES_CMAC(mac, args[1].data(), args[1].size(), args[2].data(),
                args[2].size())) {
    return false;
  }

  uint32_t mac_len;
  if (args[0].size() != sizeof(mac_len)) {
    return false;
  }
  memcpy(&mac_len, args[0].data(), sizeof(mac_len));
  if (mac_len > sizeof(mac)) {
    return false;
  }

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(mac, mac_len));
}

static std::map<unsigned, bssl::UniquePtr<RSA>>& CachedRSAKeys() {
  static std::map<unsigned, bssl::UniquePtr<RSA>> keys;
  return keys;
}

static RSA* GetRSAKey(unsigned bits) {
  auto it = CachedRSAKeys().find(bits);
  if (it != CachedRSAKeys().end()) {
    return it->second.get();
  }

  bssl::UniquePtr<RSA> key(RSA_new());
  if (!RSA_generate_key_fips(key.get(), bits, nullptr)) {
    abort();
  }

  RSA *const ret = key.get();
  CachedRSAKeys().emplace(static_cast<unsigned>(bits), std::move(key));

  return ret;
}

static bool RSAKeyGen(const Span<const uint8_t> args[]) {
  uint32_t bits;
  if (args[0].size() != sizeof(bits)) {
    return false;
  }
  memcpy(&bits, args[0].data(), sizeof(bits));

  bssl::UniquePtr<RSA> key(RSA_new());
  if (!RSA_generate_key_fips(key.get(), bits, nullptr)) {
    fprintf(stderr, "RSA_generate_key_fips failed for modulus length %u.\n",
            bits);
    return false;
  }

  const BIGNUM *n, *e, *d, *p, *q;
  RSA_get0_key(key.get(), &n, &e, &d);
  RSA_get0_factors(key.get(), &p, &q);

  if (!WriteReply(STDOUT_FILENO, BIGNUMBytes(e), BIGNUMBytes(p), BIGNUMBytes(q),
                  BIGNUMBytes(n), BIGNUMBytes(d))) {
    return false;
  }

  CachedRSAKeys().emplace(static_cast<unsigned>(bits), std::move(key));
  return true;
}

template<const EVP_MD *(MDFunc)(), bool UsePSS>
static bool RSASigGen(const Span<const uint8_t> args[]) {
  uint32_t bits;
  if (args[0].size() != sizeof(bits)) {
    return false;
  }
  memcpy(&bits, args[0].data(), sizeof(bits));
  const Span<const uint8_t> msg = args[1];

  RSA *const key = GetRSAKey(bits);
  const EVP_MD *const md = MDFunc();
  uint8_t digest_buf[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!EVP_Digest(msg.data(), msg.size(), digest_buf, &digest_len, md, NULL)) {
    return false;
  }

  std::vector<uint8_t> sig(RSA_size(key));
  size_t sig_len;
  if (UsePSS) {
    if (!RSA_sign_pss_mgf1(key, &sig_len, sig.data(), sig.size(), digest_buf,
                           digest_len, md, md, -1)) {
      return false;
    }
  } else {
    unsigned sig_len_u;
    if (!RSA_sign(EVP_MD_type(md), digest_buf, digest_len, sig.data(),
                  &sig_len_u, key)) {
      return false;
    }
    sig_len = sig_len_u;
  }

  sig.resize(sig_len);

  return WriteReply(STDOUT_FILENO, BIGNUMBytes(RSA_get0_n(key)),
                    BIGNUMBytes(RSA_get0_e(key)), sig);
}

template<const EVP_MD *(MDFunc)(), bool UsePSS>
static bool RSASigVer(const Span<const uint8_t> args[]) {
  const Span<const uint8_t> n_bytes = args[0];
  const Span<const uint8_t> e_bytes = args[1];
  const Span<const uint8_t> msg = args[2];
  const Span<const uint8_t> sig = args[3];

  BIGNUM *n = BN_new();
  BIGNUM *e = BN_new();
  bssl::UniquePtr<RSA> key(RSA_new());
  if (!BN_bin2bn(n_bytes.data(), n_bytes.size(), n) ||
      !BN_bin2bn(e_bytes.data(), e_bytes.size(), e) ||
      !RSA_set0_key(key.get(), n, e, /*d=*/nullptr)) {
    return false;
  }

  const EVP_MD *const md = MDFunc();
  uint8_t digest_buf[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!EVP_Digest(msg.data(), msg.size(), digest_buf, &digest_len, md, NULL)) {
    return false;
  }

  uint8_t ok;
  if (UsePSS) {
    ok = RSA_verify_pss_mgf1(key.get(), digest_buf, digest_len, md, md, -1,
                             sig.data(), sig.size());
  } else {
    ok = RSA_verify(EVP_MD_type(md), digest_buf, digest_len, sig.data(),
                    sig.size(), key.get());
  }
  ERR_clear_error();

  return WriteReply(STDOUT_FILENO, Span<const uint8_t>(&ok, 1));
}

static constexpr struct {
  const char name[kMaxNameLength + 1];
  uint8_t expected_args;
  bool (*handler)(const Span<const uint8_t>[]);
} kFunctions[] = {
    {"getConfig", 0, GetConfig},
    {"SHA-1", 1, Hash<SHA1, SHA_DIGEST_LENGTH>},
    {"SHA2-224", 1, Hash<SHA224, SHA224_DIGEST_LENGTH>},
    {"SHA2-256", 1, Hash<SHA256, SHA256_DIGEST_LENGTH>},
    {"SHA2-384", 1, Hash<SHA384, SHA256_DIGEST_LENGTH>},
    {"SHA2-512", 1, Hash<SHA512, SHA512_DIGEST_LENGTH>},
    {"AES/encrypt", 3, AES<AES_set_encrypt_key, AES_encrypt>},
    {"AES/decrypt", 3, AES<AES_set_decrypt_key, AES_decrypt>},
    {"AES-CBC/encrypt", 4, AES_CBC<AES_set_encrypt_key, AES_ENCRYPT>},
    {"AES-CBC/decrypt", 4, AES_CBC<AES_set_decrypt_key, AES_DECRYPT>},
    {"AES-CTR/encrypt", 3, AES_CTR},
    {"AES-CTR/decrypt", 3, AES_CTR},
    {"AES-GCM/seal", 5, AEADSeal<AESGCMSetup>},
    {"AES-GCM/open", 5, AEADOpen<AESGCMSetup>},
    {"AES-KW/seal", 5, AESKeyWrapSeal},
    {"AES-KW/open", 5, AESKeyWrapOpen},
    {"AES-KWP/seal", 5, AESPaddedKeyWrapSeal},
    {"AES-KWP/open", 5, AESPaddedKeyWrapOpen},
    {"AES-CCM/seal", 5, AEADSeal<AESCCMSetup>},
    {"AES-CCM/open", 5, AEADOpen<AESCCMSetup>},
    {"3DES-ECB/encrypt", 3, TDES<true>},
    {"3DES-ECB/decrypt", 3, TDES<false>},
    {"3DES-CBC/encrypt", 4, TDES_CBC<true>},
    {"3DES-CBC/decrypt", 4, TDES_CBC<false>},
    {"HMAC-SHA-1", 2, HMAC<EVP_sha1>},
    {"HMAC-SHA2-224", 2, HMAC<EVP_sha224>},
    {"HMAC-SHA2-256", 2, HMAC<EVP_sha256>},
    {"HMAC-SHA2-384", 2, HMAC<EVP_sha384>},
    {"HMAC-SHA2-512", 2, HMAC<EVP_sha512>},
    {"ctrDRBG/AES-256", 6, DRBG},
    {"ECDSA/keyGen", 1, ECDSAKeyGen},
    {"ECDSA/keyVer", 3, ECDSAKeyVer},
    {"ECDSA/sigGen", 4, ECDSASigGen},
    {"ECDSA/sigVer", 7, ECDSASigVer},
    {"CMAC-AES", 3, CMAC_AES},
    {"RSA/keyGen", 1, RSAKeyGen},
    {"RSA/sigGen/SHA2-224/pkcs1v1.5", 2, RSASigGen<EVP_sha224, false>},
    {"RSA/sigGen/SHA2-256/pkcs1v1.5", 2, RSASigGen<EVP_sha256, false>},
    {"RSA/sigGen/SHA2-384/pkcs1v1.5", 2, RSASigGen<EVP_sha384, false>},
    {"RSA/sigGen/SHA2-512/pkcs1v1.5", 2, RSASigGen<EVP_sha512, false>},
    {"RSA/sigGen/SHA-1/pkcs1v1.5", 2, RSASigGen<EVP_sha1, false>},
    {"RSA/sigGen/SHA2-224/pss", 2, RSASigGen<EVP_sha224, true>},
    {"RSA/sigGen/SHA2-256/pss", 2, RSASigGen<EVP_sha256, true>},
    {"RSA/sigGen/SHA2-384/pss", 2, RSASigGen<EVP_sha384, true>},
    {"RSA/sigGen/SHA2-512/pss", 2, RSASigGen<EVP_sha512, true>},
    {"RSA/sigGen/SHA-1/pss", 2, RSASigGen<EVP_sha1, true>},
    {"RSA/sigVer/SHA2-224/pkcs1v1.5", 4, RSASigVer<EVP_sha224, false>},
    {"RSA/sigVer/SHA2-256/pkcs1v1.5", 4, RSASigVer<EVP_sha256, false>},
    {"RSA/sigVer/SHA2-384/pkcs1v1.5", 4, RSASigVer<EVP_sha384, false>},
    {"RSA/sigVer/SHA2-512/pkcs1v1.5", 4, RSASigVer<EVP_sha512, false>},
    {"RSA/sigVer/SHA-1/pkcs1v1.5", 4, RSASigVer<EVP_sha1, false>},
    {"RSA/sigVer/SHA2-224/pss", 4, RSASigVer<EVP_sha224, true>},
    {"RSA/sigVer/SHA2-256/pss", 4, RSASigVer<EVP_sha256, true>},
    {"RSA/sigVer/SHA2-384/pss", 4, RSASigVer<EVP_sha384, true>},
    {"RSA/sigVer/SHA2-512/pss", 4, RSASigVer<EVP_sha512, true>},
    {"RSA/sigVer/SHA-1/pss", 4, RSASigVer<EVP_sha1, true>},
};

int main() {
  uint32_t nums[1 + kMaxArgs];
  std::unique_ptr<uint8_t[]> buf;
  size_t buf_len = 0;
  Span<const uint8_t> args[kMaxArgs];

  for (;;) {
    if (!ReadAll(STDIN_FILENO, nums, sizeof(uint32_t) * 2)) {
      return 1;
    }

    const size_t num_args = nums[0];
    if (num_args == 0) {
      fprintf(stderr, "Invalid, zero-argument operation requested.\n");
      return 2;
    } else if (num_args > kMaxArgs) {
      fprintf(stderr,
              "Operation requested with %zu args, but %zu is the limit.\n",
              num_args, kMaxArgs);
      return 2;
    }

    if (num_args > 1 &&
        !ReadAll(STDIN_FILENO, &nums[2], sizeof(uint32_t) * (num_args - 1))) {
      return 1;
    }

    size_t need = 0;
    for (size_t i = 0; i < num_args; i++) {
      const size_t arg_length = nums[i + 1];
      if (i == 0 && arg_length > kMaxNameLength) {
        fprintf(stderr,
                "Operation with name of length %zu exceeded limit of %zu.\n",
                arg_length, kMaxNameLength);
        return 2;
      } else if (arg_length > kMaxArgLength) {
        fprintf(
            stderr,
            "Operation with argument of length %zu exceeded limit of %zu.\n",
            arg_length, kMaxArgLength);
        return 2;
      }

      // static_assert around kMaxArgs etc enforces that this doesn't overflow.
      need += arg_length;
    }

    if (need > buf_len) {
      size_t alloced = need + (need >> 1);
      if (alloced < need) {
        abort();
      }
      buf.reset(new uint8_t[alloced]);
      buf_len = alloced;
    }

    if (!ReadAll(STDIN_FILENO, buf.get(), need)) {
      return 1;
    }

    size_t offset = 0;
    for (size_t i = 0; i < num_args; i++) {
      args[i] = Span<const uint8_t>(&buf[offset], nums[i + 1]);
      offset += nums[i + 1];
    }

    bool found = false;
    for (const auto &func : kFunctions) {
      if (args[0].size() == strlen(func.name) &&
          memcmp(args[0].data(), func.name, args[0].size()) == 0) {
        if (num_args - 1 != func.expected_args) {
          fprintf(stderr,
                  "\'%s\' operation received %zu arguments but expected %u.\n",
                  func.name, num_args - 1, func.expected_args);
          return 2;
        }

        if (!func.handler(&args[1])) {
          fprintf(stderr, "\'%s\' operation failed.\n", func.name);
          return 4;
        }

        found = true;
        break;
      }
    }

    if (!found) {
      const std::string name(reinterpret_cast<const char *>(args[0].data()),
                             args[0].size());
      fprintf(stderr, "Unknown operation: %s\n", name.c_str());
      return 3;
    }
  }
}
