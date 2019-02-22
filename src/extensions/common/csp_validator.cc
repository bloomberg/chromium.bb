// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"

#include <stddef.h>

#include <initializer_list>
#include <iterator>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {

namespace csp_validator {

namespace {

const char kDefaultSrc[] = "default-src";
const char kScriptSrc[] = "script-src";
const char kObjectSrc[] = "object-src";
const char kFrameSrc[] = "frame-src";
const char kChildSrc[] = "child-src";

const char kDirectiveSeparator = ';';

const char kPluginTypes[] = "plugin-types";

const char kObjectSrcDefaultDirective[] = "object-src 'self';";
const char kScriptSrcDefaultDirective[] = "script-src 'self';";

const char kAppSandboxSubframeSrcDefaultDirective[] = "child-src 'self';";
const char kAppSandboxScriptSrcDefaultDirective[] =
    "script-src 'self' 'unsafe-inline' 'unsafe-eval';";

const char kSandboxDirectiveName[] = "sandbox";
const char kAllowSameOriginToken[] = "allow-same-origin";
const char kAllowTopNavigation[] = "allow-top-navigation";

// This is the list of plugin types which are fully sandboxed and are safe to
// load up in an extension, regardless of the URL they are navigated to.
const char* const kSandboxedPluginTypes[] = {
  "application/pdf",
  "application/x-google-chrome-pdf",
  "application/x-pnacl"
};

// List of CSP hash-source prefixes that are accepted. Blink is a bit more
// lenient, but we only accept standard hashes to be forward-compatible.
// http://www.w3.org/TR/2015/CR-CSP2-20150721/#hash_algo
const char* const kHashSourcePrefixes[] = {
  "'sha256-",
  "'sha384-",
  "'sha512-"
};

// Represents the status of a directive in a CSP string.
//
// Examples of directive:
// script source related: scrict-src
// subframe source related: child-src/frame-src.
class DirectiveStatus {
 public:
  // Subframe related directives can have multiple directive names: "child-src"
  // or "frame-src".
  DirectiveStatus(std::initializer_list<const char*> directives)
      : directive_names_(directives.begin(), directives.end()) {}

  // Returns true if |directive_name| matches this DirectiveStatus.
  bool Matches(const std::string& directive_name) const {
    for (const auto& directive : directive_names_) {
      if (!base::CompareCaseInsensitiveASCII(directive_name, directive))
        return true;
    }
    return false;
  }

  bool seen_in_policy() const { return seen_in_policy_; }
  void set_seen_in_policy() { seen_in_policy_ = true; }

  const std::string& name() const {
    DCHECK(!directive_names_.empty());
    return directive_names_[0];
  }

