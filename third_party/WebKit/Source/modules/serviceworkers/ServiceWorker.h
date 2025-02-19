/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ServiceWorker_h
#define ServiceWorker_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/workers/AbstractWorker.h"
#include "modules/ModulesExport.h"
#include "public/platform/modules/serviceworker/WebServiceWorker.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProxy.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class ScriptPromiseResolver;

class MODULES_EXPORT ServiceWorker final : public AbstractWorker, public WebServiceWorkerProxy {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ServiceWorker* from(ExecutionContext*, WebServiceWorker*);

    ~ServiceWorker() override;

    // Eager finalization needed to promptly release owned WebServiceWorker.
    EAGERLY_FINALIZE();

    // Override 'operator new' to enforce allocation of eagerly finalized object.
    DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();

    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

    String scriptURL() const;
    String state() const;
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    // WebServiceWorkerProxy overrides.
    void dispatchStateChangeEvent() override;

    // AbstractWorker overrides.
    const AtomicString& interfaceName() const override;

    void internalsTerminate();
private:
    static ServiceWorker* getOrCreate(ExecutionContext*, WebServiceWorker*);
    ServiceWorker(ExecutionContext*, PassOwnPtr<WebServiceWorker>);

    // ActiveDOMObject overrides.
    bool hasPendingActivity() const override;
    void stop() override;

    OwnPtr<WebServiceWorker> m_outerWorker;
    bool m_wasStopped;
};

} // namespace blink

#endif // ServiceWorker_h
