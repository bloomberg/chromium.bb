// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_policy_diagnostic.h"

#include <stddef.h>

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_string_value_serializer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

// Keys in base::Value snapshots of Policies for chrome://sandbox.
const char kAppContainerCapabilities[] = "appContainerCapabilities";
const char kAppContainerInitialCapabilities[] =
    "appContainerInitialCapabilities";
const char kAppContainerSid[] = "appContainerSid";
const char kDesiredIntegrityLevel[] = "desiredIntegrityLevel";
const char kDesiredMitigations[] = "desiredMitigations";
const char kDisconnectCsrss[] = "disconnectCsrss";
const char kHandlesToClose[] = "handlesToClose";
const char kJobLevel[] = "jobLevel";
const char kLockdownLevel[] = "lockdownLevel";
const char kLowboxSid[] = "lowboxSid";
const char kPlatformMitigations[] = "platformMitigations";
const char kPolicyRules[] = "policyRules";
const char kProcessIds[] = "processIds";

// Values in snapshots of Policies.
const char kDisabled[] = "disabled";
const char kEnabled[] = "enabled";

base::Value ProcessIdList(std::vector<uint32_t> process_ids) {
  base::Value results(base::Value::Type::LIST);
  for (const auto pid : process_ids) {
    results.Append(base::strict_cast<double>(pid));
  }
  return results;
}

std::string GetTokenLevelInEnglish(TokenLevel token) {
  switch (token) {
    case USER_LOCKDOWN:
      return "Lockdown";
    case USER_RESTRICTED:
      return "Restricted";
    case USER_LIMITED:
      return "Limited";
    case USER_INTERACTIVE:
      return "Interactive";
    case USER_RESTRICTED_SAME_ACCESS:
      return "Restricted Same Access";
    case USER_UNPROTECTED:
      return "None";
    case USER_RESTRICTED_NON_ADMIN:
      return "Restricted Non Admin";
    case USER_LAST:
      DCHECK(false) << "Unknown TokenType";
      return "Unknown";
  }
}

std::string GetJobLevelInEnglish(JobLevel job) {
  switch (job) {
    case JOB_LOCKDOWN:
      return "Lockdown";
    case JOB_RESTRICTED:
      return "Restricted";
    case JOB_LIMITED_USER:
      return "Limited User";
    case JOB_INTERACTIVE:
      return "Interactive";
    case JOB_UNPROTECTED:
      return "Unprotected";
    case JOB_NONE:
      return "None";
  }
}

std::string GetIntegrityLevelInEnglish(IntegrityLevel integrity) {
  switch (integrity) {
    case INTEGRITY_LEVEL_SYSTEM:
      return "S-1-16-16384 System";
    case INTEGRITY_LEVEL_HIGH:
      return "S-1-16-12288 High";
    case INTEGRITY_LEVEL_MEDIUM:
      return "S-1-16-8192 Medium";
    case INTEGRITY_LEVEL_MEDIUM_LOW:
      return "S-1-16-6144 Medium Low";
    case INTEGRITY_LEVEL_LOW:
      return "S-1-16-4096 Low";
    case INTEGRITY_LEVEL_BELOW_LOW:
      return "S-1-16-2048 Below Low";
    case INTEGRITY_LEVEL_UNTRUSTED:
      return "S-1-16-0 Untrusted";
    case INTEGRITY_LEVEL_LAST:
      return "Default";
  }
}

std::wstring GetSidAsString(const base::win::Sid& sid) {
  absl::optional<std::wstring> result = sid.ToSddlString();
  if (!result) {
    DCHECK(false) << "Failed to make sddl string";
    return L"";
  }
  return *result;
}

std::string GetMitigationsAsHex(MitigationFlags mitigations) {
  return base::StringPrintf("%016" PRIx64,
                            base::checked_cast<uint64_t>(mitigations));
}

std::string GetPlatformMitigationsAsHex(MitigationFlags mitigations) {
  DWORD64 platform_flags[2] = {0};
  size_t flags_size = 0;
  sandbox::ConvertProcessMitigationsToPolicy(mitigations, &(platform_flags[0]),
                                             &flags_size);
  DCHECK(flags_size / sizeof(DWORD64) <= 2)
      << "Unexpected mitigation flags size";
  if (flags_size == 2 * sizeof(DWORD64))
    return base::StringPrintf("%016" PRIx64 "%016" PRIx64, platform_flags[0],
                              platform_flags[1]);
  return base::StringPrintf("%016" PRIx64, platform_flags[0]);
}

