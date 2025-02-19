/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ReferenceFilter_h
#define ReferenceFilter_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class SourceGraphic;
class FilterEffect;

class PLATFORM_EXPORT ReferenceFilter: public Filter {
public:
    static PassRefPtrWillBeRawPtr<ReferenceFilter> create(const FloatRect& targetBoundingBox, const FloatRect& filterRegion, float scale)
    {
        return adoptRefWillBeNoop(new ReferenceFilter(targetBoundingBox, filterRegion, scale));
    }

    static PassRefPtrWillBeRawPtr<ReferenceFilter> create(float scale)
    {
        return adoptRefWillBeNoop(new ReferenceFilter(FloatRect(), FloatRect(), scale));
    }

    ~ReferenceFilter() override;
    DECLARE_VIRTUAL_TRACE();

    IntRect sourceImageRect() const override { return IntRect(); }

    void setLastEffect(PassRefPtrWillBeRawPtr<FilterEffect>);
    FilterEffect* lastEffect() const { return m_lastEffect.get(); }

    SourceGraphic* sourceGraphic() const { return m_sourceGraphic.get(); }

private:
    ReferenceFilter(const FloatRect& targetBoundingBox, const FloatRect& filterRegion, float scale);

    RefPtrWillBeMember<SourceGraphic> m_sourceGraphic;
    RefPtrWillBeMember<FilterEffect> m_lastEffect;
};

} // namespace blink

#endif // ReferenceFilter_h
