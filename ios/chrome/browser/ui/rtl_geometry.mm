// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/rtl_geometry.h"

#import <UIKit/UIKit.h>
#include <limits>

#include "base/ios/ios_util.h"
#include "base/logging.h"

bool UseRTLLayout() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater();
}

base::i18n::TextDirection LayoutDirection() {
  return UseRTLLayout() ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
}

#pragma mark - LayoutRects.

const LayoutRect LayoutRectZero = {0.0, 0.0, 0.0, CGSizeMake(0.0, 0.0)};

LayoutRect LayoutRectMake(CGFloat leading,
                          CGFloat boundingWidth,
                          CGFloat originY,
                          CGFloat width,
                          CGFloat height) {
  LayoutRect layout = {leading, boundingWidth, originY,
                       CGSizeMake(width, height)};
  return layout;
}

CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction) {
  CGRect rect;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    CGFloat trailing =
        layout.boundingWidth - (layout.leading + layout.size.width);
    rect = CGRectMake(trailing, layout.originY, layout.size.width,
                      layout.size.height);
  } else {
    DCHECK_EQ(direction, base::i18n::LEFT_TO_RIGHT);
    rect = CGRectMake(layout.leading, layout.originY, layout.size.width,
                      layout.size.height);
  }
  return rect;
}

CGRect LayoutRectGetRect(LayoutRect layout) {
  return LayoutRectGetRectUsingDirection(layout, LayoutDirection());
}

CGRect LayoutRectGetBoundsRect(LayoutRect layout) {
  return CGRectMake(0, 0, layout.size.width, layout.size.height);
}

CGPoint LayoutRectGetPositionForAnchorUsingDirection(
    LayoutRect layout,
    CGPoint anchor,
    base::i18n::TextDirection direction) {
  CGRect rect = LayoutRectGetRectUsingDirection(layout, direction);
  return CGPointMake(CGRectGetMinX(rect) + (rect.size.width * anchor.x),
                     CGRectGetMinY(rect) + (rect.size.height * anchor.y));
}

CGPoint LayoutRectGetPositionForAnchor(LayoutRect layout, CGPoint anchor) {
  return LayoutRectGetPositionForAnchorUsingDirection(layout, anchor,
                                                      LayoutDirection());
}

LayoutRect LayoutRectForRectInBoundingRectUsingDirection(
    CGRect rect,
    CGRect contextRect,
    base::i18n::TextDirection direction) {
  LayoutRect layout;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    layout.leading = contextRect.size.width - CGRectGetMaxX(rect);
  } else {
    layout.leading = CGRectGetMinX(rect);
  }
  layout.boundingWidth = contextRect.size.width;
  layout.originY = rect.origin.y;
  layout.size = rect.size;
  return layout;
}

LayoutRect LayoutRectForRectInBoundingRect(CGRect rect, CGRect contextRect) {
  return LayoutRectForRectInBoundingRectUsingDirection(rect, contextRect,
                                                       LayoutDirection());
}

LayoutRect LayoutRectGetLeadingLayout(LayoutRect layout) {
  LayoutRect leadingLayout;
  leadingLayout.leading = 0;
  leadingLayout.boundingWidth = layout.boundingWidth;
  leadingLayout.originY = leadingLayout.originY;
  leadingLayout.size = CGSizeMake(layout.leading, layout.size.height);
  return leadingLayout;
}

LayoutRect LayoutRectGetTrailingLayout(LayoutRect layout) {
  LayoutRect leadingLayout;
  CGFloat trailing = LayoutRectGetTrailingEdge(layout);
  leadingLayout.leading = trailing;
  leadingLayout.boundingWidth = layout.boundingWidth;
  leadingLayout.originY = leadingLayout.originY;
  leadingLayout.size =
      CGSizeMake((layout.boundingWidth - trailing), layout.size.height);
  return leadingLayout;
}

CGFloat LayoutRectGetTrailingEdge(LayoutRect layout) {
  return layout.leading + layout.size.width;
}

CGPoint CGPointLayoutOffsetUsingDirection(CGPoint point,
                                          LayoutOffset offset,
                                          base::i18n::TextDirection direction) {
  CGPoint newPoint = point;
  if (direction == base::i18n::RIGHT_TO_LEFT)
    offset = -offset;
  newPoint.x += offset;
  return newPoint;
}

CGPoint CGPointLayoutOffset(CGPoint point, LayoutOffset offset) {
  return CGPointLayoutOffsetUsingDirection(point, offset, LayoutDirection());
}

CGRect CGRectLayoutOffsetUsingDirection(CGRect rect,
                                        LayoutOffset offset,
                                        base::i18n::TextDirection direction) {
  if (direction == base::i18n::RIGHT_TO_LEFT)
    offset = -offset;
  return CGRectOffset(rect, offset, 0);
}

CGRect CGRectLayoutOffset(CGRect rect, LayoutOffset offset) {
  return CGRectLayoutOffsetUsingDirection(rect, offset, LayoutDirection());
}

