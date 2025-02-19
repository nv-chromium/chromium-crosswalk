// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/ContiguousContainer.h"

#include "wtf/ContainerAnnotations.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PartitionAlloc.h"
#include "wtf/Partitions.h"
#include "wtf/PassOwnPtr.h"
#include <algorithm>

namespace blink {

// Default number of max-sized elements to allocate space for, if there is no
// initial buffer.
static const unsigned kDefaultInitialBufferSize = 32;

class ContiguousContainerBase::Buffer {
    WTF_MAKE_NONCOPYABLE(Buffer);
    WTF_MAKE_FAST_ALLOCATED(Buffer);
public:
    explicit Buffer(size_t bufferSize)
    {
        m_capacity = WTF::Partitions::bufferActualSize(bufferSize);
        m_begin = m_end = static_cast<char*>(
            WTF::Partitions::bufferMalloc(m_capacity));
        ANNOTATE_NEW_BUFFER(m_begin, m_capacity, 0);
    }

    ~Buffer()
    {
        ANNOTATE_DELETE_BUFFER(m_begin, m_capacity, usedCapacity());
        WTF::Partitions::bufferFree(m_begin);
    }

    size_t capacity() const { return m_capacity; }
    size_t usedCapacity() const { return m_end - m_begin; }
    size_t unusedCapacity() const { return capacity() - usedCapacity(); }
    bool isEmpty() const { return usedCapacity() == 0; }

    void* allocate(size_t objectSize)
    {
        ASSERT(unusedCapacity() >= objectSize);
        ANNOTATE_CHANGE_SIZE(
            m_begin, m_capacity, usedCapacity(), usedCapacity() + objectSize);
        void* result = m_end;
        m_end += objectSize;
        return result;
    }

    void deallocateLastObject(void* object)
    {
        RELEASE_ASSERT(m_begin <= object && object < m_end);
        ANNOTATE_CHANGE_SIZE(
            m_begin, m_capacity, usedCapacity(), static_cast<char*>(object) - m_begin);
        m_end = static_cast<char*>(object);
    }

private:
    // m_begin <= m_end <= m_begin + m_capacity
    char* m_begin;
    char* m_end;
    size_t m_capacity;
};

ContiguousContainerBase::ContiguousContainerBase(size_t maxObjectSize)
    : m_endIndex(0)
    , m_maxObjectSize(maxObjectSize)
{
}

ContiguousContainerBase::ContiguousContainerBase(
    size_t maxObjectSize, size_t initialSizeBytes)
    : ContiguousContainerBase(maxObjectSize)
{
    allocateNewBufferForNextAllocation(std::max(maxObjectSize, initialSizeBytes));
}

ContiguousContainerBase::~ContiguousContainerBase()
{
}

size_t ContiguousContainerBase::capacityInBytes() const
{
    size_t capacity = 0;
    for (const auto& buffer : m_buffers)
        capacity += buffer->capacity();
    return capacity;
}

size_t ContiguousContainerBase::usedCapacityInBytes() const
{
    size_t usedCapacity = 0;
    for (const auto& buffer : m_buffers)
        usedCapacity += buffer->usedCapacity();
    return usedCapacity;
}

size_t ContiguousContainerBase::memoryUsageInBytes() const
{
    return sizeof(*this) + capacityInBytes()
        + m_elements.capacity() * sizeof(m_elements[0]);
}

void* ContiguousContainerBase::allocate(size_t objectSize)
{
    ASSERT(objectSize <= m_maxObjectSize);

    Buffer* bufferForAlloc = nullptr;
    if (!m_buffers.isEmpty()) {
        Buffer* endBuffer = m_buffers[m_endIndex].get();
        if (endBuffer->unusedCapacity() >= objectSize)
            bufferForAlloc = endBuffer;
        else if (m_endIndex + 1 < m_buffers.size())
            bufferForAlloc = m_buffers[++m_endIndex].get();
    }

    if (!bufferForAlloc) {
        size_t newBufferSize = m_buffers.isEmpty()
            ? kDefaultInitialBufferSize * m_maxObjectSize
            : 2 * m_buffers.last()->capacity();
        bufferForAlloc = allocateNewBufferForNextAllocation(newBufferSize);
    }

    void* element = bufferForAlloc->allocate(objectSize);
    m_elements.append(element);
    return element;
}

void ContiguousContainerBase::removeLast()
{
    void* object = m_elements.last();
    m_elements.removeLast();

    Buffer* endBuffer = m_buffers[m_endIndex].get();
    endBuffer->deallocateLastObject(object);

    if (endBuffer->isEmpty()) {
        if (m_endIndex > 0)
            m_endIndex--;
        if (m_endIndex + 2 < m_buffers.size())
            m_buffers.removeLast();
    }
}

void ContiguousContainerBase::clear()
{
    m_elements.clear();
    m_buffers.clear();
    m_endIndex = 0;
}

void ContiguousContainerBase::swap(ContiguousContainerBase& other)
{
    m_elements.swap(other.m_elements);
    m_buffers.swap(other.m_buffers);
    std::swap(m_endIndex, other.m_endIndex);
    std::swap(m_maxObjectSize, other.m_maxObjectSize);
}

ContiguousContainerBase::Buffer*
ContiguousContainerBase::allocateNewBufferForNextAllocation(size_t bufferSize)
{
    ASSERT(m_buffers.isEmpty() || m_endIndex == m_buffers.size() - 1);
    OwnPtr<Buffer> newBuffer = adoptPtr(new Buffer(bufferSize));
    Buffer* bufferToReturn = newBuffer.get();
    m_buffers.append(newBuffer.release());
    m_endIndex = m_buffers.size() - 1;
    return bufferToReturn;
}

} // namespace blink
