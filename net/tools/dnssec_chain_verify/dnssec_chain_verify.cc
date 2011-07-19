// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/at_exit.h"
#include "net/base/dns_util.h"
#include "net/base/dnssec_chain_verifier.h"

static int usage(const char* argv0) {
  fprintf(stderr, "Usage: %s [--ignore-timestamps] <target domain> "
                  "<input file>\n", argv0);
  return 1;
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;

  if (argc < 3)
    return usage(argv[0]);

  const char* target = NULL;
  const char* infilename = NULL;
  bool ignore_timestamps = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--ignore-timestamps") == 0) {
      ignore_timestamps = true;
    } else if (!target) {
      target = argv[i];
    } else if (!infilename) {
      infilename = argv[i];
    } else {
      return usage(argv[0]);
    }
  }

  if (!target || !infilename)
    return usage(argv[0]);

  FILE* infile = fopen(infilename, "r");
  if (!infile) {
    perror("open");
    return usage(argv[0]);
  }

  fseek(infile, 0, SEEK_END);
  unsigned long inlen = ftell(infile);
  fseek(infile, 0, SEEK_SET);

  char* const input = (char *) malloc(inlen);
  if (fread(input, inlen, 1, infile) != 1) {
    perror("read");
    return 1;
  }

  std::string target_dns;
  if (!net::DNSDomainFromDot(target, &target_dns)) {
    fprintf(stderr, "Not a valid DNS name: %s\n", target);
    return usage(argv[0]);
  }

  net::DNSSECChainVerifier verifier(target_dns,
                                    base::StringPiece(input, inlen));
  if (ignore_timestamps)
    verifier.IgnoreTimestamps();
  net::DNSSECChainVerifier::Error err = verifier.Verify();
  const char* err_str;
  switch (err) {
    case net::DNSSECChainVerifier::BAD_DATA:
      err_str = "Bad data";
      break;
    case net::DNSSECChainVerifier::UNKNOWN_ROOT_KEY:
      err_str = "Unknown root key";
      break;
    case net::DNSSECChainVerifier::UNKNOWN_DIGEST:
      err_str = "Unknown digest";
      break;
    case net::DNSSECChainVerifier::UNKNOWN_TERMINAL_RRTYPE:
      err_str = "Unknown terminal RR type";
      break;
    case net::DNSSECChainVerifier::BAD_SIGNATURE:
      err_str = "Bad signature";
      break;
    case net::DNSSECChainVerifier::NO_DS_LINK:
      err_str = "No DS link";
      break;
    case net::DNSSECChainVerifier::OFF_COURSE:
      err_str = "Off course";
      break;
    case net::DNSSECChainVerifier::BAD_TARGET:
      err_str = "Bad target";
      break;
    default:
      err_str = "Unknown";
      break;
  }

  if (err != net::DNSSECChainVerifier::OK) {
    fprintf(stderr, "Chain error: %s (%d)\n", err_str, (int) err);
    return 1;
  }

  fprintf(stderr, "Chain good: rrtype:%d\n", verifier.rrtype());
  return 0;
}
