// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/decorators/decorators_utils.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {

// Private implementation of the node attached data. This keeps the complexity
// out of the header file.
class PageLiveStateDataImpl
    : public PageLiveStateDecorator::Data,
      public NodeAttachedDataImpl<PageLiveStateDataImpl> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};
  ~PageLiveStateDataImpl() override = default;
  PageLiveStateDataImpl(const PageLiveStateDataImpl& other) = delete;
  PageLiveStateDataImpl& operator=(const PageLiveStateDataImpl&) = delete;

  // PageLiveStateDecorator::Data:
  bool IsConnectedToUSBDevice() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_connected_to_usb_device_;
  }
  bool IsConnectedToBluetoothDevice() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_connected_to_bluetooth_device_;
  }
  bool IsCapturingVideo() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_video_;
  }
  bool IsCapturingAudio() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_audio_;
  }
  bool IsBeingMirrored() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_being_mirrored_;
  }
  bool IsCapturingWindow() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_window_;
  }
  bool IsCapturingDisplay() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_display_;
  }
  bool IsAutoDiscardable() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_auto_discardable_;
  }
  bool WasDiscarded() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return was_discarded_;
  }

  void SetIsConnectedToUSBDeviceForTesting(bool value) override {
    set_is_connected_to_usb_device(value);
  }
  void SetIsConnectedToBluetoothDeviceForTesting(bool value) override {
    set_is_connected_to_bluetooth_device(value);
  }
  void SetIsCapturingVideoForTesting(bool value) override {
    set_is_capturing_video(value);
  }
  void SetIsCapturingAudioForTesting(bool value) override {
    set_is_capturing_audio(value);
  }
  void SetIsBeingMirroredForTesting(bool value) override {
    set_is_being_mirrored(value);
  }
  void SetIsCapturingWindowForTesting(bool value) override {
    set_is_capturing_window(value);
  }
  void SetIsCapturingDisplayForTesting(bool value) override {
    set_is_capturing_display(value);
  }
  void SetIsAutoDiscardableForTesting(bool value) override {
    set_is_auto_discardable(value);
  }
  void SetWasDiscardedForTesting(bool value) override {
    set_was_discarded(value);
  }

  void set_is_connected_to_usb_device(bool is_connected_to_usb_device) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_connected_to_usb_device_ == is_connected_to_usb_device)
      return;
    is_connected_to_usb_device_ = is_connected_to_usb_device;
    for (auto& obs : observers_)
      obs.OnIsConnectedToUSBDeviceChanged(page_node_);
  }
  void set_is_connected_to_bluetooth_device(
      bool is_connected_to_bluetooth_device) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_connected_to_bluetooth_device_ == is_connected_to_bluetooth_device)
      return;
    is_connected_to_bluetooth_device_ = is_connected_to_bluetooth_device;
    for (auto& obs : observers_)
      obs.OnIsConnectedToBluetoothDeviceChanged(page_node_);
  }
  void set_is_capturing_video(bool is_capturing_video) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_video_ == is_capturing_video)
      return;
    is_capturing_video_ = is_capturing_video;
    for (auto& obs : observers_)
      obs.OnIsCapturingVideoChanged(page_node_);
  }
  void set_is_capturing_audio(bool is_capturing_audio) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_audio_ == is_capturing_audio)
      return;
    is_capturing_audio_ = is_capturing_audio;
    for (auto& obs : observers_)
      obs.OnIsCapturingAudioChanged(page_node_);
  }
  void set_is_being_mirrored(bool is_being_mirrored) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_being_mirrored_ == is_being_mirrored)
      return;
    is_being_mirrored_ = is_being_mirrored;
    for (auto& obs : observers_)
      obs.OnIsBeingMirroredChanged(page_node_);
  }
  void set_is_capturing_window(bool is_capturing_window) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_window_ == is_capturing_window)
      return;
    is_capturing_window_ = is_capturing_window;
    for (auto& obs : observers_)
      obs.OnIsCapturingWindowChanged(page_node_);
  }
  void set_is_capturing_display(bool is_capturing_display) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_display_ == is_capturing_display)
      return;
    is_capturing_display_ = is_capturing_display;
    for (auto& obs : observers_)
      obs.OnIsCapturingDisplayChanged(page_node_);
  }
  void set_is_auto_discardable(bool is_auto_discardable) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_auto_discardable_ == is_auto_discardable)
      return;
    is_auto_discardable_ = is_auto_discardable;
    for (auto& obs : observers_)
      obs.OnIsAutoDiscardableChanged(page_node_);
  }
  void set_was_discarded(bool was_discarded) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (was_discarded_ == was_discarded)
      return;
    was_discarded_ = was_discarded;
    for (auto& obs : observers_)
      obs.OnWasDiscardedChanged(page_node_);
  }

 private:
  // Make the impl our friend so it can access the constructor and any
  // storage providers.
  friend class ::performance_manager::NodeAttachedDataImpl<
      PageLiveStateDataImpl>;

  explicit PageLiveStateDataImpl(const PageNodeImpl* page_node)
      : page_node_(page_node) {}

  bool is_connected_to_usb_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_connected_to_bluetooth_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_capturing_video_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_audio_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_being_mirrored_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_window_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_display_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_auto_discardable_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
  bool was_discarded_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  const raw_ptr<const PageNode> page_node_;
};

