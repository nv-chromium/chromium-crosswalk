// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthBoxStyleInterpolation.h"

#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/Rect.h"
#include "core/css/StylePropertySet.h"

#include <gtest/gtest.h>

namespace blink {

class AnimationLengthBoxStyleInterpolationTest : public ::testing::Test {
protected:
    static PassOwnPtrWillBeRawPtr<InterpolableValue> lengthBoxToInterpolableValue(const CSSValue& value)
    {
        return LengthBoxStyleInterpolation::lengthBoxtoInterpolableValue(value, value, false);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToLengthBox(InterpolableValue* value, const CSSValue& start, const CSSValue& end)
    {
        return LengthBoxStyleInterpolation::interpolableValueToLengthBox(value, start, end);
    }

    static PassRefPtrWillBeRawPtr<CSSValue> roundTrip(PassRefPtrWillBeRawPtr<CSSValue> value)
    {
        return interpolableValueToLengthBox(lengthBoxToInterpolableValue(*value).get(), *value, *value);
    }

    static void testPrimitiveValue(RefPtrWillBeRawPtr<CSSValue> value, double left, double right, double top, double bottom, CSSPrimitiveValue::UnitType unitType)
    {
        EXPECT_TRUE(value->isPrimitiveValue());
        Rect* rect = toCSSPrimitiveValue(value.get())->getRectValue();

        EXPECT_EQ(rect->left()->getDoubleValue(), left);
        EXPECT_EQ(rect->right()->getDoubleValue(), right);
        EXPECT_EQ(rect->top()->getDoubleValue(), top);
        EXPECT_EQ(rect->bottom()->getDoubleValue(), bottom);

        EXPECT_EQ(unitType, rect->left()->typeWithCalcResolved());
        EXPECT_EQ(unitType, rect->right()->typeWithCalcResolved());
        EXPECT_EQ(unitType, rect->top()->typeWithCalcResolved());
        EXPECT_EQ(unitType, rect->bottom()->typeWithCalcResolved());
    }
};

TEST_F(AnimationLengthBoxStyleInterpolationTest, ZeroLengthBox)
{
    RefPtrWillBeRawPtr<Rect> rectPx = Rect::create();
    rectPx->setLeft(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setRight(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setTop(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setBottom(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));
    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(rectPx.release()));
    testPrimitiveValue(value, 0, 0, 0, 0, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<Rect> rectEms = Rect::create();
    rectEms->setLeft(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems));
    rectEms->setRight(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems));
    rectEms->setTop(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems));
    rectEms->setBottom(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Ems));

    value = roundTrip(CSSPrimitiveValue::create(rectEms.release()));
    testPrimitiveValue(value, 0, 0, 0, 0, CSSPrimitiveValue::UnitType::Ems);
}

TEST_F(AnimationLengthBoxStyleInterpolationTest, SingleUnitBox)
{
    RefPtrWillBeRawPtr<Rect> rectPx = Rect::create();
    rectPx->setLeft(CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setRight(CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setTop(CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setBottom(CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels));

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(rectPx.release()));
    testPrimitiveValue(value, 10, 10, 10, 10, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<Rect> rectPer = Rect::create();
    rectPer->setLeft(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage));
    rectPer->setRight(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage));
    rectPer->setTop(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage));
    rectPer->setBottom(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage));

    value = roundTrip(CSSPrimitiveValue::create(rectPer.release()));
    testPrimitiveValue(value, 30, 30, 30, 30, CSSPrimitiveValue::UnitType::Percentage);

    RefPtrWillBeRawPtr<Rect> rectEms = Rect::create();
    rectEms->setLeft(CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems));
    rectEms->setRight(CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems));
    rectEms->setTop(CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems));
    rectEms->setBottom(CSSPrimitiveValue::create(-10, CSSPrimitiveValue::UnitType::Ems));

    value = roundTrip(CSSPrimitiveValue::create(rectEms.release()));
    testPrimitiveValue(value, -10, -10, -10, -10, CSSPrimitiveValue::UnitType::Ems);
}

TEST_F(AnimationLengthBoxStyleInterpolationTest, MultipleValues)
{
    RefPtrWillBeRawPtr<Rect> rectPx = Rect::create();
    rectPx->setLeft(CSSPrimitiveValue::create(10, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setRight(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setTop(CSSPrimitiveValue::create(20, CSSPrimitiveValue::UnitType::Pixels));
    rectPx->setBottom(CSSPrimitiveValue::create(40, CSSPrimitiveValue::UnitType::Pixels));

    RefPtrWillBeRawPtr<CSSValue> value = roundTrip(CSSPrimitiveValue::create(rectPx.release()));
    testPrimitiveValue(value, 10, 0, 20, 40, CSSPrimitiveValue::UnitType::Pixels);

    RefPtrWillBeRawPtr<Rect> rectPer = Rect::create();
    rectPer->setLeft(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage));
    rectPer->setRight(CSSPrimitiveValue::create(-30, CSSPrimitiveValue::UnitType::Percentage));
    rectPer->setTop(CSSPrimitiveValue::create(30, CSSPrimitiveValue::UnitType::Percentage));
    rectPer->setBottom(CSSPrimitiveValue::create(-30, CSSPrimitiveValue::UnitType::Percentage));

    value = roundTrip(CSSPrimitiveValue::create(rectPer.release()));
    testPrimitiveValue(value, 30, -30, 30, -30, CSSPrimitiveValue::UnitType::Percentage);
}

}
