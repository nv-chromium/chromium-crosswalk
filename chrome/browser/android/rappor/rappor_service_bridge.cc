// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/rappor/rappor_service_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "components/rappor/rappor_utils.h"
#include "jni/RapporServiceBridge_jni.h"
#include "url/gurl.h"

namespace rappor {

void SampleDomainAndRegistryFromURL(JNIEnv* env,
                                    jclass caller,
                                    jstring j_metric,
                                    jstring j_url) {
  // TODO(knn): UMA metrics hash the string to prevent frequent re-encoding,
  // perhaps we should do that as well.
  std::string metric(base::android::ConvertJavaStringToUTF8(env, j_metric));
  GURL gurl(base::android::ConvertJavaStringToUTF8(env, j_url));
  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          metric, gurl);
}

void SampleString(JNIEnv* env,
                  jclass caller,
                  jstring j_metric,
                  jstring j_value) {
  std::string metric(base::android::ConvertJavaStringToUTF8(env, j_metric));
  std::string value(base::android::ConvertJavaStringToUTF8(env, j_value));
  rappor::SampleString(g_browser_process->rappor_service(),
                       metric, rappor::UMA_RAPPOR_TYPE, value);
}

bool RegisterRapporServiceBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace rappor
