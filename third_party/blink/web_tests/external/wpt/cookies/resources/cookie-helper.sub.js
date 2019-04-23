// Set up exciting global variables for cookie tests.
(_ => {
  var HOST = "{{host}}";
  var SECURE_PORT = ":{{ports[https][0]}}";
  var PORT = ":{{ports[http][0]}}";
  var CROSS_ORIGIN_HOST = "{{hosts[alt][]}}";
  var SECURE_CROSS_ORIGIN_HOST = "{{hosts[alt][]}}";

  //For secure cookie verification
  window.SECURE_ORIGIN = "https://" + HOST + SECURE_PORT;
  window.INSECURE_ORIGIN = "http://" + HOST + PORT;

  //standard references
  window.ORIGIN = "http://" + HOST + PORT;
  window.WWW_ORIGIN = "http://{{domains[www]}}" + PORT;
  window.SUBDOMAIN_ORIGIN = "http://{{domains[www1]}}" + PORT;
  window.CROSS_SITE_ORIGIN = "http://" + CROSS_ORIGIN_HOST + PORT;
  window.SECURE_CROSS_SITE_ORIGIN = "https://" + SECURE_CROSS_ORIGIN_HOST + SECURE_PORT;
  window.CROSS_SITE_HOST = SECURE_CROSS_ORIGIN_HOST;

  // Set the global cookie name.
  window.HTTP_COOKIE = "cookie_via_http";
})();

// A tiny helper which returns the result of fetching |url| with credentials.
function credFetch(url) {
  return fetch(url, {"credentials": "include"})
    .then(response => {
      if (response.status !== 200) {
        throw new Error(response.statusText);
      }
      return response;
    });
}

// Returns a URL on |origin| which redirects to a given absolute URL.
function redirectTo(origin, url) {
  return origin + "/cookies/resources/redirectWithCORSHeaders.py?status=307&location=" + encodeURIComponent(url);
}

// Asserts that `document.cookie` contains or does not contain (according to
// the value of |present|) a cookie named |name| with a value of |value|.
function assert_dom_cookie(name, value, present) {
  var re = new RegExp("(?:^|; )" + name + "=" + value + "(?:$|;)");
  assert_equals(re.test(document.cookie), present, "`" + name + "=" + value + "` in `document.cookie`");
}

function assert_cookie(origin, obj, name, value, present) {
  assert_equals(obj[name], present ? value : undefined, "`" + name + "=" + value + "` in request to `" + origin + "`.");
}

// Remove the cookie named |name| from |origin|, then set it on |origin| anew.
// If |origin| matches `self.origin`, also assert (via `document.cookie`) that
// the cookie was correctly removed and reset.
function create_cookie(origin, name, value, extras) {
  alert("Create_cookie: " + origin + "/cookies/resources/drop.py?name=" + name);
  return credFetch(origin + "/cookies/resources/drop.py?name=" + name)
    .then(_ => {
      if (origin == self.origin)
        assert_dom_cookie(name, value, false);
    })
    .then(_ => {
      return credFetch(origin + "/cookies/resources/set.py?" + name + "=" + value + ";path=/;" + extras)
        .then(_ => {
          if (origin == self.origin)
            assert_dom_cookie(name, value, true);
        });
    });
}

//
// Prefix-specific test helpers
//
function set_prefixed_cookie_via_dom_test(options) {
  promise_test(t => {
    var name = options.prefix + "prefixtestcookie";
    erase_cookie_from_js(name, options.paras);
    t.add_cleanup(() => erase_cookie_from_js(name, options.params));
    var value = "" + Math.random();
    document.cookie = name + "=" + value + ";" + options.params;

    assert_dom_cookie(name, value, options.shouldExistInDOM);

    return credFetch("/cookies/resources/list.py")
      .then(r => r.json())
      .then(cookies => assert_equals(cookies[name], options.shouldExistViaHTTP ? value : undefined));
  }, options.title);
}

function set_prefixed_cookie_via_http_test(options) {
  promise_test(t => {
    var name = options.prefix + "prefixtestcookie";
    var value = "" + Math.random();

    t.add_cleanup(() => {
      var cookie = name + "=0;expires=" + new Date(0).toUTCString() + ";" +
        options.params;

      return credFetch(options.origin + "/cookies/resources/set.py?" + cookie);
    });

    return credFetch(options.origin + "/cookies/resources/set.py?" + name + "=" + value + ";" + options.params)
      .then(_ => credFetch(options.origin + "/cookies/resources/list.py"))
      .then(r => r.json())
      .then(cookies => assert_equals(cookies[name], options.shouldExistViaHTTP ? value : undefined));
  }, options.title);
}

