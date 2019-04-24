// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_QUERYABLE_DATA_BINDINGS_H_
#define FUCHSIA_RUNNERS_CAST_QUERYABLE_DATA_BINDINGS_H_

#include <vector>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

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
      chromium::web::Frame* frame,
      fidl::InterfaceHandle<chromium::cast::QueryableData> service);
  ~QueryableDataBindings();

 private:
  void OnEntriesReceived(
      std::vector<chromium::cast::QueryableDataEntry> values);

  // The callbacks of any asynchronous calls made to |frame_| should ensure that
  // |this| is valid before using it (e.g. via a WeakPtr).
  chromium::web::Frame* const frame_;

  chromium::cast::QueryableDataPtr service_;

  DISALLOW_COPY_AND_ASSIGN(QueryableDataBindings);
};

#endif  // FUCHSIA_RUNNERS_CAST_QUERYABLE_DATA_BINDINGS_H_
