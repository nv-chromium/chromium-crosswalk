// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncProvider_h
#define WebSyncProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/background_sync/WebSyncError.h"
#include "public/platform/modules/background_sync/WebSyncPermissionStatus.h"
#include "public/platform/modules/background_sync/WebSyncRegistration.h"

namespace blink {

class WebServiceWorkerRegistration;

using WebSyncRegistrationCallbacks = WebCallbacks<WebSyncRegistration*, WebSyncError*>;
using WebSyncNotifyWhenDoneCallbacks = WebCallbacks<bool*, WebSyncError*>;
using WebSyncUnregistrationCallbacks = WebCallbacks<bool*, WebSyncError*>;
using WebSyncGetRegistrationsCallbacks = WebCallbacks<WebVector<WebSyncRegistration*>*, WebSyncError*>;
using WebSyncGetPermissionStatusCallbacks = WebCallbacks<WebSyncPermissionStatus*, WebSyncError*>;

class WebSyncProvider {
public:
    virtual ~WebSyncProvider() { }

    // Takes ownership of the WebSyncRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void registerBackgroundSync(const WebSyncRegistration*, WebServiceWorkerRegistration*, WebSyncRegistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncUnregistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void unregisterBackgroundSync(blink::WebSyncRegistration::Periodicity, int64_t, const WebString&, WebServiceWorkerRegistration*, WebSyncUnregistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getRegistration(blink::WebSyncRegistration::Periodicity, const WebString&, WebServiceWorkerRegistration*, WebSyncRegistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncGetRegistrationsCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getRegistrations(blink::WebSyncRegistration::Periodicity, WebServiceWorkerRegistration*, WebSyncGetRegistrationsCallbacks*) = 0;

    // Takes ownership of the WebSyncGetPermissionStatusCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getPermissionStatus(blink::WebSyncRegistration::Periodicity, WebServiceWorkerRegistration*, WebSyncGetPermissionStatusCallbacks*) = 0;

    // Takes ownership of the WebSyncNotifyWhenDoneCallbacks.
    virtual void notifyWhenDone(int64_t syncId, WebSyncNotifyWhenDoneCallbacks* callbacks)
    {
        // TODO(jkarlin): After landing both legs of the notifyWhenDone CLs, make this a pure virtual function.
        callbacks->onError(new WebSyncError(WebSyncError::ErrorTypeAbort, "Function not implemented."));
        delete callbacks;
    }

    // Takes ownership of the WebServiceWorkerRegistration.
    virtual void trackRegistration(WebSyncRegistration* registration)
    {
        // TODO(jkarlin): After landing both legs of the releaseRegistration CLs, make this a pure virtual function.
        return;
    }

    virtual void releaseRegistration(int64_t syncId)
    {
        // TODO(jkarlin): After landing both legs of the releaseRegistration CLs, make this a pure virtual function.
        return;
    }
};

} // namespace blink

#endif // WebSyncProvider_h
