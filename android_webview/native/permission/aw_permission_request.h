// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_H

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace android_webview {

class AwPermissionRequestDelegate;

// This class wraps a permission request, it works with PermissionRequestHandler
// and its' Java peer to represent the request to AwContentsClient.
// The specific permission request should implement the
// AwPermissionRequestDelegate interface, See MediaPermissionRequest.
// This object is owned by the java peer.
class AwPermissionRequest {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.permission
  enum Resource {
    Geolocation = 1 << 0,
    VideoCapture = 1 << 1,
    AudioCapture = 1 << 2,
    ProtectedMediaId = 1 << 3,
    MIDISysex = 1 << 4,
  };

  // Take the ownership of |delegate|. Returns the native pointer in
  // |weak_ptr|, which is owned by the returned java peer.
  static base::android::ScopedJavaLocalRef<jobject> Create(
      scoped_ptr<AwPermissionRequestDelegate> delegate,
      base::WeakPtr<AwPermissionRequest>* weak_ptr);

  // Return the Java peer. Must be null-checked.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Invoked by Java peer when request is processed, |granted| indicates the
  // request was granted or not.
  void OnAccept(JNIEnv* env, jobject jcaller, jboolean granted);
  void Destroy(JNIEnv* env, jobject obj);

  // Return the origin which initiated the request.
  const GURL& GetOrigin();

  // Return the resources origin requested.
  int64 GetResources();

  // Cancel this request. Guarantee that
  // AwPermissionRequestDelegate::NotifyRequestResult will not be called after
  // this call. This also deletes this object, so weak pointers are invalidated
  // and raw pointers become dangling pointers.
  void CancelAndDelete();

 private:
  AwPermissionRequest(scoped_ptr<AwPermissionRequestDelegate> delegate,
                      base::android::ScopedJavaLocalRef<jobject>* java_peer);
  ~AwPermissionRequest();

  void OnAcceptInternal(bool accept);
  void DeleteThis();

  scoped_ptr<AwPermissionRequestDelegate> delegate_;
  JavaObjectWeakGlobalRef java_ref_;

  bool processed_;
  base::WeakPtrFactory<AwPermissionRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwPermissionRequest);
};

bool RegisterAwPermissionRequest(JNIEnv* env);

}  // namespace android_webivew

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_H
