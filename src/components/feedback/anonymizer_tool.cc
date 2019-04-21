// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/anonymizer_tool.h"

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_address.h"
#include "third_party/re2/src/re2/re2.h"

using re2::RE2;

namespace feedback {

namespace {

// The |kCustomPatternsWithContext| array defines patterns to match and
// anonymize. Each pattern needs to define three capturing parentheses groups:
//
// - a group for the pattern before the identifier to be anonymized;
// - a group for the identifier to be anonymized;
// - a group for the pattern after the identifier to be anonymized.
//
// The first and the last capture group are the origin of the "WithContext"
// suffix in the name of this constant.
//
// Every matched identifier (in the context of the whole pattern) is anonymized
// by replacing it with an incremental instance identifier. Every different
// pattern defines a separate instance identifier space. See the unit test for
// AnonymizerTool::AnonymizeCustomPattern for pattern anonymization examples.
//
// Useful regular expression syntax:
//
// +? is a non-greedy (lazy) +.
// \b matches a word boundary.
// (?i) turns on case insensitivity for the remainder of the regex.
// (?-s) turns off "dot matches newline" for the remainder of the regex.
// (?:regex) denotes non-capturing parentheses group.
constexpr const char* kCustomPatternsWithContext[] = {
    // ModemManager
    "(\\bCell ID: ')([0-9a-fA-F]+)(')",
    "(\\bLocation area code: ')([0-9a-fA-F]+)(')",

    // wpa_supplicant
    "(?i-s)(\\bssid[= ]')(.+)(')",
    "(?-s)(\\bSSID - hexdump\\(len=[0-9]+\\): )(.+)()",

    // shill
    "(?-s)(\\[SSID=)(.+?)(\\])",

    // Serial numbers
    "(?i-s)(serial\\s*(?:number)?\\s*[:=]\\s*)([0-9a-zA-Z\\-\"]+)()",
};

bool MaybeUnmapAddress(net::IPAddress* addr) {
  if (!addr->IsIPv4MappedIPv6())
    return false;

  *addr = net::ConvertIPv4MappedIPv6ToIPv4(*addr);
  return true;
}

bool MaybeUntranslateAddress(net::IPAddress* addr) {
  if (!addr->IsIPv6())
    return false;

  static const net::IPAddress kTranslated6To4(0, 0x64, 0xff, 0x9b, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 0, 0, 0);
  if (!IPAddressMatchesPrefix(*addr, kTranslated6To4, 96))
    return false;

  const auto bytes = addr->bytes();
  *addr = net::IPAddress(bytes[12], bytes[13], bytes[14], bytes[15]);
  return true;
}

// If |addr| points to a valid IPv6 address, this function truncates it at /32.
bool MaybeTruncateIPv6(net::IPAddress* addr) {
  if (!addr->IsIPv6())
    return false;

  const auto bytes = addr->bytes();
  *addr = net::IPAddress(bytes[0], bytes[1], bytes[2], bytes[3], 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0);
  return true;
}

// Returns an appropriately scrubbed version of |addr| if applicable.
std::string MaybeScrubIPAddress(const std::string& addr) {
  struct {
    net::IPAddress ip_addr;
    int prefix_length;
    bool scrub;
  } static const kWhitelistedIPRanges[] = {
      // Private.
      {net::IPAddress(10, 0, 0, 0), 8, true},
      {net::IPAddress(172, 16, 0, 0), 12, true},
      {net::IPAddress(192, 168, 0, 0), 16, true},
      // Chrome OS containers and VMs.
      {net::IPAddress(100, 115, 92, 0), 24, false},
      // Loopback.
      {net::IPAddress(127, 0, 0, 0), 8, true},
      // Any.
      {net::IPAddress(0, 0, 0, 0), 8, true},
      // DNS.
      {net::IPAddress(8, 8, 8, 8), 32, false},
      {net::IPAddress(8, 8, 4, 4), 32, false},
      {net::IPAddress(1, 1, 1, 1), 32, false},
      // Multicast.
      {net::IPAddress(224, 0, 0, 0), 4, true},
      // Link local.
      {net::IPAddress(169, 254, 0, 0), 16, true},
      {net::IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
       true},
      // Broadcast.
      {net::IPAddress(255, 255, 255, 255), 32, false},
      // IPv6 loopback, unspecified and non-address strings.
      {net::IPAddress::IPv6AllZeros(), 112, false},
      // IPv6 multicast all nodes and routers.
      {net::IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1),
       128, false},
      {net::IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2),
       128, false},
      {net::IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1),
       128, false},
      {net::IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2),
       128, false},
      // IPv6 other multicast (link and interface local).
      {net::IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 16,
       true},
      {net::IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 16,
       true},

  };
  net::IPAddress input_addr;
  if (input_addr.AssignFromIPLiteral(addr) && input_addr.IsValid()) {
    bool mapped = MaybeUnmapAddress(&input_addr);
    bool translated = !mapped ? MaybeUntranslateAddress(&input_addr) : false;
    for (const auto& range : kWhitelistedIPRanges) {
      if (IPAddressMatchesPrefix(input_addr, range.ip_addr,
                                 range.prefix_length)) {
        std::string prefix;
        std::string out_addr = addr;
        if (mapped) {
          prefix = "M ";
          out_addr = input_addr.ToString();
        } else if (translated) {
          prefix = "T ";
          out_addr = input_addr.ToString();
        }
        if (range.scrub) {
          out_addr = base::StringPrintf(
              "%s/%d", range.ip_addr.ToString().c_str(), range.prefix_length);
        }
        return base::StrCat({prefix, out_addr});
      }
    }
    // |addr| may have been over-aggressively matched as an IPv6 address when
    // it's really just an arbitrary part of a sentence. If the string is the
    // same as the coarsely truncated address then keep it because even if
    // it happens to be a real address, there is no loss of anonymity.
    if (MaybeTruncateIPv6(&input_addr) && input_addr.ToString() == addr)
      return addr;
  }
  return "";
}

