/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DragCaretController_h
#define DragCaretController_h

#include "core/editing/Caret.h"

namespace blink {

class DragCaretController final : public NoBaseWillBeGarbageCollected<DragCaretController>, private CaretBase {
    WTF_MAKE_NONCOPYABLE(DragCaretController);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(DragCaretController);
public:
    static PassOwnPtrWillBeRawPtr<DragCaretController> create();

    LayoutBlock* caretLayoutObject() const;
    void paintDragCaret(LocalFrame*, GraphicsContext*, const LayoutPoint&, const LayoutRect& clipRect) const;

    bool isContentEditable() const { return m_position.rootEditableElement(); }
    bool isContentRichlyEditable() const;

    bool hasCaret() const { return m_position.isNotNull(); }
    const VisiblePosition& caretPosition() { return m_position; }
    void setCaretPosition(const VisiblePosition&);
    void clear() { setCaretPosition(VisiblePosition()); }

    void nodeWillBeRemoved(Node&);

    DECLARE_TRACE();

private:
    DragCaretController();

    VisiblePosition m_position;
};

} // namespace blink

#endif // DragCaretController_h