LayoutOffset CGRectGetLeadingLayoutOffsetInBoundingRect(CGRect rect,
                                                        CGRect boundingRect) {
  CGFloat rectLeadingEdge = CGRectGetLeadingEdge(rect);
  CGFloat boundingRectLeadingEdge = CGRectGetLeadingEdge(boundingRect);
  LayoutOffset offset = 0;
  if (LayoutDirection() == base::i18n::LEFT_TO_RIGHT) {
    // Leading edges have low x-values for LTR, so subtract the bounding rect's
    // from |rect|'s.
    offset = rectLeadingEdge - boundingRectLeadingEdge;
  } else {
    DCHECK_EQ(LayoutDirection(), base::i18n::RIGHT_TO_LEFT);
    offset = boundingRectLeadingEdge - rectLeadingEdge;
  }
  return offset;
}

LayoutOffset CGRectGetTrailingLayoutOffsetInBoundingRect(CGRect rect,
                                                         CGRect boundingRect) {
  CGFloat rectTrailingEdge = CGRectGetTrailingEdge(rect);
  CGFloat boundingRectTrailingEdge = CGRectGetTrailingEdge(boundingRect);
  LayoutOffset offset = 0;
  if (LayoutDirection() == base::i18n::RIGHT_TO_LEFT) {
    // Trailing edges have low x-values for RTL, so subtract the bounding rect's
    // from |rect|'s.
    offset = rectTrailingEdge - boundingRectTrailingEdge;
  } else {
    DCHECK_EQ(LayoutDirection(), base::i18n::LEFT_TO_RIGHT);
    offset = boundingRectTrailingEdge - rectTrailingEdge;
  }
  return offset;
}

LayoutOffset LeadingContentOffsetForScrollView(UIScrollView* scrollView) {
  return UseRTLLayout()
             ? scrollView.contentSize.width - scrollView.contentOffset.x -
                   CGRectGetWidth(scrollView.bounds)
             : scrollView.contentOffset.x;
}

#pragma mark - UIKit utilities

CGFloat CGRectGetLeadingEdgeUsingDirection(
    CGRect rect,
    base::i18n::TextDirection direction) {
  return direction == base::i18n::RIGHT_TO_LEFT ? CGRectGetMaxX(rect)
                                                : CGRectGetMinX(rect);
}

CGFloat CGRectGetTrailingEdgeUsingDirection(
    CGRect rect,
    base::i18n::TextDirection direction) {
  return direction == base::i18n::RIGHT_TO_LEFT ? CGRectGetMinX(rect)
                                                : CGRectGetMaxX(rect);
}

CGFloat CGRectGetLeadingEdge(CGRect rect) {
  return CGRectGetLeadingEdgeUsingDirection(rect, LayoutDirection());
}

CGFloat CGRectGetTrailingEdge(CGRect rect) {
  return CGRectGetTrailingEdgeUsingDirection(rect, LayoutDirection());
}

UIViewAutoresizing UIViewAutoresizingFlexibleLeadingMargin() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater()
             ? UIViewAutoresizingFlexibleRightMargin
             : UIViewAutoresizingFlexibleLeftMargin;
}

UIViewAutoresizing UIViewAutoresizingFlexibleTrailingMargin() {
  return base::i18n::IsRTL() && base::ios::IsRunningOnIOS9OrLater()
             ? UIViewAutoresizingFlexibleLeftMargin
             : UIViewAutoresizingFlexibleRightMargin;
}

UIEdgeInsets UIEdgeInsetsMakeUsingDirection(
    CGFloat top,
    CGFloat leading,
    CGFloat bottom,
    CGFloat trailing,
    base::i18n::TextDirection direction) {
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    using std::swap;
    swap(leading, trailing);
  }
  // At this point, |leading| == left, |trailing| = right.
  return UIEdgeInsetsMake(top, leading, bottom, trailing);
}

UIEdgeInsets UIEdgeInsetsMakeDirected(CGFloat top,
                                      CGFloat leading,
                                      CGFloat bottom,
                                      CGFloat trailing) {
  return UIEdgeInsetsMakeUsingDirection(top, leading, bottom, trailing,
                                        LayoutDirection());
}

CGFloat UIEdgeInsetsGetLeading(UIEdgeInsets insets) {
  return UseRTLLayout() ? insets.right : insets.left;
}

CGFloat UIEdgeInsetsGetTrailing(UIEdgeInsets insets) {
  return UseRTLLayout() ? insets.left : insets.right;
}

#pragma mark - autolayout utilities

NSLayoutFormatOptions LayoutOptionForRTLSupport() {
  if (UseRTLLayout()) {
    return NSLayoutFormatDirectionLeadingToTrailing;
  }
  return NSLayoutFormatDirectionLeftToRight;
}

#pragma mark - deprecated functions

bool IsRTLUILayout() {
  if (base::ios::IsRunningOnIOS9OrLater()) {
#if __IPHONE_9_0
    // Calling this method is better than using the locale on iOS9 since it will
    // work with the right to left pseudolanguage.
    return [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                       UISemanticContentAttributeUnspecified] ==
           UIUserInterfaceLayoutDirectionRightToLeft;
#endif
  }
  // Using NSLocale instead of base::i18n::IsRTL() in order to take into account
  // right to left pseudolanguage correctly (which base::i18n::IsRTL() doesn't).
  return [NSLocale characterDirectionForLanguage:
                       [[NSLocale currentLocale]
                           objectForKey:NSLocaleLanguageCode]] ==
         NSLocaleLanguageDirectionRightToLeft;
}