const char kDescriberName[] = "PageLiveStateDecorator";

}  // namespace

// static
void PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged(
    content::WebContents* contents,
    bool is_connected_to_usb_device) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_connected_to_usb_device,
      is_connected_to_usb_device);
}

// static
void PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged(
    content::WebContents* contents,
    bool is_connected_to_bluetooth_device) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_connected_to_bluetooth_device,
      is_connected_to_bluetooth_device);
}

// static
void PageLiveStateDecorator::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_video,
      is_capturing_video);
}

// static
void PageLiveStateDecorator::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_audio,
      is_capturing_audio);
}

// static
void PageLiveStateDecorator::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_being_mirrored,
      is_being_mirrored);
}

// static
void PageLiveStateDecorator::OnIsCapturingWindowChanged(
    content::WebContents* contents,
    bool is_capturing_window) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_window,
      is_capturing_window);
}

// static
void PageLiveStateDecorator::OnIsCapturingDisplayChanged(
    content::WebContents* contents,
    bool is_capturing_display) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_display,
      is_capturing_display);
}

// static
void PageLiveStateDecorator::SetIsAutoDiscardable(
    content::WebContents* contents,
    bool is_auto_discardable) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_auto_discardable,
      is_auto_discardable);
}

// static
void PageLiveStateDecorator::SetWasDiscarded(content::WebContents* contents,
                                             bool was_discarded) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_was_discarded, was_discarded);
}

void PageLiveStateDecorator::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageLiveStateDecorator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value PageLiveStateDecorator::DescribePageNodeData(
    const PageNode* node) const {
  auto* data = Data::FromPageNode(node);
  if (!data)
    return base::Value();

  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetBoolKey("IsConnectedToUSBDevice", data->IsConnectedToUSBDevice());
  ret.SetBoolKey("IsConnectedToBluetoothDevice",
                 data->IsConnectedToBluetoothDevice());
  ret.SetBoolKey("IsCapturingVideo", data->IsCapturingVideo());
  ret.SetBoolKey("IsCapturingAudio", data->IsCapturingAudio());
  ret.SetBoolKey("IsBeingMirrored", data->IsBeingMirrored());
  ret.SetBoolKey("IsCapturingWindow", data->IsCapturingWindow());
  ret.SetBoolKey("IsCapturingDisplay", data->IsCapturingDisplay());
  ret.SetBoolKey("IsAutoDiscardable", data->IsAutoDiscardable());
  ret.SetBoolKey("WasDiscarded", data->WasDiscarded());

  return ret;
}

PageLiveStateDecorator::Data::Data() = default;
PageLiveStateDecorator::Data::~Data() = default;

void PageLiveStateDecorator::Data::AddObserver(
    PageLiveStateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void PageLiveStateDecorator::Data::RemoveObserver(
    PageLiveStateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

const PageLiveStateDecorator::Data* PageLiveStateDecorator::Data::FromPageNode(
    const PageNode* page_node) {
  return PageLiveStateDataImpl::Get(PageNodeImpl::FromNode(page_node));
}

PageLiveStateDecorator::Data*
PageLiveStateDecorator::Data::GetOrCreateForPageNode(
    const PageNode* page_node) {
  return PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

PageLiveStateObserver::PageLiveStateObserver() = default;
PageLiveStateObserver::~PageLiveStateObserver() = default;

}  // namespace performance_manager
