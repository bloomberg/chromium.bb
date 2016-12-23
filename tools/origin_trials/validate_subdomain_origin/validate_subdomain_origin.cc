// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

using net::registry_controlled_domains::GetCanonicalHostRegistryLength;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES;

// Return codes used by this utility.
static const int kStatusValid = 0;
static const int kStatusInvalidOrigin = 1;
static const int kStatusInPublicSuffixList = 2;
static const int kStatusError = 3;
static const int kStatusIPAddress = 4;

// Causes the app to suppress logging/verbose output
static const char kOptionQuiet[] = "quiet";

void PrintHelp() {
  std::cerr
      << "Usage:\n"
         "  validate_subdomain_origin [--quiet] <origin>\n"
         "    Checks that the origin can be used in a token that matches\n"
         "    subdomains, returning 0 when the origin is valid for such use.\n"
         "    The origin may be specified as an url (e.g. "
         "'https://example.com'),\n"
         "    or as bare hostname (e.g. 'example.com').\n"
         "    The caller is responsible for ensuring that a well-formed "
         "origin\n"
         "    is provided, there are no checks for correctness.\n"
         "    Pass \"--quiet\" to suppress any output.\n";
}

void PrintValidHost(const base::StringPiece origin) {
  std::cout << "validate_subdomain_origin: Valid origin - " << origin
            << std::endl;
}

void PrintIPAddressNotSupported(const base::StringPiece origin) {
  std::cout << "validate_subdomain_origin: Origin is an IP address - "
            << origin << std::endl;
}

void PrintInvalidUrl(const base::StringPiece origin) {
  std::cout << "validate_subdomain_origin: Invalid url format for origin - "
            << origin << std::endl;
}

void PrintInvalidOrigin(const base::StringPiece origin) {
  std::cout << "validate_subdomain_origin: Invalid origin - " << origin
            << std::endl;
}

void PrintInPublicSuffixList(const base::StringPiece origin) {
  std::cout << "validate_subdomain_origin: Origin in Public Suffix List - "
            << origin << std::endl;
}

int CheckOrigin(const base::StringPiece origin, bool verbose) {
  base::StringPiece host;
  std::unique_ptr<std::string> canon_host = nullptr;

  // Validate the origin, which may be provided as an url (with scheme prefix),
  // or just as a hostname. Regardless of format, if the origin is identified
  // as an IP address, that is valid for subdomain tokens.
  GURL gurl(origin);
  if (gurl.is_valid()) {
    if (gurl.HostIsIPAddress()) {
      if (verbose) {
        PrintIPAddressNotSupported(origin);
      }
      return kStatusIPAddress;
    }
    host = gurl.host_piece();
  } else {
    // Doesn't look like an url, try the origin as a hostname
    url::CanonHostInfo host_info;
    canon_host = base::MakeUnique<std::string>(
        net::CanonicalizeHost(origin, &host_info));
    if (canon_host->empty()) {
      if (verbose) {
        PrintInvalidOrigin(origin);
      }
      return kStatusInvalidOrigin;
    }
    if (host_info.IsIPAddress()) {
      if (verbose) {
        PrintIPAddressNotSupported(origin);
      }
      return kStatusIPAddress;
    }
    host.set(canon_host->c_str());
  }

  size_t registry_length = GetCanonicalHostRegistryLength(
      host, INCLUDE_UNKNOWN_REGISTRIES, INCLUDE_PRIVATE_REGISTRIES);

  if (registry_length > 0) {
    // Host has at least one subcomponent (e.g. a.b), and the host is not just
    // a registry (e.g. co.uk).
    if (verbose) {
      PrintValidHost(origin);
    }
    return kStatusValid;
  }

  // If registry length is 0, then the host may be a registry, or it has no
  // subcomponents. If there are subcomponents, the host must be a registry,
  // which makes it invalid.
  if (host.find('.') != std::string::npos) {
    if (verbose) {
      PrintInPublicSuffixList(origin);
    }
    return kStatusInPublicSuffixList;
  }

  // There are no subcomponents, but still don't know if this a registry
  // (e.g. host = "com"), or a private/internal address (e.g. host = "bar").
  // Test by adding a subcomponent, and re-checking the registry. In this case,
  // exclude unknown registries. That means that "prefix.com" will match the
  // "com" registry, and return a non-zero length. Conversely, "prefix.bar" will
  // not match any known registry, and return a zero length.
  std::string test_host("prefix.");
  test_host.append(host.as_string());

  size_t test_registry_length = GetCanonicalHostRegistryLength(
      test_host, EXCLUDE_UNKNOWN_REGISTRIES, INCLUDE_PRIVATE_REGISTRIES);
  if (test_registry_length > 0) {
    if (verbose) {
      PrintInPublicSuffixList(origin);
    }
    return kStatusInPublicSuffixList;
  }

  if (verbose) {
    PrintValidHost(origin);
  }
  return kStatusValid;
}

int main(int argc, const char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = parsed_command_line.GetArgs();
  bool quiet = parsed_command_line.HasSwitch(kOptionQuiet);
  if (args.size() == 1) {
#if defined(OS_WIN)
    std::string origin = base::UTF16ToUTF8(args[0]);
    return CheckOrigin(origin, !quiet);
#else
    return CheckOrigin(args[0], !quiet);
#endif
  }

  PrintHelp();
  return kStatusError;
}