std::string GetIpcTagAsString(IpcTag service) {
  switch (service) {
    case IpcTag::UNUSED:
      DCHECK(false) << "Unused IpcTag";
      return "Unused";
    case IpcTag::PING1:
      return "Ping1";
    case IpcTag::PING2:
      return "Ping2";
    case IpcTag::NTCREATEFILE:
      return "NtCreateFile";
    case IpcTag::NTOPENFILE:
      return "NtOpenFile";
    case IpcTag::NTQUERYATTRIBUTESFILE:
      return "NtQueryAttributesFile";
    case IpcTag::NTQUERYFULLATTRIBUTESFILE:
      return "NtQueryFullAttributesFile";
    case IpcTag::NTSETINFO_RENAME:
      return "NtSetInfoRename";
    case IpcTag::CREATENAMEDPIPEW:
      return "CreateNamedPipeW";
    case IpcTag::NTOPENTHREAD:
      return "NtOpenThread";
    case IpcTag::NTOPENPROCESS:
      return "NtOpenProcess";
    case IpcTag::NTOPENPROCESSTOKEN:
      return "NtOpenProcessToken";
    case IpcTag::NTOPENPROCESSTOKENEX:
      return "NtOpenProcessTokenEx";
    case IpcTag::GDI_GDIDLLINITIALIZE:
      return "GdiDllInitialize";
    case IpcTag::GDI_GETSTOCKOBJECT:
      return "GetStockObject";
    case IpcTag::USER_REGISTERCLASSW:
      return "RegisterClassW";
    case IpcTag::CREATETHREAD:
      return "CreateThread";
    case IpcTag::NTCREATESECTION:
      return "NtCreateSection";
    case IpcTag::WS2SOCKET:
      return "WSA_Socket";
    case IpcTag::LAST:
      DCHECK(false) << "Unknown IpcTag";
      return "Unknown";
  }
}

std::string GetOpcodeAction(EvalResult action) {
  switch (action) {
    case EVAL_TRUE:
      return "true";
    case EVAL_FALSE:
      return "false";
    case EVAL_ERROR:
      return "error";
    case ASK_BROKER:
      return "askBroker";
    case DENY_ACCESS:
      return "deny";
    case GIVE_READONLY:
      return "readonly";
    case GIVE_ALLACCESS:
      return "allaccess";
    case GIVE_CACHED:
      return "cached";
    case GIVE_FIRST:
      return "first";
    case SIGNAL_ALARM:
      return "alarm";
    case FAKE_SUCCESS:
      return "fakeSuccess";
    case FAKE_ACCESS_DENIED:
      return "fakeDenied";
    case TERMINATE_PROCESS:
      return "terminate";
  }
}

std::string GetStringMatchOperation(int pos, uint32_t options) {
  if (pos == 0) {
    if (options & EXACT_LENGTH)
      return "exact";
    else
      return "prefix";
  } else if (pos < 0) {
    return "scan";
  } else if (pos == kSeekToEnd) {
    return "ends";
  } else {
    DCHECK(false) << "Invalid pos (" << pos << ")";
    return "unknown";
  }
}

