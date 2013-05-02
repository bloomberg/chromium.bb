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
#include "core/loader/FrameLoaderTypes.h"
#include "core/page/LayoutMilestones.h"
#include "core/page/PageVisibilityState.h"
#include "core/page/UseCounter.h"
#include "core/platform/PlatformScreen.h"
#include "core/platform/Supplementable.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/graphics/Region.h"
#include "core/rendering/Pagination.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if OS(SOLARIS)
#include <sys/time.h> // For time_t structure.
#endif

namespace WebCore {

class AlternativeTextClient;
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
class PlugInClient;
class PluginData;
class PointerLockController;
class ProgressTracker;
class Range;
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

struct ArenaSize {
    ArenaSize(size_t treeSize, size_t allocated)
        : treeSize(treeSize)
        , allocated(allocated)
    {
    }
    size_t treeSize;
    size_t allocated;
};

class Page : public Supplementable<Page> {
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

        AlternativeTextClient* alternativeTextClient;
        ChromeClient* chromeClient;
        ContextMenuClient* contextMenuClient;
        EditorClient* editorClient;
        DragClient* dragClient;
        InspectorClient* inspectorClient;
        PlugInClient* plugInClient;
        BackForwardClient* backForwardClient;
    };

    explicit Page(PageClients&);
    ~Page();

    ArenaSize renderTreeSize() const;

    void setNeedsRecalcStyleInAllFrames();

    RenderTheme* theme() const { return m_theme.get(); }

    ViewportArguments viewportArguments() const;

    static void refreshPlugins(bool reload);
    PluginData* pluginData() const;

    EditorClient* editorClient() const { return m_editorClient; }
    PlugInClient* plugInClient() const { return m_plugInClient; }

    void setMainFrame(PassRefPtr<Frame>);
    Frame* mainFrame() const { return m_mainFrame.get(); }

    bool openedByDOM() const;
    void setOpenedByDOM();

    // DEPRECATED. Use backForward() instead of the following 5 functions.
    bool goBack();
    bool goForward();
    bool canGoBackOrForward(int distance) const;
    void goBackOrForward(int distance);
    int getHistoryLength();

    void goToItem(HistoryItem*, FrameLoadType);

    enum PageGroupType { PrivatePageGroup, SharedPageGroup };
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

    Chrome* chrome() const { return m_chrome.get(); }
    DragCaretController* dragCaretController() const { return m_dragCaretController.get(); }
    DragController* dragController() const { return m_dragController.get(); }
    FocusController* focusController() const { return m_focusController.get(); }
    ContextMenuController* contextMenuController() const { return m_contextMenuController.get(); }
    InspectorController* inspectorController() const { return m_inspectorController.get(); }
    PointerLockController* pointerLockController() const { return m_pointerLockController.get(); }
    ValidationMessageClient* validationMessageClient() const { return m_validationMessageClient; }
    void setValidationMessageClient(ValidationMessageClient* client) { m_validationMessageClient = client; }

    ScrollingCoordinator* scrollingCoordinator();

    String mainThreadScrollingReasonsAsText();
    PassRefPtr<ClientRectList> nonFastScrollableRects(const Frame*);

    Settings* settings() const { return m_settings.get(); }
    ProgressTracker* progress() const { return m_progress.get(); }
    BackForwardController* backForward() const { return m_backForwardController.get(); }

    UseCounter* useCounter() { return &m_UseCounter; }

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

    // Notification that this Page was moved into or out of a native window.
    void setIsInWindow(bool);
    bool isInWindow() const { return m_isInWindow; }

    void userStyleSheetLocationChanged();
    const String& userStyleSheet() const;

    void dnsPrefetchingStateChanged();

    static void allVisitedStateChanged(PageGroup*);
    static void visitedStateChanged(PageGroup*, LinkHash visitedHash);

    StorageNamespace* sessionStorage(bool optionalCreate = true);
    void setSessionStorage(PassRefPtr<StorageNamespace>);

    void setMemoryCacheClientCallsEnabled(bool);
    bool areMemoryCacheClientCallsEnabled() const { return m_areMemoryCacheClientCallsEnabled; }

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

    AlternativeTextClient* alternativeTextClient() const { return m_alternativeTextClient; }

    PageConsole* console() { return m_console.get(); }

    void reportMemoryUsage(MemoryObjectInfo*) const;

    void captionPreferencesChanged();

    double timerAlignmentInterval() const;

private:
    void initGroup();

#if ASSERT_DISABLED
    void checkSubframeCountConsistency() const { }
#else
    void checkSubframeCountConsistency() const;
#endif

    void setTimerAlignmentInterval(double);

    OwnPtr<Chrome> m_chrome;
    OwnPtr<DragCaretController> m_dragCaretController;

    OwnPtr<DragController> m_dragController;
    OwnPtr<FocusController> m_focusController;
    OwnPtr<ContextMenuController> m_contextMenuController;
    OwnPtr<InspectorController> m_inspectorController;
    OwnPtr<PointerLockController> m_pointerLockController;
    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;

    OwnPtr<Settings> m_settings;
    OwnPtr<ProgressTracker> m_progress;

    OwnPtr<BackForwardController> m_backForwardController;
    RefPtr<Frame> m_mainFrame;

    mutable RefPtr<PluginData> m_pluginData;

    RefPtr<RenderTheme> m_theme;

    EditorClient* m_editorClient;
    PlugInClient* m_plugInClient;
    ValidationMessageClient* m_validationMessageClient;

    UseCounter m_UseCounter;

    int m_subframeCount;
    bool m_openedByDOM;

    bool m_tabKeyCyclesThroughElements;
    bool m_defersLoading;
    unsigned m_defersLoadingCallCount;

    bool m_areMemoryCacheClientCallsEnabled;

    float m_pageScaleFactor;
    float m_deviceScaleFactor;

    Pagination m_pagination;

    mutable String m_userStyleSheet;
    mutable bool m_didLoadUserStyleSheet;
    mutable time_t m_userStyleSheetModificationTime;

    RefPtr<PageGroup> m_group;

    RefPtr<StorageNamespace> m_sessionStorage;

    double m_timerAlignmentInterval;

    bool m_isInWindow;

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
    AlternativeTextClient* m_alternativeTextClient;

    OwnPtr<PageConsole> m_console;
};

} // namespace WebCore
    
#endif // Page_h
