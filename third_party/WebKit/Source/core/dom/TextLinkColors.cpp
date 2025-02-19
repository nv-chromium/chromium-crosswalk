/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/dom/TextLinkColors.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/layout/LayoutTheme.h"
#include "wtf/text/WTFString.h"

namespace blink {

TextLinkColors::TextLinkColors()
    : m_textColor(Color::black)
{
    resetLinkColor();
    resetVisitedLinkColor();
    resetActiveLinkColor();
}

void TextLinkColors::resetLinkColor()
{
    m_linkColor = Color(0, 0, 238);
}

void TextLinkColors::resetVisitedLinkColor()
{
    m_visitedLinkColor = Color(85, 26, 139);
}

void TextLinkColors::resetActiveLinkColor()
{
    m_activeLinkColor = Color(255, 0, 0);
}

static Color colorForCSSValue(CSSValueID cssValueId)
{
    if (const char* valueName = getValueName(cssValueId)) {
        if (const NamedColor* namedColor = findColor(valueName, strlen(valueName)))
            return Color(namedColor->ARGBValue);
    }
    return LayoutTheme::theme().systemColor(cssValueId);
}

Color TextLinkColors::colorFromPrimitiveValue(const CSSPrimitiveValue* value, Color currentColor, bool forVisitedLink) const
{
    if (value->isRGBColor())
        return Color(value->getRGBA32Value());

    CSSValueID valueID = value->getValueID();
    switch (valueID) {
    case 0:
        return Color();
    case CSSValueWebkitText:
        return textColor();
    case CSSValueWebkitLink:
        return forVisitedLink ? visitedLinkColor() : linkColor();
    case CSSValueWebkitActivelink:
        return activeLinkColor();
    case CSSValueWebkitFocusRingColor:
        return LayoutTheme::theme().focusRingColor();
    case CSSValueInvert: // We don't support outline-color: invert
    case CSSValueCurrentcolor:
        return currentColor;
    default:
        return colorForCSSValue(valueID);
    }
}

}