std::string GetPolicyOpcode(const PolicyOpcode* opcode, bool continuation) {
  // See |policy_engine_opcodes.cc|.
  uint32_t args[4];
  auto options = opcode->GetOptions();
  auto param = opcode->GetParameter();
  std::string condition;

  if (options & kPolNegateEval)
    condition += "!(";

  switch (opcode->GetID()) {
    case OP_ALWAYS_FALSE:
      condition += "false";
      break;
    case OP_ALWAYS_TRUE:
      condition += "true";
      break;
    case OP_NUMBER_MATCH:
      opcode->GetArgument(1, &args[1]);
      if (args[1] == UINT32_TYPE) {
        opcode->GetArgument(0, &args[0]);
        condition += base::StringPrintf("p[%d] == %x", param, args[0]);
      } else {
        const void* match_ptr = nullptr;
        opcode->GetArgument(0, &match_ptr);
        condition += base::StringPrintf("p[%d] == %p", param, match_ptr);
      }
      break;
    case OP_NUMBER_MATCH_RANGE:
      opcode->GetArgument(0, &args[0]);
      opcode->GetArgument(1, &args[1]);
      condition +=
          base::StringPrintf("%x <= p[%d] <= %x", args[0], param, args[1]);
      break;
    case OP_NUMBER_AND_MATCH:
      opcode->GetArgument(0, &args[0]);
      condition += base::StringPrintf("p[%d] & %x", param, args[0]);
      break;
    case OP_WSTRING_MATCH: {
      int pos;
      opcode->GetArgument(1, &args[1]);  // Length.
      opcode->GetArgument(2, &pos);      // Position.
      opcode->GetArgument(3, &args[3]);  // Options.
      // These are not nul-terminated so we have to wrap them here.
      auto match_string = std::wstring(opcode->GetRelativeString(0), 0,
                                       static_cast<size_t>(args[1]));
      condition += GetStringMatchOperation(pos, args[3]);
      if (args[3] & CASE_INSENSITIVE)
        condition += "_i";
      condition +=
          base::StringPrintf("(p[%d], '%S')", param, match_string.c_str());
    } break;
    case OP_ACTION:
      opcode->GetArgument(0, &args[0]);
      condition += GetOpcodeAction(static_cast<EvalResult>(args[0]));
      break;
    default:
      DCHECK(false) << "Unknown Opcode";
      return "Unknown";
  }

  if (options & kPolNegateEval)
    condition += ")";
  // If there is another rule add a joining token.
  if (continuation) {
    if (options & kPolUseOREval)
      condition += " || ";
    else
      condition += " && ";
  }
  return condition;
}

// Uses |service| to index into |policy_rules| returning a list of opcodes.
base::Value GetPolicyOpcodes(const PolicyGlobal* policy_rules, IpcTag service) {
  base::Value entry(base::Value::Type::LIST);
  PolicyBuffer* policy_buffer =
      policy_rules->entry[static_cast<size_t>(service)];
  // Build up rules and emit when we hit an action.
  std::string cur_rule;
  for (size_t i = 0; i < policy_buffer->opcode_count; i++) {
    const PolicyOpcode* opcode = &policy_buffer->opcodes[i];
    if (opcode->GetID() != OP_ACTION) {
      DCHECK(i + 1 < policy_buffer->opcode_count)
          << "Non-actions should not terminate rules";
      bool peak = policy_buffer->opcodes[i + 1].GetID() != OP_ACTION;
      cur_rule += GetPolicyOpcode(opcode, peak);
    } else {
      cur_rule += " -> ";
      cur_rule += GetPolicyOpcode(opcode, false);
      entry.Append(cur_rule);
      cur_rule.clear();
    }
  }
  return entry;
}

base::Value GetPolicyRules(const PolicyGlobal* policy_rules) {
  DCHECK(policy_rules);
  base::Value results(base::Value::Type::DICTIONARY);

  for (size_t i = 0; i < kMaxServiceCount; i++) {
    if (!policy_rules->entry[i])
      continue;
    IpcTag service = static_cast<IpcTag>(i);
    results.SetKey(GetIpcTagAsString(service),
                   GetPolicyOpcodes(policy_rules, service));
  }

  return results;
}

// HandleMap is just wstrings, nested sets could be empty.
base::Value GetHandlesToClose(const HandleMap& handle_map) {
  base::Value results(base::Value::Type::DICTIONARY);
  for (const auto& kv : handle_map) {
    base::Value entries(base::Value::Type::LIST);
    // kv.second may be an empty map.
    for (const auto& entry : kv.second) {
      entries.Append(base::AsStringPiece16(entry));
    }
    results.SetKey(base::WideToUTF8(kv.first), std::move(entries));
  }
  return results;
}

}  // namespace

