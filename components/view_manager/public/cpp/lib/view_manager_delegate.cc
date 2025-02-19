// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_manager_delegate.h"

namespace mojo {

void ViewManagerDelegate::OnUnembed() {}

void ViewManagerDelegate::OnEmbedForDescendant(
    View* view,
    mojo::URLRequestPtr request,
    mojo::ViewManagerClientPtr* client) {
}

}  // namespace mojo
