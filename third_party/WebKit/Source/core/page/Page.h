/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef Page_h
#define Page_h

#include "core/dom/ViewportArguments.h"
#include "core/page/LayoutMilestones.h"
#include "core/page/PageVisibilityState.h"
#include "core/page/UseCounter.h"
#include "core/platform/LifecycleContext.h"
#include "core/platform/Supplementable.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/graphics/Region.h"
#include "core/rendering/Pagination.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class AutoscrollController;
class BackForwardClient;
class BackForwardController;
class Chrome;
class ChromeClient;
class ClientRectList;
class ContextMenuClient;
class ContextMenuController;
class Document;
class DragCaretController;
class DragClient;
class DragController;
class EditorClient;
class FocusController;
class Frame;
class FrameSelection;
class HaltablePlugin;
class HistoryItem;
class InspectorClient;
class InspectorController;
class Node;
class PageConsole;
class PageGroup;
class PageLifecycleNotifier;
class PlatformMouseEvent;
class PluginData;
class PointerLockController;
class ProgressTracker;
class Range;
class RenderBox;
class RenderObject;
class RenderTheme;
class VisibleSelection;
class ScrollableArea;
class ScrollingCoordinator;
class Settings;
class StorageNamespace;
class ValidationMessageClient;

typedef uint64_t LinkHash;

float deviceScaleFactor(Frame*);

class Page : public Supplementable<Page>, public LifecycleContext {
    WTF_MAKE_NONCOPYABLE(Page);
    friend class Settings;
public:
    static void scheduleForcedStyleRecalcForAllPages();

    // It is up to the platform to ensure that non-null clients are provided where required.
    struct PageClients {
        WTF_MAKE_NONCOPYABLE(PageClients); WTF_MAKE_FAST_ALLOCATED;
    public:
        PageClients();
        ~PageClients();

        ChromeClient* chromeClient;
        ContextMenuClient* contextMenuClient;
        EditorClient* editorClient;
        DragClient* dragClient;
        InspectorClient* inspectorClient;
        BackForwardClient* backForwardClient;
    };

    explicit Page(PageClients&);
    ~Page();

    void setNeedsRecalcStyleInAllFrames();

    ViewportArguments viewportArguments() const;

    static void refreshPlugins(bool reload);
    PluginData* pluginData() const;

    EditorClient& editorClient() const { return *m_editorClient; }

    void setMainFrame(PassRefPtr<Frame>);
    Frame* mainFrame() const { return m_mainFrame.get(); }

    void documentDetached(Document*);

    bool openedByDOM() const;
    void setOpenedByDOM();

    // DEPRECATED. Use backForward() instead of the following function.
    void goToItem(HistoryItem*);

    // FIXME: InspectorPageGroup is only needed to support single process debugger layout tests, it should be removed when DumpRenderTree is gone.
    enum PageGroupType { InspectorPageGroup, PrivatePageGroup, SharedPageGroup };
    void setGroupType(PageGroupType);
    void clearPageGroup();
    PageGroup& group()
    {
        if (!m_group)
            setGroupType(PrivatePageGroup);
        return *m_group;
    }

    void incrementSubframeCount() { ++m_subframeCount; }
    void decrementSubframeCount() { ASSERT(m_subframeCount); --m_subframeCount; }
    int subframeCount() const { checkSubframeCountConsistency(); return m_subframeCount; }

    Chrome& chrome() const { return *m_chrome; }
    DragCaretController& dragCaretController() const { return *m_dragCaretController; }
    DragController& dragController() const { return *m_dragController; }
    FocusController& focusController() const { return *m_focusController; }
    ContextMenuController& contextMenuController() const { return *m_contextMenuController; }
    InspectorController& inspectorController() const { return *m_inspectorController; }
    PointerLockController& pointerLockController() const { return *m_pointerLockController; }
    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient; }
    void setValidationMessageClient(ValidationMessageClient* client) { m_validationMessageClient = client; }

    bool autoscrollInProgress() const;
    bool autoscrollInProgress(const RenderBox*) const;
    bool panScrollInProgress() const;
    void startAutoscrollForSelection(RenderObject*);
    void stopAutoscrollIfNeeded(RenderObject*);
    void stopAutoscrollTimer();
    void updateAutoscrollRenderer();
    void updateDragAndDrop(Node* targetNode, const IntPoint& eventPosition, double eventTime);
#if OS(WIN)
    void handleMouseReleaseForPanScrolling(Frame*, const PlatformMouseEvent&);
    void startPanScrolling(RenderBox*, const IntPoint&);
