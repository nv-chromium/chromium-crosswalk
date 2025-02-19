/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef LayoutReplaced_h
#define LayoutReplaced_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"

namespace blink {

class CORE_EXPORT LayoutReplaced : public LayoutBox {
public:
    LayoutReplaced(Element*);
    LayoutReplaced(Element*, const LayoutSize& intrinsicSize);
    ~LayoutReplaced() override;

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const override;
    LayoutUnit computeReplacedLogicalHeight() const override;

    bool hasReplacedLogicalHeight() const;
    LayoutRect replacedContentRect(const LayoutSize* overriddenIntrinsicSize = nullptr) const;

    bool needsPreferredWidthsRecalculation() const override;

    // These values are specified to be 300 and 150 pixels in the CSS 2.1 spec.
    // http://www.w3.org/TR/CSS2/visudet.html#inline-replaced-width
    static const int defaultWidth;
    static const int defaultHeight;
    bool canHaveChildren() const override { return false; }
    bool shouldPaint(const PaintInfo&, const LayoutPoint&) const;
    virtual void paintReplaced(const PaintInfo&, const LayoutPoint&) { }
    LayoutRect localSelectionRect(bool checkWhetherSelected = true) const; // This is in local coordinates, but it's a physical rect (so the top left corner is physical top left).

    void paint(const PaintInfo&, const LayoutPoint&) override;

    bool isSelected() const;

protected:
    void willBeDestroyed() override;

    void layout() override;

    LayoutSize intrinsicSize() const final { return m_intrinsicSize; }
    void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const final;

    virtual LayoutUnit intrinsicContentLogicalHeight() const { return intrinsicLogicalHeight(); }

    virtual LayoutUnit minimumReplacedHeight() const { return LayoutUnit(); }

    void setSelectionState(SelectionState) final;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void setIntrinsicSize(const LayoutSize& intrinsicSize) { m_intrinsicSize = intrinsicSize; }
    virtual void intrinsicSizeChanged();

    virtual LayoutBox* embeddedContentBox() const { return nullptr; }

private:
    void computePreferredLogicalWidths() final;

    LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState* = nullptr) const override;

    PositionWithAffinity positionForPoint(const LayoutPoint&) final;

    bool canBeSelectionLeaf() const override { return true; }

    LayoutRect selectionRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer) const final;
    void computeAspectRatioInformationForLayoutBox(LayoutBox*, FloatSize& constrainedSize, double& intrinsicRatio) const;

    mutable LayoutSize m_intrinsicSize;
};

}

#endif
