// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_

#include <string>

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

namespace extensions {

class Extension;

class DevModeBubbleController : public ExtensionMessageBubbleController {
 public:
  // Clears the list of profiles the bubble has been shown for. Should only be
  // used during testing.
  static void ClearProfileListForTesting();

  explicit DevModeBubbleController(Browser* browser);
  ~DevModeBubbleController() override;

  // ExtensionMessageBubbleController:
  void Show(ExtensionMessageBubble* bubble) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevModeBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_
