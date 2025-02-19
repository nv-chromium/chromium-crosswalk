/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef IdentifiersFactory_h
#define IdentifiersFactory_h

#include "core/CoreExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DocumentLoader;
class LocalFrame;

class CORE_EXPORT IdentifiersFactory {
public:
    static void setProcessId(long);
    static String createIdentifier();

    static String requestId(unsigned long identifier);

    static String frameId(LocalFrame*);
    static LocalFrame* frameById(LocalFrame* inspectedFrame, const String&);

    static String loaderId(DocumentLoader*);
    static DocumentLoader* loaderById(LocalFrame* inspectedFrame, const String&);

private:
    static String addProcessIdPrefixTo(int id);
    static int removeProcessIdPrefixFrom(const String&, bool* ok);
};

} // namespace blink


#endif // IdentifiersFactory_h
