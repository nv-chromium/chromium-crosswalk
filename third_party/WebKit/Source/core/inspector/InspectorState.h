/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorState_h
#define InspectorState_h

#include "core/CoreExport.h"
#include "platform/JSONValues.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InspectorStateClient;

class CORE_EXPORT InspectorStateUpdateListener : public WillBeGarbageCollectedMixin {
public:
    virtual ~InspectorStateUpdateListener() { }
    virtual void inspectorStateUpdated() = 0;
};

class CORE_EXPORT InspectorState final : public NoBaseWillBeGarbageCollectedFinalized<InspectorState> {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(InspectorState);
public:
    InspectorState(InspectorStateUpdateListener*, PassRefPtr<JSONObject>);

    void loadFromCookie(const String& inspectorStateCookie);

    void mute();
    void unmute();

    bool getBoolean(const String& propertyName);
    String getString(const String& propertyName);
    long getLong(const String& propertyName);
    long getLong(const String& propertyName, long defaultValue);
    double getDouble(const String& propertyName);
    double getDouble(const String& propertyName, double defaultValue);
    PassRefPtr<JSONObject> getObject(const String& propertyName);

    void setBoolean(const String& propertyName, bool value) { setValue(propertyName, JSONBasicValue::create(value)); }
    void setString(const String& propertyName, const String& value) { setValue(propertyName, JSONString::create(value)); }
    void setLong(const String& propertyName, long value) { setValue(propertyName, JSONBasicValue::create((double)value)); }
    void setDouble(const String& propertyName, double value) { setValue(propertyName, JSONBasicValue::create(value)); }
    void setObject(const String& propertyName, PassRefPtr<JSONObject> value) { setValue(propertyName, value); }

    void remove(const String&);

    DECLARE_TRACE();

private:
    void updateCookie();
    void setValue(const String& propertyName, PassRefPtr<JSONValue>);

    // Gets called from InspectorCompositeState::loadFromCookie().
    void setFromCookie(PassRefPtr<JSONObject>);

    friend class InspectorCompositeState;

    RawPtrWillBeMember<InspectorStateUpdateListener> m_listener;
    RefPtr<JSONObject> m_properties;
};

class CORE_EXPORT InspectorCompositeState final : public NoBaseWillBeGarbageCollectedFinalized<InspectorCompositeState>, public InspectorStateUpdateListener {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(InspectorCompositeState);
    WTF_MAKE_NONCOPYABLE(InspectorCompositeState);
public:
    InspectorCompositeState(InspectorStateClient* inspectorStateClient)
        : m_client(inspectorStateClient)
        , m_stateObject(JSONObject::create())
        , m_isMuted(false)
    {
    }
    virtual ~InspectorCompositeState() { }
    DECLARE_TRACE();

    void mute();
    void unmute();

    InspectorState* createAgentState(const String&);
    void loadFromCookie(const String&);

private:
    typedef WillBeHeapHashMap<String, OwnPtrWillBeMember<InspectorState> > InspectorStateMap;

    // From InspectorStateUpdateListener.
    void inspectorStateUpdated() override;

    InspectorStateClient* m_client;
    RefPtr<JSONObject> m_stateObject;
    bool m_isMuted;
    InspectorStateMap m_inspectorStateMap;
};

} // namespace blink

#endif // !defined(InspectorState_h)
