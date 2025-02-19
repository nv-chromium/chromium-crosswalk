// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationController.h"

#include "core/frame/LocalFrame.h"
#include "modules/presentation/PresentationSession.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

PresentationController::PresentationController(LocalFrame& frame, WebPresentationClient* client)
    : LocalFrameLifecycleObserver(&frame)
    , m_client(client)
{
    if (m_client)
        m_client->setController(this);
}

PresentationController::~PresentationController()
{
    if (m_client)
        m_client->setController(nullptr);
}

// static
PassOwnPtrWillBeRawPtr<PresentationController> PresentationController::create(LocalFrame& frame, WebPresentationClient* client)
{
    return adoptPtrWillBeNoop(new PresentationController(frame, client));
}

// static
const char* PresentationController::supplementName()
{
    return "PresentationController";
}

// static
PresentationController* PresentationController::from(LocalFrame& frame)
{
    return static_cast<PresentationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

// static
void PresentationController::provideTo(LocalFrame& frame, WebPresentationClient* client)
{
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, PresentationController::supplementName(), PresentationController::create(frame, client));
}

WebPresentationClient* PresentationController::client()
{
    return m_client;
}

DEFINE_TRACE(PresentationController)
{
    visitor->trace(m_presentation);
    visitor->trace(m_sessions);
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

void PresentationController::didStartDefaultSession(WebPresentationSessionClient* sessionClient)
{
    if (!m_presentation || !m_presentation->defaultRequest())
        return;
    PresentationSession::take(this, adoptPtr(sessionClient), m_presentation->defaultRequest());
}

void PresentationController::didChangeSessionState(WebPresentationSessionClient* sessionClient, WebPresentationSessionState state)
{
    OwnPtr<WebPresentationSessionClient> client = adoptPtr(sessionClient);

    PresentationSession* session = findSession(client.get());
    if (!session)
        return;
    session->didChangeState(state);
}

void PresentationController::didReceiveSessionTextMessage(WebPresentationSessionClient* sessionClient, const WebString& message)
{
    OwnPtr<WebPresentationSessionClient> client = adoptPtr(sessionClient);

    PresentationSession* session = findSession(client.get());
    if (!session)
        return;
    session->didReceiveTextMessage(message);
}

void PresentationController::didReceiveSessionBinaryMessage(WebPresentationSessionClient* sessionClient, const uint8_t* data, size_t length)
{
    OwnPtr<WebPresentationSessionClient> client = adoptPtr(sessionClient);

    PresentationSession* session = findSession(client.get());
    if (!session)
        return;
    session->didReceiveBinaryMessage(data, length);
}

void PresentationController::setPresentation(Presentation* presentation)
{
    m_presentation = presentation;
}

void PresentationController::setDefaultRequestUrl(const KURL& url)
{
    if (!m_client)
        return;

    if (url.isValid())
        m_client->setDefaultPresentationUrl(url.string());
    else
        m_client->setDefaultPresentationUrl(blink::WebString());
}

void PresentationController::registerSession(PresentationSession* session)
{
    m_sessions.add(session);
}

void PresentationController::willDetachFrameHost()
{
    if (m_client) {
        m_client->setController(nullptr);
        m_client = nullptr;
    }
}

PresentationSession* PresentationController::findSession(WebPresentationSessionClient* sessionClient)
{
    for (const auto& session : m_sessions) {
        if (session->matches(sessionClient))
            return session.get();
    }

    return nullptr;
}

} // namespace blink
