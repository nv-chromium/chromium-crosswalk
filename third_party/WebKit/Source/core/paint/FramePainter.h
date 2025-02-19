// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FramePainter_h
#define FramePainter_h

#include "core/paint/PaintPhase.h"
#include "platform/heap/Handle.h"

namespace blink {

class FrameView;
class GraphicsContext;
class IntRect;
class Scrollbar;

class FramePainter {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(FramePainter);
public:
    explicit FramePainter(FrameView& frameView) : m_frameView(&frameView) { }

    void paint(GraphicsContext*, const GlobalPaintFlags, const IntRect&);
    void paintScrollbars(GraphicsContext*, const IntRect&);
    void paintContents(GraphicsContext*, const GlobalPaintFlags, const IntRect& damageRect);
    void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect);

private:
    void paintScrollbar(GraphicsContext*, Scrollbar*, const IntRect&);

    FrameView& frameView();

    RawPtrWillBeMember<FrameView> m_frameView;
    static bool s_inPaintContents;
};

} // namespace blink

#endif // FramePainter_h