// Helper macro: Non capturing group
#define NCG(x) "(?:" x ")"
// Helper macro: Optional non capturing group
#define OPT_NCG(x) NCG(x) "?"

//////////////////////////////////////////////////////////////////////////
// Patterns for URLs, or better IRIs, based on RFC 3987 with an artificial
// limitation on the scheme to increase precision. Otherwise anything
// like "ID:" would be considered an IRI.

#define UNRESERVED "[-a-z0-9._~]"
#define RESERVED NGC(GEN_DELIMS "|" SUB_DELIMS)
#define SUB_DELIMS "[!$&'()*+,;=]"
#define GEN_DELIMS "[:/?#[\\]@]"

#define DIGIT "[0-9]"
#define HEXDIG "[0-9a-f]"

#define PCT_ENCODED "%" HEXDIG HEXDIG

#define DEC_OCTET NCG("1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9]")

#define IPV4ADDRESS DEC_OCTET "\\." DEC_OCTET "\\." DEC_OCTET "\\." DEC_OCTET

#define H16 NCG(HEXDIG) "{1,4}"
#define LS32 NCG(H16 ":" H16 "|" IPV4ADDRESS)

#define IPV6ADDRESS NCG( \
                                          NCG(H16 ":") "{6}" LS32 "|" \
                                     "::" NCG(H16 ":") "{5}" LS32 "|" \
  OPT_NCG(                      H16) "::" NCG(H16 ":") "{4}" LS32 "|" \
  OPT_NCG( NCG(H16 ":") "{0,1}" H16) "::" NCG(H16 ":") "{3}" LS32 "|" \
  OPT_NCG( NCG(H16 ":") "{0,2}" H16) "::" NCG(H16 ":") "{2}" LS32 "|" \
  OPT_NCG( NCG(H16 ":") "{0,3}" H16) "::" NCG(H16 ":")       LS32 "|" \
  OPT_NCG( NCG(H16 ":") "{0,4}" H16) "::"                    LS32 "|" \
  OPT_NCG( NCG(H16 ":") "{0,5}" H16) "::"                    H16 "|" \
  OPT_NCG( NCG(H16 ":") "{0,6}" H16) "::")

#define IPVFUTURE                     \
  "v" HEXDIG                          \
  "+"                                 \
  "\\." NCG(UNRESERVED "|" SUB_DELIMS \
                       "|"            \
                       ":") "+"

#define IP_LITERAL "\\[" NCG(IPV6ADDRESS "|" IPVFUTURE) "\\]"

#define PORT DIGIT "*"

// This is a diversion of RFC 3987
#define SCHEME NCG("http|https|ftp|chrome|chrome-extension|android|rtsp")

#define IPRIVATE            \
  "["                       \
  "\\x{E000}-\\x{F8FF}"     \
  "\\x{F0000}-\\x{FFFFD}"   \
  "\\x{100000}-\\x{10FFFD}" \
  "]"

