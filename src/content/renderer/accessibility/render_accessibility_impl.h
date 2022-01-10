// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_

#include <list>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/renderer/plugin_ax_tree_source.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/accessibility/blink_ax_tree_source.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/rect_f.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace blink {
class WebDocument;
}  // namespace blink

namespace ui {
struct AXActionData;
class AXActionTarget;
struct AXEvent;
}

namespace ukm {
class MojoUkmRecorder;
}

namespace content {

class AXImageAnnotator;
class RenderFrameImpl;
class RenderAccessibilityManager;

using BlinkAXTreeSerializer = ui::AXTreeSerializer<blink::WebAXObject>;

struct AXDirtyObject {
  AXDirtyObject();
  AXDirtyObject(const AXDirtyObject& other);
  ~AXDirtyObject();
  blink::WebAXObject obj;
  ax::mojom::EventFrom event_from;
  ax::mojom::Action event_from_action;
  std::vector<ui::AXEventIntent> event_intents;
};

// The browser process implements native accessibility APIs, allowing assistive
// technology (e.g., screen readers, magnifiers) to access and control the web
// contents with high-level APIs. These APIs are also used by automation tools,
// and Windows 8 uses them to determine when the on-screen keyboard should be
// shown.
//
// An instance of this class belongs to the RenderAccessibilityManager object.
// Accessibility is initialized based on the ui::AXMode passed from the browser
// process to the manager object; it lazily starts as Off or EditableTextOnly
// depending on the operating system, and switches to Complete if assistive
// technology is detected or a flag is set.
//
// A tree of accessible objects is built here and sent to the browser process;
// the browser process maintains this as a tree of platform-native accessible
// objects that can be used to respond to accessibility requests from other
// processes.
//
// This class implements complete accessibility support for assistive
// technology. It turns on Blink's accessibility code and sends a serialized
// representation of that tree whenever it changes. It also handles requests
// from the browser to perform accessibility actions on nodes in the tree (e.g.,
// change focus, or click on a button).
class CONTENT_EXPORT RenderAccessibilityImpl : public RenderAccessibility,
                                               public RenderFrameObserver {
 public:
  RenderAccessibilityImpl(
      RenderAccessibilityManager* const render_accessibility_manager,
      RenderFrameImpl* const render_frame,
      ui::AXMode mode);

  RenderAccessibilityImpl(const RenderAccessibilityImpl&) = delete;
  RenderAccessibilityImpl& operator=(const RenderAccessibilityImpl&) = delete;

  ~RenderAccessibilityImpl() override;

  ui::AXMode GetAccessibilityMode() {
    return tree_source_->accessibility_mode();
  }

  // RenderAccessibility implementation.
  int GenerateAXID() override;
  void SetPluginTreeSource(PluginAXTreeSource* source) override;
  void OnPluginRootNodeUpdated() override;
  void ShowPluginContextMenu() override;

  // RenderFrameObserver implementation.
  void DidCreateNewDocument() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void AccessibilityModeChanged(const ui::AXMode& mode) override;

  void HitTest(const gfx::Point& point,
               ax::mojom::Event event_to_fire,
               int request_id,
               mojom::RenderAccessibility::HitTestCallback callback);
  void PerformAction(const ui::AXActionData& data);
  void Reset(int32_t reset_token);

  // Called when an accessibility notification occurs in Blink.
  void HandleAXEvent(const ui::AXEvent& event);
  void MarkWebAXObjectDirty(
      const blink::WebAXObject& obj,
      bool subtree,
      ax::mojom::Action event_from_action = ax::mojom::Action::kNone,
      std::vector<ui::AXEventIntent> event_intents = {},
      ax::mojom::Event event_type = ax::mojom::Event::kNone);

  // Returns the main top-level document for this page, or NULL if there's
  // no view or frame.
  blink::WebDocument GetMainDocument();

  // Returns the page language.
  std::string GetLanguage();

  // Access the UKM recorder.
  ukm::MojoUkmRecorder* ukm_recorder() const { return ukm_recorder_.get(); }

  // Called when the renderer has closed the connection to reset the state
  // machine.
  void ConnectionClosed();

 protected:
  // Send queued events from the renderer to the browser.
  void SendPendingAccessibilityEvents();

  // Check the entire accessibility tree to see if any nodes have
  // changed location, by comparing their locations to the cached
  // versions. If any have moved, send an IPC with the new locations.
  void SendLocationChanges();

  // Return true if the event indicates that the current batch of changes
  // should be processed immediately in order for the user to get fast
  // feedback, e.g. for navigation or data entry activities.
  bool IsImmediateProcessingRequiredForEvent(const ui::AXEvent&) const;

  // Get the amount of time, in ms, that event processing should be deferred
  // in order to more efficiently batch changes.
  int GetDeferredEventsDelay();

 private:
  enum class EventScheduleMode { kDeferEvents, kProcessEventsImmediately };

  enum class EventScheduleStatus {
    // Events have been scheduled with a delay, but have not been sent.
    kScheduledDeferred,
    // Events have been scheduled without a delay, but have not been sent.
    kScheduledImmediate,
    // Events have been sent, waiting for callback.
    kWaitingForAck,
    // Events are not scheduled and we are not waiting for an ack.
    kNotWaiting
  };

  // Add an AXDirtyObject to the dirty_objects_ queue.
  // Returns an iterator pointing just after the newly inserted object.
  std::list<std::unique_ptr<AXDirtyObject>>::iterator EnqueueDirtyObject(
      const blink::WebAXObject& obj,
      ax::mojom::EventFrom event_from,
      ax::mojom::Action event_from_action,
      std::vector<ui::AXEventIntent> event_intents,
      std::list<std::unique_ptr<AXDirtyObject>>::iterator insertion_point);

  // Callback that will be called from the browser upon handling the message
  // previously sent to it via SendPendingAccessibilityEvents().
  void OnAccessibilityEventsHandled();

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Handlers for messages from the browser to the renderer.
  void OnLoadInlineTextBoxes(const ui::AXActionTarget* target);
  void OnGetImageData(const ui::AXActionTarget* target,
                      const gfx::Size& max_size);
  void AddPluginTreeToUpdate(ui::AXTreeUpdate* update,
                             bool invalidate_plugin_subtree);

  // Creates and takes ownership of an instance of the class that automatically
  // labels images for accessibility.
  void CreateAXImageAnnotator();

  // Automatically labels images for accessibility if the accessibility mode for
  // this feature is turned on, otherwise stops automatic labeling and removes
  // any automatic annotations that might have been added before.
  void StartOrStopLabelingImages(ui::AXMode old_mode, ui::AXMode new_mode);

  // Marks all AXObjects with the given role in the current tree dirty.
  void MarkAllAXObjectsDirty(ax::mojom::Role role,
                             ax::mojom::Action event_from_action);

  void Scroll(const ui::AXActionTarget* target,
              ax::mojom::Action scroll_action);

  // Whether an event should mark its associated object dirty.
  bool ShouldSerializeNodeForEvent(const blink::WebAXObject& obj,
                                   const ui::AXEvent& event) const;

  // If we are calling this from a task, scheduling is allowed even if there is
  // a running task
  void ScheduleSendPendingAccessibilityEvents(
      bool scheduling_from_task = false);
  void AddImageAnnotationDebuggingAttributes(
      const std::vector<ui::AXTreeUpdate>& updates);

  // Returns the document for the active popup if any.
  blink::WebDocument GetPopupDocument();

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  blink::WebAXObject GetPluginRoot();

  // Cancels scheduled events that are not yet in flight
  void CancelScheduledEvents();

  // Sends the URL-keyed metrics for the maximum amount of time spent in
  // SendPendingAccessibilityEvents if they meet the minimum criteria for
  // sending.
  void MaybeSendUKM();

  // Reset all of the UKM data. This can be called after sending UKM data,
  // or after navigating to a new page when any previous data will no
  // longer be valid.
  void ResetUKMData();

  bool SerializeUpdatesAndEvents(blink::WebDocument document,
                                 blink::WebAXObject root,
                                 std::vector<ui::AXEvent>& events,
                                 std::vector<ui::AXTreeUpdate>& updates,
                                 bool invalidate_plugin_subtree);

  // The initial accessibility tree root still needs to be created. Like other
  // accessible objects, it must be created when layout is clean.
  bool needs_initial_ax_tree_root_ = true;

  // The RenderAccessibilityManager that owns us.
  RenderAccessibilityManager* render_accessibility_manager_;

  // The associated RenderFrameImpl by means of the RenderAccessibilityManager.
  RenderFrameImpl* render_frame_;

  // This keeps accessibility enabled as long as it lives.
  std::unique_ptr<blink::WebAXContext> ax_context_;

  // Manages the automatic image annotations, if enabled.
  std::unique_ptr<AXImageAnnotator> ax_image_annotator_;

  // Events from Blink are collected until they are ready to be
  // sent to the browser.
  std::vector<ui::AXEvent> pending_events_;

  // Objects that need to be re-serialized, the next time
  // we send an event bundle to the browser - but don't specifically need
  // an event fired.
  std::list<std::unique_ptr<AXDirtyObject>> dirty_objects_;

  // The adapter that exposes Blink's accessibility tree to AXTreeSerializer.
  std::unique_ptr<BlinkAXTreeSource> tree_source_;

  // The serializer that sends accessibility messages to the browser process.
  std::unique_ptr<BlinkAXTreeSerializer> serializer_;

  using PluginAXTreeSerializer = ui::AXTreeSerializer<const ui::AXNode*>;
  std::unique_ptr<PluginAXTreeSerializer> plugin_serializer_;
  PluginAXTreeSource* plugin_tree_source_;
  blink::WebAXObject plugin_host_node_;

  // Current event scheduling status
  EventScheduleStatus event_schedule_status_;

  // Nonzero if the browser requested we reset the accessibility state.
  // We need to return this token in the next IPC.
  int reset_token_;

  // Whether or not we've injected a stylesheet in this document
  // (only when debugging flags are enabled, never under normal circumstances).
  bool has_injected_stylesheet_ = false;

  // We defer events to improve performance during the initial page load.
  EventScheduleMode event_schedule_mode_;

  // Whether we should highlight annotation results visually on the page
  // for debugging.
  bool image_annotation_debugging_ = false;

  // The specified page language, or empty if unknown.
  std::string page_language_;

  // The URL-keyed metrics recorder interface.
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // The longest amount of time spent serializing the accessibility tree
  // in SendPendingAccessibilityEvents. This is periodically uploaded as
  // a UKM and then reset.
  base::TimeDelta slowest_serialization_time_;

  // The amount of time since the last UKM upload.
  std::unique_ptr<base::ElapsedTimer> ukm_timer_;

  // The UKM Source ID that corresponds to the web page represented by
  // slowest_serialization_ms_. We report UKM before the user navigates
  // away, or every few minutes.
  ukm::SourceId last_ukm_source_id_;

  // So we can queue up tasks to be executed later.
  base::WeakPtrFactory<RenderAccessibilityImpl>
      weak_factory_for_pending_events_{this};

  friend class AXImageAnnotatorTest;
  friend class PluginActionHandlingTest;
  friend class RenderAccessibilityImplTest;
  friend class RenderAccessibilityImplUKMTest;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
