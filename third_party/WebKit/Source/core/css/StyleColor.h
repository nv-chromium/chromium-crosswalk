/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleColor_h
#define StyleColor_h

#include "platform/graphics/Color.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleColor {
    DISALLOW_ALLOCATION();
public:
    StyleColor(Color color) : m_color(color), m_currentColor(false) { }
    static StyleColor currentColor() { return StyleColor(); }

    bool isCurrentColor() const { return m_currentColor; }
    Color color() const { ASSERT(!isCurrentColor()); return m_color; }

    Color resolve(Color currentColor) const { return m_currentColor ? currentColor : m_color; }

private:
    StyleColor() : m_currentColor(true) { }
    Color m_color;
    bool m_currentColor;
};

inline bool operator==(const StyleColor& a, const StyleColor& b)
{
    if (a.isCurrentColor() || b.isCurrentColor())
        return a.isCurrentColor() && b.isCurrentColor();
    return a.color() == b.color();
}

inline bool operator!=(const StyleColor& a, const StyleColor& b)
{
    return !(a == b);
}


} // namespace blink

#endif // StyleColor_h
