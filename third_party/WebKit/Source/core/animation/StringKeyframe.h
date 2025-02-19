// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringKeyframe_h
#define StringKeyframe_h

#include "core/animation/Keyframe.h"
#include "core/css/StylePropertySet.h"

#include "wtf/HashMap.h"

namespace blink {

class StyleSheetContents;

class StringKeyframe : public Keyframe {
public:
    static PassRefPtrWillBeRawPtr<StringKeyframe> create()
    {
        return adoptRefWillBeNoop(new StringKeyframe);
    }

    void setPropertyValue(CSSPropertyID, const String& value, Element*, StyleSheetContents*);
    void setPropertyValue(CSSPropertyID, PassRefPtrWillBeRawPtr<CSSValue>);
    void setPropertyValue(const QualifiedName&, const String& value, Element*);
    CSSValue* cssPropertyValue(CSSPropertyID property) const
    {
        int index = m_propertySet->findPropertyIndex(property);
        RELEASE_ASSERT(index >= 0);
        return m_propertySet->propertyAt(static_cast<unsigned>(index)).value();
    }
    String svgPropertyValue(const QualifiedName& attributeName) const
    {
        return m_svgPropertyMap.get(&attributeName);
    }

    PropertyHandleSet properties() const override;

    DECLARE_VIRTUAL_TRACE();

    class CSSPropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        CSSPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue*, EffectModel::CompositeOperation);

        CSSValue* value() const { return m_value.get(); }

        virtual void populateAnimatableValue(CSSPropertyID, Element&, const ComputedStyle* baseStyle) const;
        const PassRefPtrWillBeRawPtr<AnimatableValue> getAnimatableValue() const final { return m_animatableValueCache.get(); }
        void setAnimatableValue(PassRefPtrWillBeRawPtr<AnimatableValue> value) { m_animatableValueCache = value; }

        bool isNeutral() const final { return !m_value; }
        PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const final;
        PassRefPtrWillBeRawPtr<Interpolation> maybeCreateInterpolation(PropertyHandle, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const final;

        DECLARE_VIRTUAL_TRACE();

    private:
        CSSPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue*);

        virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(double offset) const;
        bool isCSSPropertySpecificKeyframe() const override { return true; }

        PassRefPtrWillBeRawPtr<Interpolation> createLegacyStyleInterpolation(CSSPropertyID, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const;
        static bool createInterpolationsFromCSSValues(CSSPropertyID, CSSValue* fromCSSValue, CSSValue* toCSSValue, Element*, OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>>& interpolations);

        void populateAnimatableValueCaches(CSSPropertyID, Keyframe::PropertySpecificKeyframe&, Element*, CSSValue& fromCSSValue, CSSValue& toCSSValue) const;

        RefPtrWillBeMember<CSSValue> m_value;
        mutable RefPtrWillBeMember<AnimatableValue> m_animatableValueCache;
    };

    class SVGPropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        SVGPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String&, EffectModel::CompositeOperation);

        const String& value() const { return m_value; }

        PassOwnPtrWillBeRawPtr<PropertySpecificKeyframe> cloneWithOffset(double offset) const final;

        const PassRefPtrWillBeRawPtr<AnimatableValue> getAnimatableValue() const final { return nullptr; }

        PassOwnPtrWillBeRawPtr<PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const final;
        PassRefPtrWillBeRawPtr<Interpolation> maybeCreateInterpolation(PropertyHandle, Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const final;

        DECLARE_VIRTUAL_TRACE();

    private:
        SVGPropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, const String&);

        bool isSVGPropertySpecificKeyframe() const override { return true; }

        String m_value;
    };

private:
    StringKeyframe()
        : m_propertySet(MutableStylePropertySet::create())
    { }

    StringKeyframe(const StringKeyframe& copyFrom);

    PassRefPtrWillBeRawPtr<Keyframe> clone() const override;
    PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(PropertyHandle) const override;

    bool isStringKeyframe() const override { return true; }

    RefPtrWillBeMember<MutableStylePropertySet> m_propertySet;
    HashMap<const QualifiedName*, String> m_svgPropertyMap;
};

using CSSPropertySpecificKeyframe = StringKeyframe::CSSPropertySpecificKeyframe;
using SVGPropertySpecificKeyframe = StringKeyframe::SVGPropertySpecificKeyframe;

DEFINE_TYPE_CASTS(StringKeyframe, Keyframe, value, value->isStringKeyframe(), value.isStringKeyframe());
DEFINE_TYPE_CASTS(CSSPropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isCSSPropertySpecificKeyframe(), value.isCSSPropertySpecificKeyframe());
DEFINE_TYPE_CASTS(SVGPropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isSVGPropertySpecificKeyframe(), value.isSVGPropertySpecificKeyframe());

}

#endif
