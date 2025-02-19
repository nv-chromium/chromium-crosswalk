// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_WIN_WIN_WINDOW_H_
#define UI_PLATFORM_WINDOW_WIN_WIN_WINDOW_H_

#include "base/compiler_specific.h"
#include "ui/gfx/win/window_impl.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/win/win_window_export.h"

namespace ui {

class WIN_WINDOW_EXPORT WinWindow : public NON_EXPORTED_BASE(PlatformWindow),
                                    public gfx::WindowImpl {
 public:
  WinWindow(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);
  ~WinWindow() override;

 private:
  void Destroy();

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;

  CR_BEGIN_MSG_MAP_EX(WinWindow)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    CR_MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE,
                                WM_NCXBUTTONDBLCLK,
                                OnMouseRange)
    CR_MESSAGE_HANDLER_EX(WM_CAPTURECHANGED, OnCaptureChanged)

    CR_MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_CHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_SYSCHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_IME_CHAR, OnKeyEvent)
    CR_MESSAGE_HANDLER_EX(WM_NCACTIVATE, OnNCActivate)

    CR_MSG_WM_CLOSE(OnClose)
    CR_MSG_WM_CREATE(OnCreate)
    CR_MSG_WM_DESTROY(OnDestroy)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
  CR_END_MSG_MAP()

  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnCaptureChanged(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param);
  void OnClose();
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnPaint(HDC);
  void OnWindowPosChanged(WINDOWPOS* window_pos);

  PlatformWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WinWindow);
};

namespace test {

// Set true to let WindowTreeHostWin use a popup window
// with no frame/title so that the window size and test's
// expectations matches.
WIN_WINDOW_EXPORT void SetUsePopupAsRootWindowForTest(bool use);

}  // namespace test

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_WIN_WIN_WINDOW_H_
