// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_QUERYABLE_DATA_BINDINGS_H_
#define FUCHSIA_RUNNERS_CAST_QUERYABLE_DATA_BINDINGS_H_

#include <fuchsia/web/cpp/fidl.h>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

// Adds JavaScript functions to a Frame for querying platform values from the
// Agent.
class QueryableDataBindings {
 public:
  // |frame|: The Frame which will receive the bindings and data.
  //          Must outlive |this|.
  // |service|: The QueryableData service which will supply |this| with data.
  //            Values from |service| will be read once and injected into
  //            |frame|. Any changes to |service|'s values will not be
  //            propagated to the Frame for the lifetime of |this|.
  QueryableDataBindings(
      fuchsia::web::Frame* frame,
      fidl::InterfaceHandle<chromium::cast::QueryableData> service);
  ~QueryableDataBindings();

 private:
  // Allows QueryableDataEntry to be stored in a flat_set.
  struct QueryableDataEntryLess {
    bool operator()(const chromium::cast::QueryableDataEntry& lhs,
                    const chromium::cast::QueryableDataEntry& rhs) const;
  };

  // Takes the initial list of QueryableData entries, or a list of updated
  // entries, and propagates the information to |frame_|'s script context.
  void OnEntriesReceived(
      std::vector<chromium::cast::QueryableDataEntry> new_entries);

  // The callbacks of any asynchronous calls made to |frame_| should ensure that
  // |this| is valid before using it (e.g. via a WeakPtr).
  fuchsia::web::Frame* const frame_;

  chromium::cast::QueryableDataPtr service_;
  base::flat_set<chromium::cast::QueryableDataEntry, QueryableDataEntryLess>
      cached_entries_;

  DISALLOW_COPY_AND_ASSIGN(QueryableDataBindings);
};

#endif  // FUCHSIA_RUNNERS_CAST_QUERYABLE_DATA_BINDINGS_H_