#define UCSCHAR \
  "[" "\\x{A0}-\\x{D7FF}" "\\x{F900}-\\x{FDCF}" "\\x{FDF0}-\\x{FFEF}" \
  "\\x{10000}-\\x{1FFFD}" "\\x{20000}-\\x{2FFFD}" "\\x{30000}-\\x{3FFFD}" \
  "\\x{40000}-\\x{4FFFD}" "\\x{50000}-\\x{5FFFD}" "\\x{60000}-\\x{6FFFD}" \
  "\\x{70000}-\\x{7FFFD}" "\\x{80000}-\\x{8FFFD}" "\\x{90000}-\\x{9FFFD}" \
  "\\x{A0000}-\\x{AFFFD}" "\\x{B0000}-\\x{BFFFD}" "\\x{C0000}-\\x{CFFFD}" \
  "\\x{D0000}-\\x{DFFFD}" "\\x{E1000}-\\x{EFFFD}" "]"

#define IUNRESERVED NCG("[-a-z0-9._~]" "|" UCSCHAR)

#define IPCHAR NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS "|" "[:@]")
#define IFRAGMENT NCG(IPCHAR "|" "[/?]") "*"
#define IQUERY NCG(IPCHAR "|" IPRIVATE "|" "[/?]") "*"

#define ISEGMENT IPCHAR "*"
#define ISEGMENT_NZ IPCHAR "+"
#define ISEGMENT_NZ_NC                           \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|" "@") "+"

#define IPATH_EMPTY ""
#define IPATH_ROOTLESS ISEGMENT_NZ NCG("/" ISEGMENT) "*"
#define IPATH_NOSCHEME ISEGMENT_NZ_NC NCG("/" ISEGMENT) "*"
#define IPATH_ABSOLUTE "/" OPT_NCG(ISEGMENT_NZ NCG("/" ISEGMENT) "*")
#define IPATH_ABEMPTY NCG("/" ISEGMENT) "*"

#define IPATH NCG(IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_NOSCHEME "|" \
                  IPATH_ROOTLESS "|" IPATH_EMPTY)

#define IREG_NAME NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS) "*"

#define IHOST NCG(IP_LITERAL "|" IPV4ADDRESS "|" IREG_NAME)
#define IUSERINFO NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS "|" ":") "*"
#define IAUTHORITY OPT_NCG(IUSERINFO "@") IHOST OPT_NCG(":" PORT)

#define IRELATIVE_PART NCG("//" IAUTHORITY IPATH_ABEMPTY "|" IPATH_ABSOLUTE \
                           "|" IPATH_NOSCHEME "|" IPATH_EMPTY)

#define IRELATIVE_REF IRELATIVE_PART OPT_NCG("?" IQUERY) OPT_NCG("#" IFRAGMENT)

// RFC 3987 requires IPATH_EMPTY here but it is omitted so that statements
// that end with "Android:" for example are not considered a URL.
#define IHIER_PART NCG("//" IAUTHORITY IPATH_ABEMPTY "|" IPATH_ABSOLUTE \
                       "|" IPATH_ROOTLESS)

#define ABSOLUTE_IRI SCHEME ":" IHIER_PART OPT_NCG("?" IQUERY)

#define IRI SCHEME ":" IHIER_PART OPT_NCG("\\?" IQUERY) OPT_NCG("#" IFRAGMENT)

#define IRI_REFERENCE NCG(IRI "|" IRELATIVE_REF)

// TODO(battre): Use http://tools.ietf.org/html/rfc5322 to represent email
// addresses. Capture names as well ("First Lastname" <foo@bar.com>).

// The |kCustomPatternWithoutContext| array defines further patterns to match
// and anonymize. Each pattern consists of a single capturing group.
CustomPatternWithoutContext kCustomPatternsWithoutContext[] = {
    {"URL", "(?i)(" IRI ")"},
    // Email Addresses need to come after URLs because they can be part
    // of a query parameter.
    {"email", "(?i)([0-9a-z._%+-]+@[a-z0-9.-]+\\.[a-z]{2,6})"},
    // IP filter rules need to come after URLs so that they don't disturb the
    // URL pattern in case the IP address is part of a URL.
    {"IPv4", "(?i)(" IPV4ADDRESS ")"},
    {"IPv6", "(?i)(" IPV6ADDRESS ")"},
    // Universal Unique Identifiers (UUIDs).
    {"UUID",
     "(?i)([0-9a-zA-Z]{8}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-"
     "[0-9a-zA-Z]{12})"},
};

