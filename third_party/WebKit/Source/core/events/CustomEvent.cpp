/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "core/events/CustomEvent.h"

#include "bindings/core/v8/SerializedScriptValue.h"

namespace blink {

CustomEvent::CustomEvent()
{
}

CustomEvent::CustomEvent(const AtomicString& type, const CustomEventInit& initializer)
    : Event(type, initializer)
{
    if (initializer.hasDetail())
        m_detail = initializer.detail();
}

CustomEvent::~CustomEvent()
{
}

void CustomEvent::initCustomEvent(const AtomicString& type, bool canBubble, bool cancelable, const ScriptValue& detail)
{
    initEvent(type, canBubble, cancelable);
    m_detail = detail;
}

void CustomEvent::initCustomEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<SerializedScriptValue> serializedDetail)
{
    if (dispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_serializedDetail = serializedDetail;
}

const AtomicString& CustomEvent::interfaceName() const
{
    return EventNames::CustomEvent;
}

DEFINE_TRACE(CustomEvent)
{
    Event::trace(visitor);
}

} // namespace blink