#endif

    ScrollingCoordinator* scrollingCoordinator();

    String mainThreadScrollingReasonsAsText();
    PassRefPtr<ClientRectList> nonFastScrollableRects(const Frame*);

    Settings& settings() const { return *m_settings; }
    ProgressTracker& progress() const { return *m_progress; }
    BackForwardController& backForward() const { return *m_backForwardController; }

    UseCounter& useCounter() { return m_useCounter; }

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    void unmarkAllTextMatches();

    void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    void setPageScaleFactor(float scale, const IntPoint& origin);
    float pageScaleFactor() const { return m_pageScaleFactor; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float);

    // Page and FrameView both store a Pagination value. Page::pagination() is set only by API,
    // and FrameView::pagination() is set only by CSS. Page::pagination() will affect all
    // FrameViews in the page cache, but FrameView::pagination() only affects the current
    // FrameView.
    const Pagination& pagination() const { return m_pagination; }
    void setPagination(const Pagination&);

    void userStyleSheetLocationChanged();
    const String& userStyleSheet() const;

    void dnsPrefetchingStateChanged();

    static void allVisitedStateChanged(PageGroup*);
    static void visitedStateChanged(PageGroup*, LinkHash visitedHash);

    StorageNamespace* sessionStorage(bool optionalCreate = true);

    // Don't allow more than a certain number of frames in a page.
    // This seems like a reasonable upper bound, and otherwise mutually
    // recursive frameset pages can quickly bring the program to its knees
    // with exponential growth in the number of frames.
    static const int maxNumberOfFrames = 1000;

    PageVisibilityState visibilityState() const;
    void setVisibilityState(PageVisibilityState, bool);

    bool isCursorVisible() const { return m_isCursorVisible; }
    void setIsCursorVisible(bool isVisible) { m_isCursorVisible = isVisible; }

    void addLayoutMilestones(LayoutMilestones);
    LayoutMilestones layoutMilestones() const { return m_layoutMilestones; }

    bool isCountingRelevantRepaintedObjects() const;
    void startCountingRelevantRepaintedObjects();
    void resetRelevantPaintedObjectCounter();
    void addRelevantRepaintedObject(RenderObject*, const LayoutRect& objectPaintRect);
    void addRelevantUnpaintedObject(RenderObject*, const LayoutRect& objectPaintRect);

#ifndef NDEBUG
    void setIsPainting(bool painting) { m_isPainting = painting; }
    bool isPainting() const { return m_isPainting; }
#endif

    PageConsole& console() { return *m_console; }

    double timerAlignmentInterval() const;

    class MultisamplingChangedObserver {
    public:
        virtual void multisamplingChanged(bool) = 0;
    };

    void addMultisamplingChangedObserver(MultisamplingChangedObserver*);
    void removeMultisamplingChangedObserver(MultisamplingChangedObserver*);
    void multisamplingChanged();

    void didCommitLoad(Frame*);

protected:
    PageLifecycleNotifier* lifecycleNotifier();

private:
    void initGroup();

#if ASSERT_DISABLED
    void checkSubframeCountConsistency() const { }
#else
    void checkSubframeCountConsistency() const;
#endif

    void setTimerAlignmentInterval(double);

    virtual PassOwnPtr<LifecycleNotifier> createLifecycleNotifier() OVERRIDE;

    const OwnPtr<AutoscrollController> m_autoscrollController;
    const OwnPtr<Chrome> m_chrome;
    const OwnPtr<DragCaretController> m_dragCaretController;
    const OwnPtr<DragController> m_dragController;
    const OwnPtr<FocusController> m_focusController;
    const OwnPtr<ContextMenuController> m_contextMenuController;
    const OwnPtr<InspectorController> m_inspectorController;
    const OwnPtr<PointerLockController> m_pointerLockController;
    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

    const OwnPtr<Settings> m_settings;
    const OwnPtr<ProgressTracker> m_progress;

    const OwnPtr<BackForwardController> m_backForwardController;
    RefPtr<Frame> m_mainFrame;

    mutable RefPtr<PluginData> m_pluginData;

    EditorClient* const m_editorClient;
    ValidationMessageClient* m_validationMessageClient;

    UseCounter m_useCounter;

    int m_subframeCount;
    bool m_openedByDOM;

    bool m_tabKeyCyclesThroughElements;
    bool m_defersLoading;

    float m_pageScaleFactor;
    float m_deviceScaleFactor;

    Pagination m_pagination;

    mutable String m_userStyleSheet;
    mutable bool m_didLoadUserStyleSheet;

    RefPtr<PageGroup> m_group;

    OwnPtr<StorageNamespace> m_sessionStorage;

    double m_timerAlignmentInterval;

    PageVisibilityState m_visibilityState;

    bool m_isCursorVisible;

    LayoutMilestones m_layoutMilestones;

    HashSet<RenderObject*> m_relevantUnpaintedRenderObjects;
    Region m_topRelevantPaintedRegion;
    Region m_bottomRelevantPaintedRegion;
    Region m_relevantUnpaintedRegion;
    bool m_isCountingRelevantRepaintedObjects;
#ifndef NDEBUG
    bool m_isPainting;
#endif

    const OwnPtr<PageConsole> m_console;

    HashSet<MultisamplingChangedObserver*> m_multisamplingChangedObservers;
};

} // namespace WebCore

#endif // Page_h