// Like RE2's FindAndConsume, searches for the first occurrence of |pattern| in
// |input| and consumes the bytes until the end of the pattern matching. Unlike
// FindAndConsume, the bytes skipped before the match of |pattern| are stored
// in |skipped_input|. |args| needs to contain at least one element.
// Returns whether a match was found.
//
// Example: input = "aaabbbc", pattern = "(b+)" leads to skipped_input = "aaa",
// args[0] = "bbb", and the beginning input is moved to the right so that it
// only contains "c".
// Example: input = "aaabbbc", pattern = "(z+)" leads to input = "aaabbbc",
// the args values are not modified and skipped_input is not modified.
bool FindAndConsumeAndGetSkippedN(re2::StringPiece* input,
                                  const re2::RE2& pattern,
                                  re2::StringPiece* skipped_input,
                                  re2::StringPiece* args[],
                                  int argc) {
  re2::StringPiece old_input = *input;

  CHECK_GE(argc, 1);
  re2::RE2::Arg a0(argc > 0 ? args[0] : nullptr);
  re2::RE2::Arg a1(argc > 1 ? args[1] : nullptr);
  re2::RE2::Arg a2(argc > 2 ? args[2] : nullptr);
  const re2::RE2::Arg* const wrapped_args[] = {&a0, &a1, &a2};
  CHECK_LE(argc, 3);

  bool result = re2::RE2::FindAndConsumeN(input, pattern, wrapped_args, argc);

  if (skipped_input && result) {
    size_t bytes_skipped = args[0]->data() - old_input.data();
    *skipped_input = re2::StringPiece(old_input.data(), bytes_skipped);
  }
  return result;
}

// All |match_groups| need to be of type re2::StringPiece*.
template <typename... Arg>
bool FindAndConsumeAndGetSkipped(re2::StringPiece* input,
                                 const re2::RE2& pattern,
                                 re2::StringPiece* skipped_input,
                                 Arg*... match_groups) {
  re2::StringPiece* args[] = {match_groups...};
  return FindAndConsumeAndGetSkippedN(input, pattern, skipped_input, args,
                                      base::size(args));
}

}  // namespace

AnonymizerTool::AnonymizerTool()
    : custom_patterns_with_context_(base::size(kCustomPatternsWithContext)),
      custom_patterns_without_context_(
          base::size(kCustomPatternsWithoutContext)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AnonymizerTool::~AnonymizerTool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string AnonymizerTool::Anonymize(const std::string& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI))
      << "This is an expensive operation. Do not execute this on the UI "
         "thread.";
  std::string anonymized = AnonymizeMACAddresses(input);
  anonymized = AnonymizeCustomPatterns(std::move(anonymized));
  return anonymized;
}

RE2* AnonymizerTool::GetRegExp(const std::string& pattern) {
  if (regexp_cache_.find(pattern) == regexp_cache_.end()) {
    RE2::Options options;
    // set_multiline of pcre is not supported by RE2, yet.
    options.set_dot_nl(true);  // Dot matches a new line.
    std::unique_ptr<RE2> re = std::make_unique<RE2>(pattern, options);
    DCHECK_EQ(re2::RE2::NoError, re->error_code())
        << "Failed to parse:\n" << pattern << "\n" << re->error();
    regexp_cache_[pattern] = std::move(re);
  }
  return regexp_cache_[pattern].get();
}

std::string AnonymizerTool::AnonymizeMACAddresses(const std::string& input) {
  // This regular expression finds the next MAC address. It splits the data into
  // an OUI (Organizationally Unique Identifier) part and a NIC (Network
  // Interface Controller) specific part.

  RE2* mac_re = GetRegExp(
      "([0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F]):("
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F]:"
      "[0-9a-fA-F][0-9a-fA-F])");

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped;
  re2::StringPiece pre_mac, oui, nic;
  while (FindAndConsumeAndGetSkipped(&text, *mac_re, &skipped, &oui, &nic)) {
    // Look up the MAC address in the hash.
    std::string oui_string = base::ToLowerASCII(oui.as_string());
    std::string nic_string = base::ToLowerASCII(nic.as_string());
    std::string mac = oui_string + ":" + nic_string;
    std::string replacement_mac = mac_addresses_[mac];
    if (replacement_mac.empty()) {
      // If not found, build up a replacement MAC address by generating a new
      // NIC part.
      int mac_id = mac_addresses_.size();
      replacement_mac = base::StringPrintf(
          "%s:%02x:%02x:%02x", oui_string.c_str(), (mac_id & 0x00ff0000) >> 16,
          (mac_id & 0x0000ff00) >> 8, (mac_id & 0x000000ff));
      mac_addresses_[mac] = replacement_mac;
    }

    skipped.AppendToString(&result);
    result += replacement_mac;
  }

  text.AppendToString(&result);
  return result;
}

