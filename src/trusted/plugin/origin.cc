/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"

#include <stdio.h>

#include <cctype>
#include <stdlib.h>
#include <algorithm>

#include "base/basictypes.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/origin.h"

#define NACL_SELENIUM_TEST "NACL_DISABLE_SECURITY_FOR_SELENIUM_TEST"

#ifdef  ORIGIN_DEBUG
# define dprintf(alist) do { printf alist; } while (0)
#else
# define dprintf(alist) do { if (0) { printf alist; } } while (0)
#endif

namespace nacl {

  nacl::string UrlToOrigin(nacl::string url) {
    nacl::string::iterator it = find(url.begin(), url.end(), ':');
    if (url.end() == it) {
      dprintf(("no protospec separator found\n"));
      return "";
    }
    for (int num_slashes = 0; num_slashes < 3; ++num_slashes) {
      it = find(it + 1, url.end(), '/');
      if (url.end() == it) {
        dprintf(("no start of pathspec found\n"));
        return "";
      }
    }

    nacl::string origin(url.begin(), it);

    //
    // Domain names are in ascii and case insensitive, so we can
    // canonicalize to all lower case.  NB: Internationalizing Domain
    // Names in Applications (IDNA) encodes unicode in this reduced
    // alphabet.
    //
    for (it = origin.begin(); origin.end() != it; ++it) {
      *it = tolower(*it);
    }
    //
    // cannonicalize empty hostname as "localhost"
    //
    if ("file://" == origin) {
      origin = "file://localhost";
    }
    return origin;
  }

// For now, if NACL_STANDALONE is defined, we are just checking that
// NaCl modules are local, or on code.google.com.  Beware NaCl modules
// in the browser cache!  (NACL_STANDALONE is defined when being built
// as a browser plugin.)
//
// Eventually, after sufficient security testing, we will switch over
// to a more permissive security policy.
//
// When NACL_STANDALONE is not defined, we use a more permissive
// security policy (which approximates that of the final policy for
// the release version of NaCl): the "http" and "https" protocols are
// white-listed, and that's it.  That is, any host is okay as long as
// the content is served using http or https.  We must still disallow
// "file" access due to cache poisoning attacks (see below), but
// rather than having a black list, we use a whitelist of protocols,
// in case that there are other, new protocols that exhibit a similar
// property as "file" or "ftp".

  bool OriginIsInWhitelist(nacl::string origin) {
#if NACL_STANDALONE
    static char const *allowed_origin[] = {
      // do *NOT* add in file://localhost as a way to get old tests to
      // work.  The file://localhost was only for early stage testing
      // -- having it can be a security problem if an adversary can
      // guess browser cache file names.

      "http://localhost",
      "http://localhost:80",
      "http://localhost:5103",
#if 0
      "http://code.google.com",  // for demos hosted on project website
#endif
    };
    dprintf(("OriginIsInWhitelist(%s)\n", origin.c_str()));
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(allowed_origin); ++i) {
      if (origin == allowed_origin[i]) {
        dprintf((" found at position %"NACL_PRIdS"\n", i));
        return true;
      }
    }
    // We disregard the origin whitelist when running Selenium tests.
    // The code below is temporary since eventually we will drop the
    // whitelist completely.
#if NACL_WINDOWS
    char buffer[2];
    size_t required_buffer_size;
    if (0 == getenv_s(&required_buffer_size, buffer, 2, NACL_SELENIUM_TEST)) {
      dprintf((" selenium test!\n"));
      return true;
    }
#else
    if (NULL != getenv(NACL_SELENIUM_TEST)) {
      dprintf((" selenium test!\n"));
      return true;
    }
#endif

    dprintf((" FAILED!\n"));
    return false;
#else  // NACL_STANDALONE

    // All hosts are allowed, but we whitelist protocols.  Some
    // protocols, such as "file" may be used in a cache-poisoning
    // attack -- or where the user is tricked into downloading a file
    // to the user's download directory using a file name that is
    // suggested by the web site -- where an HTML page guesses the
    // file name of browser cache entries -- or Download directory
    // entry -- and loads both an HTML page and a nexe from the cache.
    // This way both has the same origin, and would be given access
    // rights to access other URLs from the same origin (the entire
    // filesystem) due to the same-origin policy.
    //
    // We are still potentially vulnerable to DNS rebinding,
    // anti-anti-DNS-rebindng, ..., (anti-)^(2n)-rebinding attacks,
    // etc. but we assume that the browser's defense mechanisms --
    // (anti-)^(2n+1)-rebinding attack mechanisms -- take care of
    // that.

    static char const *allowed_protocols[] = {
      "http",
      "https",
    };

    nacl::string::iterator it = find(origin.begin(), origin.end(), ':');
    if (origin.end() == it) {
      dprintf(("no protospec separator found\n"));
      return false;
    }

    nacl::string proto(origin.begin(), it);

    dprintf(("OriginIsInWhitelist, protocol(%s)\n", proto.c_str()));
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(allowed_protocols); ++i) {
      if (proto == allowed_protocols[i]) {
        dprintf((" found protocol at position %"NACL_PRIdS"\n", i));
        return true;
      }
    }

    return false;
#endif  // NACL_STANDALONE
  }
}
