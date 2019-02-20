// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_task.h"
#include "third_party/blink/renderer/core/content_capture/content_holder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

namespace blink {

class Document;
class Node;

// This class is used to create the NodeHolder, and start the ContentCaptureTask
// when necessary. The ContentCaptureManager is owned by Document.
class CORE_EXPORT ContentCaptureManager
    : public GarbageCollectedFinalized<ContentCaptureManager>,
      public ContentCaptureTask::Delegate {
 public:
  ContentCaptureManager(Document& document, NodeHolder::Type type);
  ~ContentCaptureManager() override;

  // Creates and returns NodeHolder for the given |node|, and schedules
  // ContentCaptureTask if it isn't already scheduled.
  // Can't use const Node& for parameter, because |node| is passed to
  // DOMNodeIds::IdForNode(Node*).
  NodeHolder GetNodeHolder(Node& node);

  // Invokes when the |node_holder| asscociated LayoutText will be destroyed.
  void OnLayoutTextWillBeDestroyed(NodeHolder node_holder);

  // Invokes when scroll position was changed.
  void OnScrollPositionChanged();

  // Invokes when the document shutdown.
  void Shutdown();

  // ContentCaptureTask::Delegate, these methods have to be in this class
  // because the node stores in HeapHashSet.
  bool HasSent(const Node& node) override;
  void OnSent(const Node& node) override;

  void EnableContentCaptureTask() { should_capture_content_ = true; }

  virtual void Trace(blink::Visitor*);

 protected:
  virtual scoped_refptr<ContentCaptureTask> CreateContentCaptureTask();

 private:
  void NotifyNodeDetached(const NodeHolder& node_holder);
  void ScheduleTask(ContentCaptureTask::ScheduleReason reason);

  // Indicates if the ContentCaptureTask should be started.
  bool should_capture_content_ = false;

  scoped_refptr<ContentCaptureTask> content_capture_idle_task_;

  Member<Document> document_;

  // Indicates the NodeHolder::Type should be used.
  NodeHolder::Type node_holder_type_;

  // Indicates if the first NodeHolder is created.
  bool first_node_holder_created_ = false;

  // The list of nodes that have been sent, only used when
  // |node_identification_method| is kNodeID.
  HeapHashSet<WeakMember<const Node>> sent_nodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_MANAGER_H_
