// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_TRACKER_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_TRACKER_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "net/url_request/url_request_status.h"

class GURL;

namespace net {
class URLRequestJob;

// This class maintains a list of active URLRequestJobs for debugging purposes.
// This allows us to warn on leaked jobs and also allows an observer to track
// what is happening, for example, for the network status monitor.
//
// NOTE: URLRequest is single-threaded, so this class should only be used
// onthe same thread where all of the application's URLRequest calls are
// made.
//
class URLRequestJobTracker {
 public:
  typedef std::vector<URLRequestJob*> JobList;
  typedef JobList::const_iterator JobIterator;

  // The observer's methods are called on the thread that called AddObserver.
  class JobObserver {
   public:
    virtual ~JobObserver() {}

    // Called after the given job has been added to the list
    virtual void OnJobAdded(URLRequestJob* job) = 0;

    // Called after the given job has been removed from the list
    virtual void OnJobRemoved(URLRequestJob* job) = 0;

    // Called when the given job has completed, before notifying the request
    virtual void OnJobDone(URLRequestJob* job,
                           const URLRequestStatus& status) = 0;

    // Called when the given job is about to follow a redirect to the given
    // new URL. The redirect type is given in status_code
    virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                               int status_code) = 0;

    // Called when a new chunk of unfiltered bytes has been read for
    // the given job. |byte_count| is the number of bytes for that
    // read event only. |buf| is a pointer to the data buffer that
    // contains those bytes. The data in |buf| is only valid for the
    // duration of the OnBytesRead callback.
    virtual void OnBytesRead(URLRequestJob* job, const char* buf,
                             int byte_count) = 0;
  };

  URLRequestJobTracker();
  ~URLRequestJobTracker();

  // adds or removes an observer from the list.  note, these methods should
  // only be called on the same thread where URLRequest objects are used.
  void AddObserver(JobObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(JobObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // adds or removes the job from the active list, should be called by the
  // job constructor and destructor. Note: don't use "AddJob" since that
  // is #defined by windows.h :(
  void AddNewJob(URLRequestJob* job);
  void RemoveJob(URLRequestJob* job);

  // Job status change notifications
  void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  void OnJobRedirect(URLRequestJob* job, const GURL& location,
                     int status_code);

  // Bytes read notifications.
  void OnBytesRead(URLRequestJob* job, const char* buf, int byte_count);

  // allows iteration over all active jobs
  JobIterator begin() const {
    return active_jobs_.begin();
  }
  JobIterator end() const {
    return active_jobs_.end();
  }

 private:
  ObserverList<JobObserver> observers_;
  JobList active_jobs_;
};

extern URLRequestJobTracker g_url_request_job_tracker;

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_TRACKER_H_