// We are a friend of PolicyBase so that we can steal its private members
// quickly in the BrokerServices tracker thread.
PolicyDiagnostic::PolicyDiagnostic(PolicyBase* policy) {
  DCHECK(policy);
  // TODO(crbug/997273) Add more fields once webui plumbing is complete.
  {
    base::AutoLock lock(policy->lock_);
    for (auto&& target_process : policy->targets_) {
      process_ids_.push_back(
          base::strict_cast<uint32_t>(target_process->ProcessId()));
    }
  }
  lockdown_level_ = policy->lockdown_level_;
  job_level_ = policy->job_level_;

  // Select the final integrity level.
  if (policy->delayed_integrity_level_ == INTEGRITY_LEVEL_LAST)
    desired_integrity_level_ = policy->integrity_level_;
  else
    desired_integrity_level_ = policy->delayed_integrity_level_;

  desired_mitigations_ = policy->mitigations_ | policy->delayed_mitigations_;

  if (policy->app_container_) {
    app_container_sid_.emplace(policy->app_container_->GetPackageSid().Clone());
    for (const auto& sid : policy->app_container_->GetCapabilities()) {
      capabilities_.push_back(sid.Clone());
    }
    for (const auto& sid :
         policy->app_container_->GetImpersonationCapabilities()) {
      initial_capabilities_.push_back(sid.Clone());
    }

    app_container_type_ = policy->app_container_->GetAppContainerType();
  }

  if (policy->policy_) {
    size_t policy_mem_size = policy->policy_->data_size + sizeof(PolicyGlobal);
    policy_rules_.reset(
        static_cast<sandbox::PolicyGlobal*>(::operator new(policy_mem_size)));
    memcpy(policy_rules_.get(), policy->policy_, policy_mem_size);
    // Fixup pointers (see |PolicyGlobal| in policy_low_level.h).
    PolicyBuffer** original_entries = policy->policy_->entry;
    PolicyBuffer** copy_base = policy_rules_->entry;
    for (size_t i = 0; i < kMaxServiceCount; i++) {
      if (policy_rules_->entry[i]) {
        policy_rules_->entry[i] = reinterpret_cast<PolicyBuffer*>(
            reinterpret_cast<char*>(copy_base) +
            (reinterpret_cast<char*>(original_entries[i]) -
             reinterpret_cast<char*>(original_entries)));
      }
    }
  }
  is_csrss_connected_ = policy->is_csrss_connected_;
  handles_to_close_.insert(policy->handle_closer_.handles_to_close_.begin(),
                           policy->handle_closer_.handles_to_close_.end());
}

PolicyDiagnostic::~PolicyDiagnostic() = default;

const char* PolicyDiagnostic::JsonString() {
  // Lazily constructs json_string_.
  if (json_string_)
    return json_string_->c_str();

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kProcessIds, ProcessIdList(process_ids_));
  value.SetKey(kLockdownLevel,
               base::Value(GetTokenLevelInEnglish(lockdown_level_)));
  value.SetKey(kJobLevel, base::Value(GetJobLevelInEnglish(job_level_)));
  value.SetKey(
      kDesiredIntegrityLevel,
      base::Value(GetIntegrityLevelInEnglish(desired_integrity_level_)));
  value.SetKey(kDesiredMitigations,
               base::Value(GetMitigationsAsHex(desired_mitigations_)));
  value.SetKey(kPlatformMitigations,
               base::Value(GetPlatformMitigationsAsHex(desired_mitigations_)));

  if (app_container_sid_) {
    value.SetStringKey(
        kAppContainerSid,
        base::AsStringPiece16(GetSidAsString(*app_container_sid_)));
    std::vector<base::Value> caps;
    for (const auto& sid : capabilities_) {
      auto sid_value = base::Value(base::AsStringPiece16(GetSidAsString(sid)));
      caps.push_back(std::move(sid_value));
    }
    if (!caps.empty()) {
      value.SetKey(kAppContainerCapabilities, base::Value(std::move(caps)));
    }
    std::vector<base::Value> imp_caps;
    for (const auto& sid : initial_capabilities_) {
      auto sid_value = base::Value(base::AsStringPiece16(GetSidAsString(sid)));
      imp_caps.push_back(std::move(sid_value));
    }
    if (!imp_caps.empty()) {
      value.SetKey(kAppContainerInitialCapabilities,
                   base::Value(std::move(imp_caps)));
    }

    if (app_container_type_ == AppContainerType::kLowbox)
      value.SetStringKey(kLowboxSid, base::AsStringPiece16(
                                         GetSidAsString(*app_container_sid_)));
  }

  if (policy_rules_)
    value.SetKey(kPolicyRules, GetPolicyRules(policy_rules_.get()));

  value.SetStringKey(kDisconnectCsrss,
                     is_csrss_connected_ ? kDisabled : kEnabled);
  if (!handles_to_close_.empty())
    value.SetKey(kHandlesToClose, GetHandlesToClose(handles_to_close_));

  auto json_string = std::make_unique<std::string>();
  JSONStringValueSerializer to_json(json_string.get());
  CHECK(to_json.Serialize(value));
  json_string_ = std::move(json_string);
  return json_string_->c_str();
}

}  // namespace sandbox