std::string AnonymizerTool::AnonymizeCustomPatterns(std::string input) {
  for (size_t i = 0; i < base::size(kCustomPatternsWithContext); i++) {
    input =
        AnonymizeCustomPatternWithContext(input, kCustomPatternsWithContext[i],
                                          &custom_patterns_with_context_[i]);
  }
  for (size_t i = 0; i < base::size(kCustomPatternsWithoutContext); i++) {
    input = AnonymizeCustomPatternWithoutContext(
        input, kCustomPatternsWithoutContext[i],
        &custom_patterns_without_context_[i]);
  }
  return input;
}

std::string AnonymizerTool::AnonymizeCustomPatternWithContext(
    const std::string& input,
    const std::string& pattern,
    std::map<std::string, std::string>* identifier_space) {
  RE2* re = GetRegExp(pattern);
  DCHECK_EQ(3, re->NumberOfCapturingGroups());

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped;
  re2::StringPiece pre_match, pre_matched_id, matched_id, post_matched_id;
  while (FindAndConsumeAndGetSkipped(&text, *re, &skipped, &pre_matched_id,
                                     &matched_id, &post_matched_id)) {
    std::string matched_id_as_string = matched_id.as_string();
    std::string replacement_id = (*identifier_space)[matched_id_as_string];
    if (replacement_id.empty()) {
      replacement_id = base::NumberToString(identifier_space->size());
      (*identifier_space)[matched_id_as_string] = replacement_id;
    }

    skipped.AppendToString(&result);
    pre_matched_id.AppendToString(&result);
    result += replacement_id;
    post_matched_id.AppendToString(&result);
  }
  text.AppendToString(&result);
  return result;
}

bool WhitelistMatchedId(re2::StringPiece matched_id) {
  bool is_safe_chrome_resource =
      matched_id.starts_with("chrome://resources/") &&
      !matched_id.contains("?");
  return is_safe_chrome_resource;
}

std::string AnonymizerTool::AnonymizeCustomPatternWithoutContext(
    const std::string& input,
    const CustomPatternWithoutContext& pattern,
    std::map<std::string, std::string>* identifier_space) {
  RE2* re = GetRegExp(pattern.pattern);
  DCHECK_EQ(1, re->NumberOfCapturingGroups());

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped;
  re2::StringPiece matched_id;
  while (FindAndConsumeAndGetSkipped(&text, *re, &skipped, &matched_id)) {
    if (WhitelistMatchedId(matched_id)) {
      skipped.AppendToString(&result);
      matched_id.AppendToString(&result);
      continue;
    }
    std::string matched_id_as_string = matched_id.as_string();
    std::string replacement_id = (*identifier_space)[matched_id_as_string];
    if (replacement_id.empty()) {
      replacement_id = MaybeScrubIPAddress(matched_id_as_string);
      if (replacement_id != matched_id_as_string) {
        // The weird Uint64toString trick is because Windows does not like
        // to deal with %zu and a size_t in printf, nor does it support %llu.
        replacement_id = base::StringPrintf(
            "<%s: %s>",
            replacement_id.empty() ? pattern.alias : replacement_id.c_str(),
            base::NumberToString(identifier_space->size()).c_str());
        (*identifier_space)[matched_id_as_string] = replacement_id;
      }
    }

    skipped.AppendToString(&result);
    result += replacement_id;
  }
  text.AppendToString(&result);
  return result;
}

AnonymizerToolContainer::AnonymizerToolContainer(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : anonymizer_(new AnonymizerTool), task_runner_(task_runner) {}

AnonymizerToolContainer::~AnonymizerToolContainer() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(anonymizer_));
}

AnonymizerTool* AnonymizerToolContainer::Get() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return anonymizer_.get();
}

}  // namespace feedback
