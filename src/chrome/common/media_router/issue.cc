// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/issue.h"

#include "base/atomic_sequence_num.h"

namespace media_router {

namespace {
// ID generator for Issue.
base::AtomicSequenceNumber g_next_issue_id;
}

IssueInfo::IssueInfo()
    : default_action(IssueInfo::Action::DISMISS),
      severity(IssueInfo::Severity::NOTIFICATION),
      is_blocking(false),
      help_page_id(IssueInfo::kUnknownHelpPageId) {}

IssueInfo::IssueInfo(const std::string& title,
                     const Action default_action,
                     Severity severity)
    : title(title),
      default_action(default_action),
      severity(severity),
      is_blocking(severity == IssueInfo::Severity::FATAL),
      help_page_id(IssueInfo::kUnknownHelpPageId) {}

IssueInfo::IssueInfo(const IssueInfo& other) = default;

IssueInfo::~IssueInfo() = default;

IssueInfo& IssueInfo::operator=(const IssueInfo& other) = default;

bool IssueInfo::operator==(const IssueInfo& other) const {
  return title == other.title && default_action == other.default_action &&
         severity == other.severity && message == other.message &&
         secondary_actions == other.secondary_actions &&
         route_id == other.route_id && is_blocking == other.is_blocking &&
         help_page_id == other.help_page_id;
}

Issue::Issue(const IssueInfo& info)
    : id_(g_next_issue_id.GetNext()), info_(info) {}

Issue::~Issue() = default;

}  // namespace media_router