 private:
  // The CSP directive names this DirectiveStatus cares about.
  std::vector<std::string> directive_names_;
  // Whether or not we've seen any directive name that matches |this|.
  bool seen_in_policy_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectiveStatus);
};

// Returns whether |url| starts with |scheme_and_separator| and does not have a
// too permissive wildcard host name. If |should_check_rcd| is true, then the
// Public suffix list is used to exclude wildcard TLDs such as "https://*.org".
bool isNonWildcardTLD(const std::string& url,
                      const std::string& scheme_and_separator,
                      bool should_check_rcd) {
  if (!base::StartsWith(url, scheme_and_separator,
                        base::CompareCase::SENSITIVE))
    return false;

  size_t start_of_host = scheme_and_separator.length();

  size_t end_of_host = url.find("/", start_of_host);
  if (end_of_host == std::string::npos)
    end_of_host = url.size();

  // Note: It is sufficient to only compare the first character against '*'
  // because the CSP only allows wildcards at the start of a directive, see
  // host-source and host-part at http://www.w3.org/TR/CSP2/#source-list-syntax
  bool is_wildcard_subdomain = end_of_host > start_of_host + 2 &&
      url[start_of_host] == '*' && url[start_of_host + 1] == '.';
  if (is_wildcard_subdomain)
    start_of_host += 2;

  size_t start_of_port = url.rfind(":", end_of_host);
  // The ":" check at the end of the following condition is used to avoid
  // treating the last part of an IPv6 address as a port.
  if (start_of_port > start_of_host && url[start_of_port - 1] != ':') {
    bool is_valid_port = false;
    // Do a quick sanity check. The following check could mistakenly flag
    // ":123456" or ":****" as valid, but that does not matter because the
    // relaxing CSP directive will just be ignored by Blink.
    for (size_t i = start_of_port + 1; i < end_of_host; ++i) {
      is_valid_port = base::IsAsciiDigit(url[i]) || url[i] == '*';
      if (!is_valid_port)
        break;
    }
    if (is_valid_port)
      end_of_host = start_of_port;
  }

  std::string host(url, start_of_host, end_of_host - start_of_host);
  // Global wildcards are not allowed.
  if (host.empty() || host.find("*") != std::string::npos)
    return false;

  if (!is_wildcard_subdomain || !should_check_rcd)
    return true;

  // Allow *.googleapis.com to be whitelisted for backwards-compatibility.
  // (crbug.com/409952)
  if (host == "googleapis.com")
    return true;

  // Wildcards on subdomains of a TLD are not allowed.
  return net::registry_controlled_domains::HostHasRegistryControlledDomain(
      host, net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Checks whether the source is a syntactically valid hash.
bool IsHashSource(base::StringPiece source) {
  if (source.empty() || source.back() != '\'')
    return false;

  size_t hash_end = source.length() - 1;
  for (const char* prefix : kHashSourcePrefixes) {
    if (base::StartsWith(source, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      for (size_t i = strlen(prefix); i < hash_end; ++i) {
        const char c = source[i];
        // The hash must be base64-encoded. Do not allow any other characters.
        if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '+' &&
            c != '/' && c != '=') {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

InstallWarning CSPInstallWarning(const std::string& csp_warning) {
  return InstallWarning(csp_warning, manifest_keys::kContentSecurityPolicy);
}

std::string GetSecureDirectiveValues(
    int options,
    const std::string& directive_name,
    const std::vector<base::StringPiece>& directive_values,
    std::vector<InstallWarning>* warnings) {
  std::vector<base::StringPiece> sane_csp_parts{directive_name};
  for (base::StringPiece source_literal : directive_values) {
    std::string source_lower = base::ToLowerASCII(source_literal);
    bool is_secure_csp_token = false;

    // We might need to relax this whitelist over time.
    if (source_lower == "'self'" || source_lower == "'none'" ||
        source_lower == "'wasm-eval'" || source_lower == "http://127.0.0.1" ||
        source_lower == "blob:" || source_lower == "filesystem:" ||
        source_lower == "http://localhost" ||
        base::StartsWith(source_lower,
                         "http://127.0.0.1:", base::CompareCase::SENSITIVE) ||
        base::StartsWith(source_lower,
                         "http://localhost:", base::CompareCase::SENSITIVE) ||
        isNonWildcardTLD(source_lower, "https://", true) ||
        isNonWildcardTLD(source_lower, "chrome://", false) ||
        isNonWildcardTLD(source_lower,
                         std::string(extensions::kExtensionScheme) +
                             url::kStandardSchemeSeparator,
                         false) ||
        IsHashSource(source_literal)) {
      is_secure_csp_token = true;
    } else if ((options & OPTIONS_ALLOW_UNSAFE_EVAL) &&
               source_lower == "'unsafe-eval'") {
      is_secure_csp_token = true;
    } else if (base::StartsWith(source_lower, "chrome-extension-resource:",
                                base::CompareCase::SENSITIVE)) {
      // The "chrome-extension-resource" scheme has been removed from the
      // codebase, but it may still appear in existing CSPs. We continue to
      // allow it here for compatibility. Requests on this scheme will not
      // return any kind of network response.
      is_secure_csp_token = true;
    }

    if (is_secure_csp_token) {
      sane_csp_parts.push_back(source_literal);
    } else if (warnings) {
      warnings->push_back(CSPInstallWarning(ErrorUtils::FormatErrorMessage(
          manifest_errors::kInvalidCSPInsecureValue, source_literal.as_string(),
          directive_name)));
    }
  }
  // End of CSP directive that was started at the beginning of this method. If
  // none of the values are secure, the policy will be empty and default to
  // 'none', which is secure.
  std::string last_part = sane_csp_parts.back().as_string();
  last_part.push_back(kDirectiveSeparator);
  sane_csp_parts.back() = last_part;
  return base::JoinString(sane_csp_parts, " ");
}

// Given a CSP directive-token for app sandbox, returns a secure value of that
// directive.
// The directive-token's name is |directive_name| and its values are splitted
// into |directive_values|.
std::string GetAppSandboxSecureDirectiveValues(
    const std::string& directive_name,
    const std::vector<base::StringPiece>& directive_values,
    std::vector<InstallWarning>* warnings) {
  std::vector<std::string> sane_csp_parts{directive_name};
  bool seen_self_or_none = false;
  for (base::StringPiece source_literal : directive_values) {
    std::string source_lower = base::ToLowerASCII(source_literal);

    // Keyword directive sources are surrounded with quotes, e.g. 'self',
    // 'sha256-...', 'unsafe-eval', 'nonce-...'. These do not specify a remote
    // host or '*', so keep them and restrict the rest.
    if (source_lower.size() > 1u && source_lower[0] == '\'' &&
        source_lower.back() == '\'') {
      seen_self_or_none |= source_lower == "'none'" || source_lower == "'self'";
      sane_csp_parts.push_back(source_lower);
    } else if (warnings) {
      warnings->push_back(CSPInstallWarning(ErrorUtils::FormatErrorMessage(
          manifest_errors::kInvalidCSPInsecureValue, source_literal.as_string(),
          directive_name)));
    }
  }

  // If we haven't seen any of 'self' or 'none', that means this directive
  // value isn't secure. Specify 'self' to secure it.
  if (!seen_self_or_none)
    sane_csp_parts.push_back("'self'");

  sane_csp_parts.back().push_back(kDirectiveSeparator);
  return base::JoinString(sane_csp_parts, " ");
}

// Returns true if the |plugin_type| is one of the fully sandboxed plugin types.
bool PluginTypeAllowed(const std::string& plugin_type) {
  for (size_t i = 0; i < arraysize(kSandboxedPluginTypes); ++i) {
    if (plugin_type == kSandboxedPluginTypes[i])
      return true;
  }
  return false;
}

// Returns true if the policy is allowed to contain an insecure object-src
// directive. This requires OPTIONS_ALLOW_INSECURE_OBJECT_SRC to be specified
// as an option and the plugin-types that can be loaded must be restricted to
// the set specified in kSandboxedPluginTypes.
bool AllowedToHaveInsecureObjectSrc(
    int options,
    const std::vector<std::string>& directives) {
  if (!(options & OPTIONS_ALLOW_INSECURE_OBJECT_SRC))
    return false;

  for (size_t i = 0; i < directives.size(); ++i) {
    const std::string& input = directives[i];
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;
    if (!base::LowerCaseEqualsASCII(tokenizer.token(), kPluginTypes))
      continue;
    while (tokenizer.GetNext()) {
      if (!PluginTypeAllowed(tokenizer.token()))
        return false;
    }
    // All listed plugin types are whitelisted.
    return true;
  }
  // plugin-types not specified.
  return false;
}

using SecureDirectiveValueFunction = base::Callback<std::string(
    const std::string& directive_name,
    const std::vector<base::StringPiece>& directive_values,
    std::vector<InstallWarning>* warnings)>;

// Represents a token in CSP string.
// Tokens are delimited by ";" CSP string.
class CSPDirectiveToken {
 public:
  explicit CSPDirectiveToken(base::StringPiece directive_token)
      : directive_token_(directive_token),
        parsed_(false),
        tokenizer_(directive_token.begin(), directive_token.end(), " \t\r\n") {
    is_empty_ = !tokenizer_.GetNext();
    if (!is_empty_)
      directive_name_ = tokenizer_.token();
  }

  // Returns true if this token affects |status|. In that case, the token's
  // directive values are secured by |secure_function|.
  bool MatchAndUpdateStatus(DirectiveStatus* status,
                            const SecureDirectiveValueFunction& secure_function,
                            std::vector<InstallWarning>* warnings) {
    if (is_empty_ || !status->Matches(directive_name_))
      return false;

    EnsureTokenPartsParsed();

    bool is_duplicate_directive = status->seen_in_policy();
    status->set_seen_in_policy();

    secure_value_ = secure_function.Run(
        directive_name_, directive_values_,
        // Don't show any errors for duplicate CSP directives, because it will
        // be ignored by the CSP parser
        // (http://www.w3.org/TR/CSP2/#policy-parsing). Therefore, set warnings
        // param to nullptr.
        is_duplicate_directive ? nullptr : warnings);
    return true;
  }

  std::string ToString() {
    if (secure_value_)
      return secure_value_.value();
    // This token didn't require modification.
    return base::StringPrintf("%s%c", directive_token_.as_string().c_str(),
                              kDirectiveSeparator);
  }

 private:
  void EnsureTokenPartsParsed() {
    if (!parsed_) {
      while (tokenizer_.GetNext())
        directive_values_.push_back(tokenizer_.token_piece());
      parsed_ = true;
    }
  }

  base::StringPiece directive_token_;
  std::string directive_name_;
  std::vector<base::StringPiece> directive_values_;

  base::Optional<std::string> secure_value_;

  bool is_empty_;
  bool parsed_;
  base::CStringTokenizer tokenizer_;

  DISALLOW_COPY_AND_ASSIGN(CSPDirectiveToken);
};

// Class responsible for parsing a given CSP string |policy|, and enforcing
// secure directive-tokens within the policy.
//
// If a CSP directive's value is not secure, this class will use secure
// values (via |secure_function|). If a CSP directive-token is not present and
// as a result will fallback to default (possibly non-secure), this class
// will use default secure values (via GetDefaultCSPValue).
class CSPEnforcer {
 public:
  CSPEnforcer(bool show_missing_csp_warnings,
              const SecureDirectiveValueFunction& secure_function)
      : show_missing_csp_warnings_(show_missing_csp_warnings),
        secure_function_(secure_function) {}
  virtual ~CSPEnforcer() {}

  // Returns the enforced CSP.
  // Emits warnings in |warnings| for insecure directive values. If
  // |show_missing_csp_warnings_| is true, these will also include missing CSP
  // directive warnings.
  std::string Enforce(const std::string& policy,
                      std::vector<InstallWarning>* warnings);

 protected:
  virtual std::string GetDefaultCSPValue(const DirectiveStatus& status) = 0;

  // List of directives we care about.
  std::vector<std::unique_ptr<DirectiveStatus>> directives_;

 private:
  const bool show_missing_csp_warnings_;
  const SecureDirectiveValueFunction secure_function_;

  DISALLOW_COPY_AND_ASSIGN(CSPEnforcer);
};

std::string CSPEnforcer::Enforce(const std::string& policy,
                                 std::vector<InstallWarning>* warnings) {
  DCHECK(!directives_.empty());
  std::vector<std::string> enforced_csp_parts;

  // If any directive that we care about isn't explicitly listed in |policy|,
  // "default-src" fallback is used.
  DirectiveStatus default_src_status({kDefaultSrc});
  std::vector<InstallWarning> default_src_csp_warnings;

  for (const base::StringPiece& directive_token : base::SplitStringPiece(
           policy, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    CSPDirectiveToken csp_directive_token(directive_token);
    bool matches_enforcing_directive = false;
    for (const std::unique_ptr<DirectiveStatus>& status : directives_) {
      if (csp_directive_token.MatchAndUpdateStatus(
              status.get(), secure_function_, warnings)) {
        matches_enforcing_directive = true;
        break;
      }
    }
    if (!matches_enforcing_directive) {
      csp_directive_token.MatchAndUpdateStatus(
          &default_src_status, secure_function_, &default_src_csp_warnings);
    }

    enforced_csp_parts.push_back(csp_directive_token.ToString());
  }

  if (default_src_status.seen_in_policy()) {
    for (const std::unique_ptr<DirectiveStatus>& status : directives_) {
      if (!status->seen_in_policy()) {
        // This |status| falls back to "default-src". So warnings from
        // "default-src" will apply.
        if (warnings) {
          warnings->insert(
              warnings->end(),
              std::make_move_iterator(default_src_csp_warnings.begin()),
              std::make_move_iterator(default_src_csp_warnings.end()));
        }
        break;
      }
    }
  } else {
    // Did not see "default-src".
    // Make sure we cover all sources from |directives_|.
    for (const std::unique_ptr<DirectiveStatus>& status : directives_) {
      if (status->seen_in_policy())  // Already covered.
        continue;
      enforced_csp_parts.push_back(GetDefaultCSPValue(*status));

      if (warnings && show_missing_csp_warnings_) {
        warnings->push_back(CSPInstallWarning(ErrorUtils::FormatErrorMessage(
            manifest_errors::kInvalidCSPMissingSecureSrc, status->name())));
      }
    }
  }

  return base::JoinString(enforced_csp_parts, " ");
}

class ExtensionCSPEnforcer : public CSPEnforcer {
 public:
  ExtensionCSPEnforcer(bool allow_insecure_object_src, int options)
      : CSPEnforcer(true, base::Bind(&GetSecureDirectiveValues, options)) {
    directives_.emplace_back(new DirectiveStatus({kScriptSrc}));
    if (!allow_insecure_object_src)
      directives_.emplace_back(new DirectiveStatus({kObjectSrc}));
  }

 protected:
  std::string GetDefaultCSPValue(const DirectiveStatus& status) override {
    if (status.Matches(kObjectSrc))
      return kObjectSrcDefaultDirective;
    DCHECK(status.Matches(kScriptSrc));
    return kScriptSrcDefaultDirective;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionCSPEnforcer);
};

class AppSandboxPageCSPEnforcer : public CSPEnforcer {
 public:
  AppSandboxPageCSPEnforcer()
      : CSPEnforcer(false, base::Bind(&GetAppSandboxSecureDirectiveValues)) {
    directives_.emplace_back(new DirectiveStatus({kChildSrc, kFrameSrc}));
    directives_.emplace_back(new DirectiveStatus({kScriptSrc}));
  }

 protected:
  std::string GetDefaultCSPValue(const DirectiveStatus& status) override {
    if (status.Matches(kChildSrc))
      return kAppSandboxSubframeSrcDefaultDirective;
    DCHECK(status.Matches(kScriptSrc));
    return kAppSandboxScriptSrcDefaultDirective;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppSandboxPageCSPEnforcer);
};

}  //  namespace

bool ContentSecurityPolicyIsLegal(const std::string& policy) {
  // We block these characters to prevent HTTP header injection when
  // representing the content security policy as an HTTP header.
  const char kBadChars[] = {',', '\r', '\n', '\0'};

  return policy.find_first_of(kBadChars, 0, arraysize(kBadChars)) ==
         std::string::npos;
}

std::string SanitizeContentSecurityPolicy(
    const std::string& policy,
    int options,
    std::vector<InstallWarning>* warnings) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  std::vector<std::string> directives = base::SplitString(
      policy, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  bool allow_insecure_object_src =
      AllowedToHaveInsecureObjectSrc(options, directives);

  ExtensionCSPEnforcer csp_enforcer(allow_insecure_object_src, options);
  return csp_enforcer.Enforce(policy, warnings);
}

std::string GetEffectiveSandoxedPageCSP(const std::string& policy,
                                        std::vector<InstallWarning>* warnings) {
  AppSandboxPageCSPEnforcer csp_enforcer;
  return csp_enforcer.Enforce(policy, warnings);
}

bool ContentSecurityPolicyIsSandboxed(
    const std::string& policy, Manifest::Type type) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  bool seen_sandbox = false;
  for (const std::string& input : base::SplitString(
           policy, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;

    std::string directive_name = base::ToLowerASCII(tokenizer.token_piece());
    if (directive_name != kSandboxDirectiveName)
      continue;

    seen_sandbox = true;

    while (tokenizer.GetNext()) {
      std::string token = base::ToLowerASCII(tokenizer.token_piece());

      // The same origin token negates the sandboxing.
      if (token == kAllowSameOriginToken)
        return false;

      // Platform apps don't allow navigation.
      if (type == Manifest::TYPE_PLATFORM_APP) {
        if (token == kAllowTopNavigation)
          return false;
      }
    }
  }

  return seen_sandbox;
}

}  // namespace csp_validator

}  // namespace extensions
