/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorLayerTreeAgent_h
#define InspectorLayerTreeAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/LayerTree.h"
#include "platform/Timer.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class GraphicsContext;
class GraphicsLayer;
class InspectedFrames;
class LayoutRect;
class PictureSnapshot;
class PaintLayer;
class PaintLayerCompositor;

class CORE_EXPORT InspectorLayerTreeAgent final
    : public InspectorBaseAgent<protocol::LayerTree::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorLayerTreeAgent);

 public:
  static InspectorLayerTreeAgent* create(InspectedFrames* inspectedFrames) {
    return new InspectorLayerTreeAgent(inspectedFrames);
  }
  ~InspectorLayerTreeAgent() override;
  DECLARE_VIRTUAL_TRACE();

  void restore() override;

  // Called from InspectorController
  void willAddPageOverlay(const GraphicsLayer*);
  void didRemovePageOverlay(const GraphicsLayer*);

  // Called from InspectorInstrumentation
  void layerTreeDidChange();
  void didPaint(const GraphicsLayer*, GraphicsContext&, const LayoutRect&);

  // Called from the front-end.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response compositingReasons(
      const String& layerId,
      std::unique_ptr<protocol::Array<String>>* compositingReasons) override;
  protocol::Response makeSnapshot(const String& layerId,
                                  String* snapshotId) override;
  protocol::Response loadSnapshot(
      std::unique_ptr<protocol::Array<protocol::LayerTree::PictureTile>> tiles,
      String* snapshotId) override;
  protocol::Response releaseSnapshot(const String& snapshotId) override;
  protocol::Response profileSnapshot(
      const String& snapshotId,
      protocol::Maybe<int> minRepeatCount,
      protocol::Maybe<double> minDuration,
      protocol::Maybe<protocol::DOM::Rect> clipRect,
      std::unique_ptr<protocol::Array<protocol::Array<double>>>* timings)
      override;
  protocol::Response replaySnapshot(const String& snapshotId,
                                    protocol::Maybe<int> fromStep,
                                    protocol::Maybe<int> toStep,
                                    protocol::Maybe<double> scale,
                                    String* dataURL) override;
  protocol::Response snapshotCommandLog(
      const String& snapshotId,
      std::unique_ptr<protocol::Array<protocol::DictionaryValue>>* commandLog)
      override;

  // Called by other agents.
  std::unique_ptr<protocol::Array<protocol::LayerTree::Layer>> buildLayerTree();

 private:
  static unsigned s_lastSnapshotId;

  explicit InspectorLayerTreeAgent(InspectedFrames*);

  GraphicsLayer* rootGraphicsLayer();

  PaintLayerCompositor* paintLayerCompositor();
  protocol::Response layerById(const String& layerId, GraphicsLayer*&);
  protocol::Response snapshotById(const String& snapshotId,
                                  const PictureSnapshot*&);

  typedef HashMap<int, int> LayerIdToNodeIdMap;
  void buildLayerIdToNodeIdMap(PaintLayer*, LayerIdToNodeIdMap&);
  void gatherGraphicsLayers(
      GraphicsLayer*,
      HashMap<int, int>& layerIdToNodeIdMap,
      std::unique_ptr<protocol::Array<protocol::LayerTree::Layer>>&,
      bool hasWheelEventHandlers,
      int scrollingRootLayerId);
  int idForNode(Node*);

  Member<InspectedFrames> m_inspectedFrames;
  Vector<int, 2> m_pageOverlayLayerIds;

  typedef HashMap<String, RefPtr<PictureSnapshot>> SnapshotById;
  SnapshotById m_snapshotById;
  bool m_suppressLayerPaintEvents;
};

}  // namespace blink

#endif  // !defined(InspectorLayerTreeAgent_h)
