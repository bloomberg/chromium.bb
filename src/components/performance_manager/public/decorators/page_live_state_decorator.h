// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PageNode;
class PageLiveStateObserver;

// Used to record some live state information about the PageNode.
// All the functions that take a WebContents* as a parameter should only be
// called from the UI thread, the event will be forwarded to the corresponding
// PageNode on the Performance Manager's sequence.
class PageLiveStateDecorator
    : public GraphOwnedDefaultImpl,
      public NodeDataDescriberDefaultImpl {
 public:
  class Data;

  // This object should only be used via its static methods.
  PageLiveStateDecorator() = default;
  ~PageLiveStateDecorator() override = default;
  PageLiveStateDecorator(const PageLiveStateDecorator& other) = delete;
  PageLiveStateDecorator& operator=(const PageLiveStateDecorator&) = delete;

  // Must be called when the connected to USB device state changes.
  static void OnIsConnectedToUSBDeviceChanged(content::WebContents* contents,
                                              bool is_connected_to_usb_device);

  // Must be called when the connected to Bluetooth device state changes.
  static void OnIsConnectedToBluetoothDeviceChanged(
      content::WebContents* contents,
      bool is_connected_to_bluetooth_device);

  // Functions that should be called by a MediaStreamCaptureIndicator::Observer.
  static void OnIsCapturingVideoChanged(content::WebContents* contents,
                                        bool is_capturing_video);
  static void OnIsCapturingAudioChanged(content::WebContents* contents,
                                        bool is_capturing_audio);
  static void OnIsBeingMirroredChanged(content::WebContents* contents,
                                       bool is_being_mirrored);
  static void OnIsCapturingWindowChanged(content::WebContents* contents,
                                         bool is_capturing_window);
  static void OnIsCapturingDisplayChanged(content::WebContents* contents,
                                          bool is_capturing_display);

  // Set the auto discardable property. This indicates whether or not the page
  // can be discarded during an intervention.
  static void SetIsAutoDiscardable(content::WebContents* contents,
                                   bool is_auto_discardable);

  static void SetWasDiscarded(content::WebContents* contents,
                              bool was_discarded);

 private:
  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* node) const override;
};

class PageLiveStateDecorator::Data {
 public:
  Data();
  virtual ~Data();
  Data(const Data& other) = delete;
  Data& operator=(const Data&) = delete;

  void AddObserver(PageLiveStateObserver* observer);
  void RemoveObserver(PageLiveStateObserver* observer);

  virtual bool IsConnectedToUSBDevice() const = 0;
  virtual bool IsConnectedToBluetoothDevice() const = 0;
  virtual bool IsCapturingVideo() const = 0;
  virtual bool IsCapturingAudio() const = 0;
  virtual bool IsBeingMirrored() const = 0;
  virtual bool IsCapturingWindow() const = 0;
  virtual bool IsCapturingDisplay() const = 0;
  virtual bool IsAutoDiscardable() const = 0;
  virtual bool WasDiscarded() const = 0;

  static const Data* FromPageNode(const PageNode* page_node);
  static Data* GetOrCreateForPageNode(const PageNode* page_node);

  virtual void SetIsConnectedToUSBDeviceForTesting(bool value) = 0;
  virtual void SetIsConnectedToBluetoothDeviceForTesting(bool value) = 0;
  virtual void SetIsCapturingVideoForTesting(bool value) = 0;
  virtual void SetIsCapturingAudioForTesting(bool value) = 0;
  virtual void SetIsBeingMirroredForTesting(bool value) = 0;
  virtual void SetIsCapturingWindowForTesting(bool value) = 0;
  virtual void SetIsCapturingDisplayForTesting(bool value) = 0;
  virtual void SetIsAutoDiscardableForTesting(bool value) = 0;
  virtual void SetWasDiscardedForTesting(bool value) = 0;

 protected:
  base::ObserverList<PageLiveStateObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

class PageLiveStateObserver : public base::CheckedObserver {
 public:
  PageLiveStateObserver();
  ~PageLiveStateObserver() override;
  PageLiveStateObserver(const PageLiveStateObserver& other) = delete;
  PageLiveStateObserver& operator=(const PageLiveStateObserver&) = delete;

  virtual void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) = 0;
  virtual void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) = 0;
  virtual void OnIsCapturingVideoChanged(const PageNode* page_node) = 0;
  virtual void OnIsCapturingAudioChanged(const PageNode* page_node) = 0;
  virtual void OnIsBeingMirroredChanged(const PageNode* page_node) = 0;
  virtual void OnIsCapturingWindowChanged(const PageNode* page_node) = 0;
  virtual void OnIsCapturingDisplayChanged(const PageNode* page_node) = 0;
  virtual void OnIsAutoDiscardableChanged(const PageNode* page_node) = 0;
  virtual void OnWasDiscardedChanged(const PageNode* page_node) = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_
