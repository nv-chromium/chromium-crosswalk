// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerResponseType_h
#define WebServiceWorkerResponseType_h

namespace blink {

// FIXME: This header is temporarily kept around not to break builds, but
// should be removed once chromium-side change is landed.
// If you make changes in this file please make sure you have the same changes
// in public/platform/modules/serviceworker/WebServiceWorkerResponseType.h
// until then.
enum WebServiceWorkerResponseType {
    WebServiceWorkerResponseTypeBasic,
    WebServiceWorkerResponseTypeCORS,
    WebServiceWorkerResponseTypeDefault,
    WebServiceWorkerResponseTypeError,
    WebServiceWorkerResponseTypeOpaque,
    WebServiceWorkerResponseTypeOpaqueRedirect,
    WebServiceWorkerResponseTypeLast = WebServiceWorkerResponseTypeOpaqueRedirect
};

} // namespace blink

#endif // WebServiceWorkerResponseType_h
