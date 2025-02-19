// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_

#include "base/base_export.h"
#include "base/macros.h"

namespace base {
namespace trace_event {

class ProcessMemoryDump;

// Contains information about the type of memory dump the MemoryDumpProvider
// should generate on dump request. This is to control the size of dumps
// generated.
struct MemoryDumpArgs {
  enum class LevelOfDetail {
    LOW,
    HIGH,
    LAST = HIGH  // For IPC Macros.
  };

  LevelOfDetail level_of_detail;
};

// The contract interface that memory dump providers must implement.
class BASE_EXPORT MemoryDumpProvider {
 public:
  // Called by the MemoryDumpManager when generating memory dumps.
  // The |args| specify if the embedder should generate light/heavy dumps on
  // dump requests. The embedder should return true if the |pmd| was
  // successfully populated, false if something went wrong and the dump should
  // be considered invalid.
  // (Note, the MemoryDumpManager has a fail-safe logic which will disable the
  // MemoryDumpProvider for the entire trace session if it fails consistently).
  virtual bool OnMemoryDump(const MemoryDumpArgs& args,
                            ProcessMemoryDump* pmd) = 0;

 protected:
  MemoryDumpProvider() {}
  virtual ~MemoryDumpProvider() {}

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpProvider);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
