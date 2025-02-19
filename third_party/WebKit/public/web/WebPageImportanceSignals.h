// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPageImportanceSignals_h
#define WebPageImportanceSignals_h

#include "public/platform/WebCommon.h"

namespace blink {

// WebPageImportanceSignals indicate the importance of the page state to user.
// This signal is propagated to embedder so that it can prioritize preserving
// state of certain page over the others.
class WebPageImportanceSignals {
public:
    WebPageImportanceSignals() { reset(); }

    bool hadFormInteraction() const { return m_hadFormInteraction; }
    void setHadFormInteraction() { m_hadFormInteraction = true; }

    BLINK_EXPORT void reset();
#if BLINK_IMPLEMENTATION
    void onCommitLoad();
#endif

private:
    bool m_hadFormInteraction : 1;
};

} // namespace blink

#endif // WebPageImportancesignals_h
