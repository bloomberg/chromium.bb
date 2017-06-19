// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/process_stats_sender.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "remoting/host/process_stats_agent.h"
#include "remoting/host/process_stats_util.h"

namespace remoting {

ProcessStatsSender::ProcessStatsSender(
    protocol::ProcessStatsStub* host_stats_stub,
    base::TimeDelta interval,
    std::initializer_list<ProcessStatsAgent*> agents)
    : host_stats_stub_(host_stats_stub),
      agents_(agents),
      thread_checker_() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(host_stats_stub_);
  DCHECK(interval > base::TimeDelta());
  DCHECK(!agents_.empty());

  timer_.Start(FROM_HERE, interval, this, &ProcessStatsSender::ReportUsage);
}

ProcessStatsSender::~ProcessStatsSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
  timer_.Stop();
}

void ProcessStatsSender::ReportUsage() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<protocol::ProcessResourceUsage> usages;
  for (auto* const agent : agents_) {
    DCHECK(agent);
    protocol::ProcessResourceUsage usage = agent->GetResourceUsage();
    if (!IsEmptyProcessResourceUsage(usage)) {
      usages.push_back(std::move(usage));
    }
  }

  host_stats_stub_->OnProcessStats(AggregateProcessResourceUsage(usages));
}

}  // namespace remoting
