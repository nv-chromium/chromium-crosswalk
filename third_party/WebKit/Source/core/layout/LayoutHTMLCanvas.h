/*
 * Copyright (C) 2004, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutHTMLCanvas_h
#define LayoutHTMLCanvas_h

#include "core/layout/LayoutReplaced.h"

namespace blink {

class HTMLCanvasElement;

class LayoutHTMLCanvas final : public LayoutReplaced {
public:
    explicit LayoutHTMLCanvas(HTMLCanvasElement*);

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectCanvas || LayoutReplaced::isOfType(type); }
    DeprecatedPaintLayerType layerTypeRequired() const override;
    PaintInvalidationReason invalidatePaintIfNeeded(PaintInvalidationState&, const LayoutBoxModelObject&) final;

    void canvasSizeChanged();

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    const char* name() const override { return "LayoutHTMLCanvas"; }

private:
    void paintReplaced(const PaintInfo&, const LayoutPoint&) override;
    void intrinsicSizeChanged() override { canvasSizeChanged(); }

    CompositingReasons additionalCompositingReasons() const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutHTMLCanvas, isCanvas());

} // namespace blink

#endif // LayoutHTMLCanvas_h
