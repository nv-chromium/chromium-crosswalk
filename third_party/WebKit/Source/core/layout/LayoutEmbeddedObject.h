/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef LayoutEmbeddedObject_h
#define LayoutEmbeddedObject_h

#include "core/layout/LayoutPart.h"

namespace blink {

class TextRun;

// LayoutObject for embeds and objects, often, but not always, rendered via plugins.
// For example, <embed src="foo.html"> does not invoke a plugin.
class LayoutEmbeddedObject : public LayoutPart {
public:
    LayoutEmbeddedObject(Element*);
    ~LayoutEmbeddedObject() override;

    enum PluginUnavailabilityReason {
        PluginMissing,
        PluginBlockedByContentSecurityPolicy,
    };
    void setPluginUnavailabilityReason(PluginUnavailabilityReason);
    bool showsUnavailablePluginIndicator() const;

    const char* name() const override { return "LayoutEmbeddedObject"; }

    const String& unavailablePluginReplacementText() const { return m_unavailablePluginReplacementText; }

private:
    void paintContents(const PaintInfo&, const LayoutPoint&) final;
    void paintReplaced(const PaintInfo&, const LayoutPoint&) final;
    void paint(const PaintInfo&, const LayoutPoint&) final;

    void layout() final;
    PaintInvalidationReason invalidatePaintIfNeeded(PaintInvalidationState&, const LayoutBoxModelObject&) final;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectEmbeddedObject || LayoutPart::isOfType(type); }
    LayoutBox* embeddedContentBox() const final;

    DeprecatedPaintLayerType layerTypeRequired() const final;

    ScrollResultOneDimensional scroll(ScrollDirectionPhysical, ScrollGranularity, float multiplier) final;

    CompositingReasons additionalCompositingReasons() const override;

    bool m_showsUnavailablePluginIndicator;
    PluginUnavailabilityReason m_pluginUnavailabilityReason;
    String m_unavailablePluginReplacementText;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutEmbeddedObject, isEmbeddedObject());

} // namespace blink

#endif // LayoutEmbeddedObject_h
