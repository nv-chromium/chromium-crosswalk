// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_
#define CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

// Static only class for querying which properties / images are themeable and
// the defaults of these properties.
// All methods are thread safe unless indicated otherwise.
class ThemeProperties {
 public:
  // ---------------------------------------------------------------------------
  // The int values of OverwritableByUserThemeProperties, Alignment, and Tiling
  // are used as a key to store the property in the browser theme pack. If you
  // modify any of these enums, increment the version number in
  // browser_theme_pack.h

  enum OverwritableByUserThemeProperty {
    COLOR_FRAME,
    COLOR_FRAME_INACTIVE,
    COLOR_FRAME_INCOGNITO,
    COLOR_FRAME_INCOGNITO_INACTIVE,
    COLOR_TOOLBAR,
    COLOR_TAB_TEXT,
    COLOR_BACKGROUND_TAB_TEXT,
    COLOR_BOOKMARK_TEXT,
    COLOR_NTP_BACKGROUND,
    COLOR_NTP_TEXT,
    COLOR_NTP_LINK,
    COLOR_NTP_LINK_UNDERLINE,
    COLOR_NTP_HEADER,
    COLOR_NTP_SECTION,
    COLOR_NTP_SECTION_TEXT,
    COLOR_NTP_SECTION_LINK,
    COLOR_NTP_SECTION_LINK_UNDERLINE,
    COLOR_BUTTON_BACKGROUND,

    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,
    TINT_BACKGROUND_TAB,

    NTP_BACKGROUND_ALIGNMENT,
    NTP_BACKGROUND_TILING,
    NTP_LOGO_ALTERNATE
  };

  // A bitfield mask for alignments.
  enum Alignment {
    ALIGN_CENTER = 0,
    ALIGN_LEFT   = 1 << 0,
    ALIGN_TOP    = 1 << 1,
    ALIGN_RIGHT  = 1 << 2,
    ALIGN_BOTTOM = 1 << 3,
  };

  // Background tiling choices.
  enum Tiling {
    NO_REPEAT = 0,
    REPEAT_X = 1,
    REPEAT_Y = 2,
    REPEAT = 3
  };

  // --------------------------------------------------------------------------
  // The int value of the properties in NotOverwritableByUserThemeProperties
  // has no special meaning. Modify the enum to your heart's content.
  // The enum takes on values >= 1000 as not to overlap with
  // OverwritableByUserThemeProperties.
  enum NotOverwritableByUserThemeProperty {
    COLOR_CONTROL_BACKGROUND = 1000,
    COLOR_TOOLBAR_SEPARATOR,

    // These colors don't have constant default values. They are derived from
    // the runtime value of other colors.
    COLOR_NTP_SECTION_HEADER_TEXT,
    COLOR_NTP_SECTION_HEADER_TEXT_HOVER,
    COLOR_NTP_SECTION_HEADER_RULE,
    COLOR_NTP_SECTION_HEADER_RULE_LIGHT,
    COLOR_NTP_TEXT_LIGHT,
    // Color for any icon on the tabstrip (e.g. media indicator).
    COLOR_TAB_ICON,
    COLOR_THROBBER_SPINNING,
    COLOR_THROBBER_WAITING,
#if defined(ENABLE_SUPERVISED_USERS)
    COLOR_SUPERVISED_USER_LABEL,
    COLOR_SUPERVISED_USER_LABEL_BACKGROUND,
    COLOR_SUPERVISED_USER_LABEL_BORDER,
#endif

    COLOR_STATUS_BAR_TEXT,

#if defined(OS_MACOSX)
    COLOR_TOOLBAR_BEZEL,
    COLOR_TOOLBAR_STROKE,
    COLOR_TOOLBAR_STROKE_INACTIVE,
    COLOR_TOOLBAR_BUTTON_STROKE,
    COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE,
    GRADIENT_FRAME_INCOGNITO,
    GRADIENT_FRAME_INCOGNITO_INACTIVE,
    GRADIENT_TOOLBAR,
    GRADIENT_TOOLBAR_INACTIVE,
    GRADIENT_TOOLBAR_BUTTON,
    GRADIENT_TOOLBAR_BUTTON_INACTIVE,
    GRADIENT_TOOLBAR_BUTTON_PRESSED,
    GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE,
#endif  // OS_MACOSX

    // TODO(jonross): Upon the completion of Material Design work, evaluate
    // which of these properties can be moved out of ThemeProperties.

    // Layout Properties for the Toolbar
    PROPERTY_ICON_LABEL_VIEW_TRAILING_PADDING,
    PROPERTY_LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING,
    PROPERTY_LOCATION_BAR_BUBBLE_VERTICAL_PADDING,
    PROPERTY_LOCATION_BAR_HEIGHT,
    PROPERTY_LOCATION_BAR_HORIZONTAL_PADDING,
    PROPERTY_LOCATION_BAR_VERTICAL_PADDING,
    PROPERTY_OMNIBOX_DROPDOWN_BORDER_INTERIOR,
    PROPERTY_OMNIBOX_DROPDOWN_MIN_ICON_VERTICAL_PADDING,
    PROPERTY_OMNIBOX_DROPDOWN_MIN_TEXT_VERTICAL_PADDING,
    PROPERTY_TOOLBAR_BUTTON_BORDER_INSET,
    PROPERTY_TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT,
    PROPERTY_TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT_ASH,
    PROPERTY_TOOLBAR_VIEW_ELEMENT_PADDING,
    PROPERTY_TOOLBAR_VIEW_LEFT_EDGE_SPACING,
    PROPERTY_TOOLBAR_VIEW_LOCATION_BAR_RIGHT_PADDING,
    PROPERTY_TOOLBAR_VIEW_RIGHT_EDGE_SPACING,
    PROPERTY_TOOLBAR_VIEW_STANDARD_SPACING,
    PROPERTY_TOOLBAR_VIEW_VERTICAL_PADDING,
  };

  // Used by the browser theme pack to parse alignments from something like
  // "top left" into a bitmask of Alignment.
  static int StringToAlignment(const std::string& alignment);

  // Used by the browser theme pack to parse alignments from something like
  // "no-repeat" into a Tiling value.
  static int StringToTiling(const std::string& tiling);

  // Converts a bitmask of Alignment into a string like "top left". The result
  // is used to generate a CSS value.
  static std::string AlignmentToString(int alignment);

  // Converts a Tiling into a string like "no-repeat". The result is used to
  // generate a CSS value.
  static std::string TilingToString(int tiling);

  // Returns the set of IDR_* resources that should be tinted.
  // This method is not thread safe.
  static const std::set<int>& GetTintableToolbarButtons();

  // Returns the default tint for the given tint |id| TINT_* enum value.
  // Returns an HSL value of {-1, -1, -1} if |id| is invalid.
  static color_utils::HSL GetDefaultTint(int id);

  // Returns the default color for the given color |id| COLOR_* enum value.
  // Returns SK_ColorRED if |id| is invalid.
  static SkColor GetDefaultColor(int id);

  // Returns the default value for the given property |id|. Returns -1 if |id|
  // is invalid.
  static int GetDefaultDisplayProperty(int id);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ThemeProperties);
};

#endif  // CHROME_BROWSER_THEMES_THEME_PROPERTIES_H_
