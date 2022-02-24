// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

#include "base/callback_forward.h"

using OnMetricsReportingCallbackType = base::OnceCallback<void(bool)>;

// Specifies from where a change to the metrics reporting state was made. When
// metrics reporting is enabled from a settings page, histogram data that was
// collected while metrics reporting was disabled should be cleared (marked as
// reported) so as to not include them in the next log.
// TODO(crbug.com/1296618): Make all call sites pass an appropriate value, and
// remove |kUnknown|. Right now, |kUnknown| is used as a placeholder value while
// call sites are being migrated.
enum class ChangeMetricsReportingStateCalledFrom {
  kUnknown,
  kUiSettings,
};

// Changes metrics reporting state without caring about the success of the
// change. |called_from| should be set to |kUiSettings| when enabling metrics
// from a settings page (to mark histogram data collected while metrics
// reporting was disabled as reported so as to not include them in the next
// log). If |called_from| is set to anything else (|kUnknown| by default), then
// metrics will not be cleared when enabling metrics reporting.
void ChangeMetricsReportingState(
    bool enabled,
    ChangeMetricsReportingStateCalledFrom called_from =
        ChangeMetricsReportingStateCalledFrom::kUnknown);

// Changes metrics reporting state to the new value of |enabled|. Starts or
// stops the metrics service based on the new state and then runs |callback_fn|
// (which can be null) with the updated state (as the operation may fail). On
// platforms other than CrOS and Android, also updates the underlying pref.
// |called_from| should be set to |kUiSettings| when enabling metrics from a
// settings page (to mark histogram data collected while metrics reporting was
// disabled as reported so as to not include them in the next log). If
// |called_from| is set to anything else (|kUnknown| by default), then metrics
// will not be cleared when enabling metrics reporting.
// TODO(https://crbug.com/880936): Support setting the pref on all platforms.
void ChangeMetricsReportingStateWithReply(
    bool enabled,
    OnMetricsReportingCallbackType callback_fn,
    ChangeMetricsReportingStateCalledFrom called_from =
        ChangeMetricsReportingStateCalledFrom::kUnknown);

// Update metrics prefs on a permission (opt-in/out) change. When opting out,
// this clears various client ids. When opting in, this resets saving crash
// prefs, so as not to trigger upload of various stale data. |called_from|
// should be set to |kUiSettings| when enabling metrics from a settings page (to
// mark histogram data collected while metrics reporting was disabled as
// reported so as to not include them in the next log). If |called_from| is set
// to anything else (|kUnknown| by default), then metrics will not be cleared
// when enabling metrics reporting.
void UpdateMetricsPrefsOnPermissionChange(
    bool metrics_enabled,
    ChangeMetricsReportingStateCalledFrom called_from =
        ChangeMetricsReportingStateCalledFrom::kUnknown);

// Propagates the state of metrics reporting pref (which may be policy
// managed) to GoogleUpdateSettings.
void ApplyMetricsReportingPolicy();

// Returns whether MetricsReporting can be modified by the user (except
// Android).
bool IsMetricsReportingPolicyManaged();

// Clears previously collected metrics data. Used when enabling metrics to
// prevent data collected while metrics reporting was disabled from being
// included in the next log. Note that histogram data is not discarded. Rather,
// they are just marked as being already reported.
void ClearPreviouslyCollectedMetricsData();

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
