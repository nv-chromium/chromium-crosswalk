/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorProfilerAgent_h
#define InspectorProfilerAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"
#include <v8-profiler.h>

namespace v8 {
class CpuProfile;
}

namespace blink {

class ExecutionContext;
class InjectedScriptManager;
class InspectorFrontend;
class InspectorOverlay;

typedef String ErrorString;

class HeapProfileXDK final : public RefCountedWillBeGarbageCollectedFinalized<HeapProfileXDK> {
public:
    static PassRefPtrWillBeRawPtr<HeapProfileXDK> create(v8::HeapEventXDK* event, v8::Isolate* isolate)
    {
        return adoptRefWillBeNoop(new HeapProfileXDK(event, isolate));
    }

    String getSymbols() const;
    String getFrames() const;
    String getTypes() const;
    String getChunks() const;
    String getRetentions() const;
    int getDuration() const;

private:
    HeapProfileXDK(v8::HeapEventXDK* event, v8::Isolate* isolate)
      : m_event(event),
        m_isolate(isolate)
    {
    }

    v8::HeapEventXDK* m_event;
    v8::Isolate* m_isolate;
};

class CORE_EXPORT InspectorProfilerAgent final : public InspectorBaseAgent<InspectorProfilerAgent, InspectorFrontend::Profiler>, public InspectorBackendDispatcher::ProfilerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(InspectorProfilerAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorProfilerAgent> create(v8::Isolate*, InjectedScriptManager*, InspectorOverlay*);
    ~InspectorProfilerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    class OutputStream {
    public:
      virtual ~OutputStream() { }
      virtual void write(const uint32_t* chunk, const int size) = 0;
      virtual void write(const char* symbols, int symbolsSize,
                         const char* frames, int framesSize,
                         const char* types, int typesSize,
                         const char* chunks, int chunksSize,
                         const char* retentions, int retentionsSize) { };
    };

    void consoleProfile(ExecutionContext*, const String& title);
    void consoleProfileEnd(const String& title);

    void enable(ErrorString*) override;
    void setSamplingInterval(ErrorString*, int) override;
    void start(ErrorString*) override;
    void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>&) override;

    void disable(ErrorString*) override;
    void restore() override;

    void willProcessTask();
    void didProcessTask();
    void willEnterNestedRunLoop();
    void didLeaveNestedRunLoop();

    static void startTrackingHeapObjectsXDK(int stackDepth, bool retentions);
    static PassRefPtr<HeapProfileXDK> stopTrackingHeapObjectsXDK();
    static void requestHeapXDKUpdate(OutputStream*);

private:
    InspectorProfilerAgent(v8::Isolate*, InjectedScriptManager*, InspectorOverlay*);
    bool enabled();
    void doEnable();
    void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>*);
    String nextProfileId();

    void startProfiling(const String& title);
    PassRefPtr<TypeBuilder::Profiler::CPUProfile> stopProfiling(const String& title, bool serialize);

    bool isRecording() const;
    void idleStarted();
    void idleFinished();

    v8::Isolate* m_isolate;
    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    bool m_recordingCPUProfile;
    class ProfileDescriptor;
    Vector<ProfileDescriptor> m_startedProfiles;
    String m_frontendInitiatedProfileId;
    RawPtrWillBeMember<InspectorOverlay> m_overlay;
};

} // namespace blink


#endif // !defined(InspectorProfilerAgent_h)