//
// SameSite-specific test helpers:
//

window.SameSiteStatus = {
  CROSS_SITE: "cross-site",
  LAX: "lax",
  STRICT: "strict"
};

const wait_for_message = (type, origin) => {
  return new Promise((resolve, reject) => {
    window.addEventListener('message', e => {
      if (e.origin != origin) {
        reject("Message from unexpected origin in wait_for_message:" + e.origin);
        return;
      }

      if (e.data.type && e.data.type === type)
        resolve(e);
    }, { once: true });
  });
};

// Reset SameSite test cookies on |origin|. If |origin| matches `self.origin`, assert
// (via `document.cookie`) that they were properly removed and reset.
async function resetSameSiteCookies(origin, value) {
  let w = window.open(origin + "/cookies/samesite/resources/puppet.html");
  try {
    await wait_for_message("READY", origin);
    w.postMessage({type: "drop", useOwnOrigin: true}, "*");
    await wait_for_message("drop-complete", origin);
    if (origin == self.origin) {
      assert_dom_cookie("samesite_strict", value, false);
      assert_dom_cookie("samesite_lax", value, false);
      assert_dom_cookie("samesite_none", value, false);
    }

    w.postMessage({type: "set", value: value, useOwnOrigin: true}, "*");
    await wait_for_message("set-complete", origin);
    if (origin == self.origin) {
      assert_dom_cookie("samesite_strict", value, true);
      assert_dom_cookie("samesite_lax", value, true);
      assert_dom_cookie("samesite_none", value, true);
    }
  } finally {
    w.close();
  }
}

// Given an |expectedStatus| and |expectedValue|, assert the |cookies| contains the
// proper set of cookie names and values.
function verifySameSiteCookieState(expectedStatus, expectedValue, cookies) {
    assert_equals(cookies["samesite_none"], expectedValue, "Non-SameSite cookies are always sent.");
    if (expectedStatus == SameSiteStatus.CROSS_SITE) {
      assert_not_equals(cookies["samesite_strict"], expectedValue, "SameSite=Strict cookies are not sent with cross-site requests.");
      assert_not_equals(cookies["samesite_lax"], expectedValue, "SameSite=Lax cookies are not sent with cross-site requests.");
    } else if (expectedStatus == SameSiteStatus.LAX) {
      assert_not_equals(cookies["samesite_strict"], expectedValue, "SameSite=Strict cookies are not sent with lax requests.");
      assert_equals(cookies["samesite_lax"], expectedValue, "SameSite=Lax cookies are sent with lax requests.");
    } else if (expectedStatus == SameSiteStatus.STRICT) {
      assert_equals(cookies["samesite_strict"], expectedValue, "SameSite=Strict cookies are sent with strict requests.");
      assert_equals(cookies["samesite_lax"], expectedValue, "SameSite=Lax cookies are sent with strict requests.");
    }
}

//
// LeaveSecureCookiesAlone-specific test helpers:
//

window.SecureStatus = {
  INSECURE_COOKIE_ONLY: "1",
  BOTH_COOKIES: "2",
};

//Reset SameSite test cookies on |origin|. If |origin| matches `self.origin`, assert
//(via `document.cookie`) that they were properly removed and reset.
function resetSecureCookies(origin, value) {
return credFetch(origin + "/cookies/resources/dropSecure.py")
 .then(_ => {
   if (origin == self.origin) {
     assert_dom_cookie("alone_secure", value, false);
     assert_dom_cookie("alone_insecure", value, false);
   }
 })
 .then(_ => {
     return credFetch(origin + "/cookie/resources/setSecure.py?" + value)
 })
}

//
// DOM based cookie manipulation APIs
//

// erase cookie value and set for expiration
function erase_cookie_from_js(name, params) {
  document.cookie = `${name}=0; expires=${new Date(0).toUTCString()}; ${params};`;
  var re = new RegExp("(?:^|; )" + name);
  assert_equals(re.test(document.cookie), false, "Sanity check: " + name + " has been deleted.");
}
