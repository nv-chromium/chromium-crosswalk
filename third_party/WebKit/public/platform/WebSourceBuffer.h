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

#ifndef WebSourceBuffer_h
#define WebSourceBuffer_h

#include "WebTimeRange.h"

namespace blink {

class WebSourceBufferClient;

class WebSourceBuffer {
public:
    enum AppendMode {
        AppendModeSegments,
        AppendModeSequence
    };

    virtual ~WebSourceBuffer() { }

    // This will only be called once and only with a non-null pointer to a
    // client whose ownership is not transferred to this WebSourceBuffer.
    virtual void setClient(WebSourceBufferClient*) = 0;

    virtual bool setMode(AppendMode) = 0;
    virtual WebTimeRanges buffered() = 0;

    // Run coded frame eviction/garbage collection algorithm.
    // |currentPlaybackTime| is HTMLMediaElement::currentTime. The algorithm
    // will try to preserve data around current playback position.
    // |newDataSize| is size of new data about to be appended to SourceBuffer.
    // Could be zero for appendStream if stream size is unknown in advance.
    // Returns false if buffer is still full after eviction.
    virtual bool evictCodedFrames(double currentPlaybackTime, size_t newDataSize) = 0;

    // Appends data and runs the segment parser loop algorithm.
    // The algorithm may update |*timestampOffset| if |timestampOffset| is not null.
    virtual void append(const unsigned char* data, unsigned length, double* timestampOffset) = 0;

    virtual void abort() = 0;
    virtual void remove(double start, double end) = 0;
    virtual bool setTimestampOffset(double) = 0;

    // Set presentation timestamp for the start of append window.
    virtual void setAppendWindowStart(double) = 0;

    // Set presentation timestamp for the end of append window.
    virtual void setAppendWindowEnd(double) = 0;

    // After this method is called, this WebSourceBuffer should never use the
    // client pointer passed to setClient().
    virtual void removedFromMediaSource() = 0;
};

} // namespace blink

#endif
