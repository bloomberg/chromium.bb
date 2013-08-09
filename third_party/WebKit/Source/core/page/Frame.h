/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef Frame_h
#define Frame_h

#include "core/css/StyleColor.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/AdjustViewSizeOrNot.h"
#include "core/page/FrameTree.h"
#include "core/platform/ScrollTypes.h"
#include "core/platform/graphics/IntSize.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace WebCore {

    class AnimationController;
    class Color;
    class DOMWindow;
    class Document;
    class DragImage;
    class Editor;
    class Element;
    class EventHandler;
    class FloatSize;
    class FrameDestructionObserver;
    class FrameSelection;
    class FrameView;
    class HTMLFrameOwnerElement;
    class HTMLTableCellElement;
    class InputMethodController;
    class IntPoint;
    class Node;
    class Range;
    class RenderPart;
    class RenderView;
    class TreeScope;
    class ScriptController;
    class Settings;
    class TreeScope;
    class VisiblePosition;
    class Widget;

    class Frame : public RefCounted<Frame> {
    public:
        static PassRefPtr<Frame> create(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);

        void init();
        void setView(PassRefPtr<FrameView>);
        void createView(const IntSize&, const StyleColor&, bool,
            const IntSize& fixedLayoutSize = IntSize(), bool useFixedLayout = false, ScrollbarMode = ScrollbarAuto, bool horizontalLock = false,
            ScrollbarMode = ScrollbarAuto, bool verticalLock = false);

        ~Frame();

        void addDestructionObserver(FrameDestructionObserver*);
        void removeDestructionObserver(FrameDestructionObserver*);

        void willDetachPage();
        void detachFromPage();
        void disconnectOwnerElement();

        Page* page() const;
        HTMLFrameOwnerElement* ownerElement() const;

        void setDOMWindow(PassRefPtr<DOMWindow>);
        DOMWindow* domWindow() const;
        Document* document() const;
        FrameView* view() const;

        Editor* editor() const;
        EventHandler* eventHandler() const;
        FrameLoader* loader() const;
        NavigationScheduler* navigationScheduler() const;
        FrameSelection* selection() const;
        FrameTree* tree() const;
        AnimationController* animation() const;
        InputMethodController& inputMethodController() const;
        ScriptController* script();

        RenderView* contentRenderer() const; // Root of the render tree for the document contained in this frame.
        RenderPart* ownerRenderer() const; // Renderer for the element that contains this frame.

        void dispatchVisibilityStateChangeEvent();

    // ======== All public functions below this point are candidates to move out of Frame into another class. ========

        bool inScope(TreeScope*) const;

        // See GraphicsLayerClient.h for accepted flags.
        String layerTreeAsText(unsigned flags = 0) const;
        String trackedRepaintRectsAsText() const;

        static Frame* frameForWidget(const Widget*);

        Settings* settings() const; // can be NULL

        void setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio, AdjustViewSizeOrNot);
        bool shouldUsePrintingLayout() const;
        FloatSize resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize);

        bool inViewSourceMode() const;
        void setInViewSourceMode(bool = true);

        void setPageZoomFactor(float factor);
        float pageZoomFactor() const { return m_pageZoomFactor; }
        void setTextZoomFactor(float factor);
        float textZoomFactor() const { return m_textZoomFactor; }
        void setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor);

        void deviceOrPageScaleFactorChanged();

#if ENABLE(ORIENTATION_EVENTS)
        // Orientation is the interface orientation in degrees. Some examples are:
        //  0 is straight up; -90 is when the device is rotated 90 clockwise;
        //  90 is when rotated counter clockwise.
        void sendOrientationChangeEvent(int orientation);
        int orientation() const { return m_orientation; }
#endif

        void clearTimers();
        static void clearTimers(FrameView*, Document*);

        String documentTypeString() const;

        String displayStringModifiedByEncoding(const String&) const;

        PassOwnPtr<DragImage> nodeImage(Node*);
        PassOwnPtr<DragImage> dragImageForSelection();

        String selectedText() const;
        String selectedTextForClipboard() const;

        VisiblePosition visiblePositionForPoint(const IntPoint& framePoint);
        Document* documentAtPoint(const IntPoint& windowPoint);
        PassRefPtr<Range> rangeForPoint(const IntPoint& framePoint);

        // Should only be called on the main frame of a page.
        void notifyChromeClientWheelEventHandlerCountChanged() const;

        bool isURLAllowed(const KURL&) const;

    // ========

    private:
        Frame(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);

        HashSet<FrameDestructionObserver*> m_destructionObservers;

        Page* m_page;
        mutable FrameTree m_treeNode;
        mutable FrameLoader m_loader;
        mutable NavigationScheduler m_navigationScheduler;

        HTMLFrameOwnerElement* m_ownerElement;
        RefPtr<FrameView> m_view;
        RefPtr<DOMWindow> m_domWindow;

        OwnPtr<ScriptController> m_script;
        OwnPtr<Editor> m_editor;
        OwnPtr<FrameSelection> m_selection;
        OwnPtr<EventHandler> m_eventHandler;
        OwnPtr<AnimationController> m_animationController;
        OwnPtr<InputMethodController> m_inputMethodController;

        float m_pageZoomFactor;
        float m_textZoomFactor;

#if ENABLE(ORIENTATION_EVENTS)
        int m_orientation;
#endif

        bool m_inViewSourceMode;
    };

    inline void Frame::init()
    {
        m_loader.init();
    }

    inline FrameLoader* Frame::loader() const
    {
        return &m_loader;
    }

    inline NavigationScheduler* Frame::navigationScheduler() const
    {
        return &m_navigationScheduler;
    }

    inline FrameView* Frame::view() const
    {
        return m_view.get();
    }

    inline ScriptController* Frame::script()
    {
        return m_script.get();
    }

    inline DOMWindow* Frame::domWindow() const
    {
        return m_domWindow.get();
    }

    inline FrameSelection* Frame::selection() const
    {
        return m_selection.get();
    }

    inline Editor* Frame::editor() const
    {
        return m_editor.get();
    }

    inline AnimationController* Frame::animation() const
    {
        return m_animationController.get();
    }

    inline InputMethodController& Frame::inputMethodController() const
    {
        return *m_inputMethodController;
    }

    inline HTMLFrameOwnerElement* Frame::ownerElement() const
    {
        return m_ownerElement;
    }

    inline bool Frame::inViewSourceMode() const
    {
        return m_inViewSourceMode;
    }

    inline void Frame::setInViewSourceMode(bool mode)
    {
        m_inViewSourceMode = mode;
    }

    inline FrameTree* Frame::tree() const
    {
        return &m_treeNode;
    }

    inline Page* Frame::page() const
    {
        return m_page;
    }

    inline EventHandler* Frame::eventHandler() const
    {
        return m_eventHandler.get();
    }

} // namespace WebCore

#endif // Frame_h
