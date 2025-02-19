// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FONT_SERVICE_PUBLIC_CPP_FONT_LOADER_H_
#define COMPONENTS_FONT_SERVICE_PUBLIC_CPP_FONT_LOADER_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/font_service/public/cpp/mapped_font_file.h"
#include "components/font_service/public/interfaces/font_service.mojom.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace mojo {
class ApplicationImpl;
class Shell;
}

namespace font_service {
namespace internal {
class FontServiceThread;
}

// FontConfig implementation for Skia which proxies to the font service to get
// out of the sandbox. This methods of this class (as imposed by blink
// requirements) may be called on any thread. (Because of this restriction,
// also see the FontServiceThread class.)
//
// This is the mojo equivalent to content/common/font_config_ipc_linux.h
class FontLoader : public SkFontConfigInterface,
                   public internal::MappedFontFile::Observer {
 public:
  explicit FontLoader(mojo::Shell* shell);
  explicit FontLoader(mojo::ApplicationImpl* application_impl);
  ~FontLoader() override;

  // SkFontConfigInterface:
  bool matchFamilyName(const char family_name[],
                       SkTypeface::Style requested,
                       FontIdentity* out_font_identifier,
                       SkString* out_family_name,
                       SkTypeface::Style* out_style) override;
  SkStreamAsset* openStream(const FontIdentity& identity) override;

 private:
  // internal::MappedFontFile::Observer:
  void OnMappedFontFileDestroyed(internal::MappedFontFile* f) override;

  // Thread to own the mojo message pipe. Because FontLoader can be called on
  // multiple threads, we create a dedicated thread to send and receive mojo
  // message calls.
  scoped_refptr<internal::FontServiceThread> thread_;

  // Lock preventing multiple threads from opening font file and accessing
  // |mapped_font_files_| map at the same time.
  base::Lock lock_;

  // Maps font identity ID to the memory-mapped file with font data.
  base::hash_map<uint32_t, internal::MappedFontFile*> mapped_font_files_;

  DISALLOW_COPY_AND_ASSIGN(FontLoader);
};

}  // namespace font_service

#endif  // COMPONENTS_FONT_SERVICE_PUBLIC_CPP_FONT_LOADER_H_
