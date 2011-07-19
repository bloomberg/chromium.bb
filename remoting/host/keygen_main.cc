// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a tool used by register_host.py to generate RSA keypair. It just
// generates new keys and prints them on stdout.
//
// This code will need to be integrated with the host registration UI.

#include <stdio.h>

#include <vector>

#include "base/at_exit.h"
#include "base/base64.h"
#include "crypto/rsa_private_key.h"
#include "base/memory/scoped_ptr.h"

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  scoped_ptr<crypto::RSAPrivateKey> key(crypto::RSAPrivateKey::Create(2048));

  std::vector<uint8> private_key_buf;
  key->ExportPrivateKey(&private_key_buf);
  std::string private_key_str(private_key_buf.begin(), private_key_buf.end());
  std::string private_key_base64;
  base::Base64Encode(private_key_str, &private_key_base64);
  printf("%s\n", private_key_base64.c_str());

  std::vector<uint8> public_key_buf;
  key->ExportPublicKey(&public_key_buf);
  std::string public_key_str(public_key_buf.begin(), public_key_buf.end());
  std::string public_key_base64;
  base::Base64Encode(public_key_str, &public_key_base64);
  printf("%s\n", public_key_base64.c_str());
}
