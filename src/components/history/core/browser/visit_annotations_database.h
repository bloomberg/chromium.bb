// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_

#include <vector>

#include "components/history/core/browser/history_types.h"

namespace sql {
class Database;
}  // namespace sql

namespace history {

struct VisitContentAnnotations;

// A database that stores visit content & context annotations. A
// `VisitAnnotationsDatabase` must also be a `VisitDatabase`, as this joins with
// the `visits` table. The `content_annotations` and `context_annotations` use
// `visit_id` as their primary key; each row in the `visits` table will be
// associated with 0 or 1 rows in each annotation table.
class VisitAnnotationsDatabase {
 public:
  // Must call `InitAnnotationsTables()` before using any other part of this
  // class.
  VisitAnnotationsDatabase();
  VisitAnnotationsDatabase(const VisitAnnotationsDatabase&) = delete;
  VisitAnnotationsDatabase& operator=(const VisitAnnotationsDatabase&) = delete;
  virtual ~VisitAnnotationsDatabase();

  // Adds a line to the content annotations table with the given information.
  void AddContentAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentAnnotations& visit_content_annotations);

  // Adds a line to the context annotation table with the given information..
  void AddContextAnnotationsForVisit(
      VisitID visit_id,
      const VisitContextAnnotations& visit_context_annotations);

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The content annotations for the visit must exist.
  void UpdateContentAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentAnnotations& visit_content_annotations);

  // Query for a VisitContentAnnotations given a `VisitID` and returns it if
  // present. If it's present, `out_content_annotations` will be filled with the
  // expected data; otherwise, `out_content_annotations` won't be changed.
  bool GetContentAnnotationsForVisit(
      VisitID visit_id,
      VisitContentAnnotations* out_content_annotations);

  // Get the `max_results` most recent `AnnotatedVisitRow`s.
  std::vector<AnnotatedVisitRow> GetAnnotatedVisits(int max_results);

  // Gets all the context annotation rows for testing.
  std::vector<AnnotatedVisitRow> GetAllContextAnnotationsForTesting();

  // Deletes the content & context annotations associated with `visit_id`. This
  // will also delete any associated annotations usage data. If no annotations
  // exist for the `VisitId`, this is a no-op.
  void DeleteAnnotationsForVisit(VisitID visit_id);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Creates the tables used by this class if necessary. Returns true on
  // success.
  bool InitVisitAnnotationsTables();

  // Deletes all the annotations tables, returning true on success.
  bool DropVisitAnnotationsTables();

  // Called by the derived classes to migrate the older visits table's
  // floc_allowed (for historical reasons named "publicly_routable" in the
  // schema) column to the content_annotations table, from a BOOLEAN filed to
  // a bit masking INTEGER filed.
  bool MigrateFlocAllowedToAnnotationsTable();

  // Replaces `cluster_visits` with `context_annotations`. Besides the name
  // change, the new table drops 2 columns: cluster_visit_id (obsolete) and
  // url_id (redundant); and renames 1 column:
  // cluster_visit_context_signal_bitmask to context_annotation_flags.
  bool MigrateReplaceClusterVisitsTable();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
