// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/child/blink_platform_impl.h"

namespace scheduler {
class WebThreadImplForPPAPI;
}

namespace content {

class PpapiBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  PpapiBlinkPlatformImpl();
  virtual ~PpapiBlinkPlatformImpl();

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  // BlinkPlatformImpl methods:
  virtual blink::WebThread* currentThread();
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebMimeRegistry* mimeRegistry();
  virtual blink::WebFileUtilities* fileUtilities();
  virtual blink::WebSandboxSupport* sandboxSupport();
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual void createMessageChannel(blink::WebMessagePortChannel** channel1,
                                    blink::WebMessagePortChannel** channel2);
  virtual void setCookies(const blink::WebURL& url,
                          const blink::WebURL& first_party_for_cookies,
                          const blink::WebString& value);
  virtual blink::WebString cookies(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);
  virtual blink::WebString defaultLocale();
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebURLLoader* createURLLoader();
  virtual void getPluginList(bool refresh, blink::WebPluginListBuilder*);
  virtual blink::WebData loadResource(const char* name);
  virtual blink::WebStorageNamespace* createLocalStorageNamespace();
  virtual void dispatchStorageEvent(const blink::WebString& key,
      const blink::WebString& oldValue, const blink::WebString& newValue,
      const blink::WebString& origin, const blink::WebURL& url,
      bool isLocalStorage);
  virtual int databaseDeleteFile(const blink::WebString& vfs_file_name,
                                 bool sync_dir);

 private:
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  class SandboxSupport;
  scoped_ptr<SandboxSupport> sandbox_support_;
#endif
  scoped_ptr<scheduler::WebThreadImplForPPAPI> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(PpapiBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
