// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils_android.h"

#include "chrome/browser/after_startup_task_utils.h"
#include "jni/AfterStartupTaskUtils_jni.h"

namespace android {

class AfterStartupTaskUtilsJNI {
 public:
  static void SetBrowserStartupIsComplete() {
    AfterStartupTaskUtils::SetBrowserStartupIsComplete();
  }
};

}  // android

static void SetStartupComplete(JNIEnv* env, jclass obj) {
  android::AfterStartupTaskUtilsJNI::SetBrowserStartupIsComplete();
}

bool RegisterAfterStartupTaskUtilsJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
