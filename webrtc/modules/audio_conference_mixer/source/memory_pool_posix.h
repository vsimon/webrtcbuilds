/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_GENERIC_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_GENERIC_H_

#include <assert.h>

#include "critical_section_wrapper.h"
#include "list_wrapper.h"
#include "typedefs.h"

namespace webrtc {
template<class MemoryType>
class MemoryPoolImpl
{
public:
    // MemoryPool functions.
    int32_t PopMemory(MemoryType*&  memory);
    int32_t PushMemory(MemoryType*& memory);

    MemoryPoolImpl(int32_t initialPoolSize);
    ~MemoryPoolImpl();

    // Atomic functions
    int32_t Terminate();
    bool Initialize();
private:
    // Non-atomic function.
    int32_t CreateMemory(uint32_t amountToCreate);

    CriticalSectionWrapper* _crit;

    bool _terminate;

    ListWrapper _memoryPool;

    uint32_t _initialPoolSize;
    uint32_t _createdMemory;
    uint32_t _outstandingMemory;
};

template<class MemoryType>
MemoryPoolImpl<MemoryType>::MemoryPoolImpl(int32_t initialPoolSize)
    : _crit(CriticalSectionWrapper::CreateCriticalSection()),
      _terminate(false),
      _memoryPool(),
      _initialPoolSize(initialPoolSize),
      _createdMemory(0),
      _outstandingMemory(0)
{
}

template<class MemoryType>
MemoryPoolImpl<MemoryType>::~MemoryPoolImpl()
{
    // Trigger assert if there is outstanding memory.
    assert(_createdMemory == 0);
    assert(_outstandingMemory == 0);
    delete _crit;
}

template<class MemoryType>
int32_t MemoryPoolImpl<MemoryType>::PopMemory(MemoryType*& memory)
{
    CriticalSectionScoped cs(_crit);
    if(_terminate)
    {
        memory = NULL;
        return -1;
    }
    ListItem* item = _memoryPool.First();
    if(item == NULL)
    {
        // _memoryPool empty create new memory.
        CreateMemory(_initialPoolSize);
        item = _memoryPool.First();
        if(item == NULL)
        {
            memory = NULL;
            return -1;
        }
    }
    memory = static_cast<MemoryType*>(item->GetItem());
    _memoryPool.Erase(item);
    _outstandingMemory++;
    return 0;
}

template<class MemoryType>
int32_t MemoryPoolImpl<MemoryType>::PushMemory(MemoryType*& memory)
{
    if(memory == NULL)
    {
        return -1;
    }
    CriticalSectionScoped cs(_crit);
    _outstandingMemory--;
    if(_memoryPool.GetSize() > (_initialPoolSize << 1))
    {
        // Reclaim memory if less than half of the pool is unused.
        _createdMemory--;
        delete memory;
        memory = NULL;
        return 0;
    }
    _memoryPool.PushBack(static_cast<void*>(memory));
    memory = NULL;
    return 0;
}

template<class MemoryType>
bool MemoryPoolImpl<MemoryType>::Initialize()
{
    CriticalSectionScoped cs(_crit);
    return CreateMemory(_initialPoolSize) == 0;
}

template<class MemoryType>
int32_t MemoryPoolImpl<MemoryType>::Terminate()
{
    CriticalSectionScoped cs(_crit);
    assert(_createdMemory == _outstandingMemory + _memoryPool.GetSize());

    _terminate = true;
    // Reclaim all memory.
    while(_createdMemory > 0)
    {
        ListItem* item = _memoryPool.First();
        if(item == NULL)
        {
            // There is memory that hasn't been returned yet.
            return -1;
        }
        MemoryType* memory = static_cast<MemoryType*>(item->GetItem());
        delete memory;
        _memoryPool.Erase(item);
        _createdMemory--;
    }
    return 0;
}

template<class MemoryType>
int32_t MemoryPoolImpl<MemoryType>::CreateMemory(
    uint32_t amountToCreate)
{
    for(uint32_t i = 0; i < amountToCreate; i++)
    {
        MemoryType* memory = new MemoryType();
        if(memory == NULL)
        {
            return -1;
        }
        _memoryPool.PushBack(static_cast<void*>(memory));
        _createdMemory++;
    }
    return 0;
}
} // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_GENERIC_H_
