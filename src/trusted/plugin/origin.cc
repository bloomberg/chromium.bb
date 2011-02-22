/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/plugin/origin.h"

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cctype>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"

#ifdef  ORIGIN_DEBUG
# define dprintf(alist) do { printf alist; } while (0)
#else
# define dprintf(alist) do { if (0) { printf alist; } } while (0)
#endif

namespace nacl {

// for http/https protocols, an URL is
//
// <protospec>://[<user>[:<pass>]@]<host>[:<port>]/<pathspec>
//
// where <protospec> is either "http" or "https", and none of the
// other meta-syntactic variables' values contain the separater
// characters ":", "@", or "/".
//
// when <protospec> is "chrome-extension", an URL is
//
// chrome-extension://<hash>/<pathspec>
//
// where <hash> is 32 lower-case ascii characters from a-p, and
// <pathspec> is never empty.

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
    // NB: If the <host> part isn't really a host name, e.g., when
    // <protospec> is "chrome-extension", then we may be downcasing
    // other data.  In the case of "chrome-extension", this is <hash>
    // as above, so it should already be lower case characters.
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

// We use a more permissive security policy (which approximates that
// of the final policy for the release version of NaCl): the "http"
// and "https" protocols are white-listed, and that's it.  That is,
// any host is okay as long as the content is served using http or
// https.  We must still disallow "file" access due to cache poisoning
// attacks (see below), but rather than having a black list, we use a
// whitelist of protocols, in case that there are other, new protocols
// that exhibit a similar property as "file" or "ftp".

  bool OriginIsInWhitelist(nacl::string origin) {
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
    // anti-anti-DNS-rebinding, ..., (anti-)^(2n)-rebinding attacks,
    // etc. but we assume that the browser's defense mechanisms --
    // (anti-)^(2n+1)-rebinding attack mechanisms -- take care of
    // that.

    static char const *allowed_protocols[] = {
      "http",
      "https",
      "chrome-extension",
    };

    nacl::string::iterator it = find(origin.begin(), origin.end(), ':');
    if (origin.end() == it) {
      dprintf(("no protospec separator found\n"));
      return false;
    }

    nacl::string proto(origin.begin(), it);

    dprintf(("OriginIsInWhitelist, protocol(%s)\n", proto.c_str()));
    for (size_t i = 0; i < NACL_ARRAY_SIZE_UNSAFE(allowed_protocols); ++i) {
      if (proto == allowed_protocols[i]) {
        dprintf((" found protocol at position %"NACL_PRIdS"\n", i));
        return true;
      }
    }

    return false;
  }
}
