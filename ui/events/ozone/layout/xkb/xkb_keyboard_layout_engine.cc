// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"

#include <xkbcommon/xkbcommon-names.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_code_conversion.h"

namespace ui {

namespace {

typedef base::Callback<void(const std::string&,
                            scoped_ptr<char, base::FreeDeleter>)>
    LoadKeymapCallback;

KeyboardCode AlphanumericKeyboardCode(base::char16 character) {
  // Plain ASCII letters and digits map directly to VKEY values.
  if ((character >= '0') && (character <= '9'))
    return static_cast<KeyboardCode>(VKEY_0 + character - '0');
  if ((character >= 'a') && (character <= 'z'))
    return static_cast<KeyboardCode>(VKEY_A + character - 'a');
  if ((character >= 'A') && (character <= 'Z'))
    return static_cast<KeyboardCode>(VKEY_A + character - 'A');
  return VKEY_UNKNOWN;
}

// These tables map layout-dependent printable characters/codes
// to legacy Windows-based VKEY values.
//
// VKEYs are determined by the character produced from a DomCode without
// any modifiers, plus zero or more of the DomCode itself, the character
// produced with the Shift modifier, and the character produced with the
// AltGr modifier.

// A table of one or more PrintableSubEntry cases applies when the VKEY is
// not determined by the unmodified character value alone. Each such table
// corresponds to one unmodified character value. For an entry to match,
// the dom_code must match, and, if test_X is set, then the character for
// the key plus modifier X must also match.
struct PrintableSubEntry {
  DomCode dom_code;
  bool test_shift : 1;
  bool test_altgr : 1;
  base::char16 shift_character;
  base::char16 altgr_character;
  KeyboardCode key_code;
};

const base::char16 kNone = 0xFFFE;
const base::char16 kAny = 0xFFFF;

// U+0021 exclamation mark
const PrintableSubEntry kU0021[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_8}};

// U+0022 quote
const PrintableSubEntry kU0022[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3}};

// U+0023 number sign
const PrintableSubEntry kU0023[] = {
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, 1, 0, 0x0027, kAny, VKEY_OEM_2},   // apostrophe
    {DomCode::BACKSLASH, 1, 0, 0x007E, kAny, VKEY_OEM_7}};  // ~, NoSymbol

// U+0024 dollar sign
const PrintableSubEntry kU0024[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_8}};

// U+0027 apostrophe
const PrintableSubEntry kU0027[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::QUOTE, 1, 0, 0x0022, kAny, VKEY_OEM_7},        // quote
    {DomCode::BACKQUOTE, 1, 0, 0x0022, kAny, VKEY_OEM_3},    // quote
    {DomCode::BACKQUOTE, 1, 0, 0x00B7, kAny, VKEY_OEM_5},    // middle dot
    {DomCode::BACKSLASH, 1, 0, kNone, kAny, VKEY_OEM_5},     // NoSymbol
    {DomCode::MINUS, 1, 0, 0x003F, kAny, VKEY_OEM_4},        // ?
    {DomCode::EQUAL, 1, 0, 0x002A, kAny, VKEY_OEM_PLUS},     // *
    {DomCode::QUOTE, 1, 0, 0x0040, kAny, VKEY_OEM_3},        // @
    {DomCode::BACKSLASH, 1, 1, 0x002A, 0x00BD, VKEY_OEM_5},  // *, one half
    {DomCode::BACKSLASH, 1, 0, 0x002A, kAny, VKEY_OEM_2},    // *, NoSymbol
    {DomCode::KEY_Z, 1, 1, 0x0022, 0x0158, VKEY_OEM_7},      // quote, R caron
    {DomCode::KEY_Z, 1, 0, 0x0022, kAny, VKEY_Z}};           // quote

// U+0028 left parenthesis
const PrintableSubEntry kU0028[] = {
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+0029 right parenthesis
const PrintableSubEntry kU0029[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+002A *
const PrintableSubEntry kU002A[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+002B plus sign
const PrintableSubEntry kU002B[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::MINUS, 1, 1, 0x003F, 0x005C, VKEY_OEM_MINUS},  // ?, backslash
    {DomCode::MINUS, 1, 0, 0x003F, kAny, VKEY_OEM_PLUS}};    // ?

// U+002C comma
const PrintableSubEntry kU002C[] = {
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3},
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::KEY_W, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::KEY_V, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::KEY_M, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_COMMA}};

// U+002D hyphen-minus
const PrintableSubEntry kU002D[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::KEY_A, 0, 0, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::SLASH, 1, 0, 0x003D, kAny, VKEY_OEM_MINUS},   // =
    {DomCode::EQUAL, 1, 1, 0x005F, 0x0157, VKEY_OEM_4},     // _, r cedilla
    {DomCode::EQUAL, 1, 0, 0x005F, kAny, VKEY_OEM_MINUS},   // _
    {DomCode::SLASH, 1, 1, 0x005F, 0x002F, VKEY_OEM_2},     // _, /
    {DomCode::SLASH, 1, 0, 0x005F, kAny, VKEY_OEM_MINUS}};  // _

// U+002E full stop
const PrintableSubEntry kU002E[] = {
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::KEY_E, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::KEY_R, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::KEY_O, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+002F /
const PrintableSubEntry kU002F[] = {
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::DIGIT3, 1, 0, 0x0033, kAny, VKEY_3},       // 3
    {DomCode::DIGIT3, 1, 0, 0x003F, kAny, VKEY_OEM_2},   // ?
    {DomCode::DIGIT0, 1, 0, 0x0030, kAny, VKEY_0},       // 0
    {DomCode::DIGIT0, 1, 0, 0x003F, kAny, VKEY_OEM_2}};  // ?

// U+003A colon
const PrintableSubEntry kU003A[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+003B semicolon
const PrintableSubEntry kU003B[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::KEY_Z, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};
// U+003D =
const PrintableSubEntry kU003D[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::SLASH, 1, 0, 0x0025, kAny, VKEY_OEM_8},        // %
    {DomCode::SLASH, 1, 0, 0x002B, kAny, VKEY_OEM_PLUS},     // +
    {DomCode::MINUS, 1, 1, 0x0025, 0x002D, VKEY_OEM_MINUS},  // %, -
    {DomCode::MINUS, 1, 0, 0x0025, kAny, VKEY_OEM_PLUS}};    // %, NoSymbol

// U+003F ?
const PrintableSubEntry kU003F[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_PLUS}};

// U+0040 @
const PrintableSubEntry kU0040[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+005B left square bracket
const PrintableSubEntry kU005B[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+005C backslash
const PrintableSubEntry kU005C[] = {
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BACKSLASH, 1, 0, 0x002F, kAny, VKEY_OEM_7},   // /
    {DomCode::BACKSLASH, 1, 0, 0x007C, kAny, VKEY_OEM_5},   // |
    {DomCode::BACKQUOTE, 1, 1, 0x007C, 0x0031, VKEY_OEM_5},   // |, 1
    {DomCode::BACKQUOTE, 1, 1, 0x007C, 0x0145, VKEY_OEM_3}};  // |, N cedilla

// U+005D right square bracket
const PrintableSubEntry kU005D[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+005F _
const PrintableSubEntry kU005F[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_MINUS}};

// U+0060 grave accent
const PrintableSubEntry kU0060[] = {
    {DomCode::BACKQUOTE, 1, 0, kNone, kAny, VKEY_OEM_3},   // NoSymbol
    {DomCode::BACKQUOTE, 1, 0, 0x00AC, kAny, VKEY_OEM_8},   // not
    {DomCode::BACKQUOTE, 1, 0, 0x007E, kAny, VKEY_OEM_3}};  // ~

// U+00A7 section
const PrintableSubEntry kU00A7[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKQUOTE, 1, 0, 0x00B0, kAny, VKEY_OEM_2},   // degree
    {DomCode::BACKQUOTE, 1, 0, 0x00BD, kAny, VKEY_OEM_5}};  // one half

// U+00AB left-pointing double angle quote
const PrintableSubEntry kU00AB[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+00B0 degree
const PrintableSubEntry kU00B0[] = {
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00BA masculine ordinal indicator
const PrintableSubEntry kU00BA[] = {
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+00E0 a grave
const PrintableSubEntry kU00E0[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5},
    {DomCode::QUOTE, 1, 0, 0x00B0, kAny, VKEY_OEM_7},   // degree
    {DomCode::QUOTE, 1, 0, 0x00E4, kAny, VKEY_OEM_5}};  // a diaeresis

// U+00E1 a acute
const PrintableSubEntry kU00E1[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00E2 a circumflex
const PrintableSubEntry kU00E2[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+00E4 a diaeresis
const PrintableSubEntry kU00E4[] = {
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, 1, 0, 0x00E0, kAny, VKEY_OEM_5},   // a grave
    {DomCode::QUOTE, 1, 0, 0x00C4, kAny, VKEY_OEM_7}};  // A dia.

// U+00E6 ae
const PrintableSubEntry kU00E6[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00E7 c cedilla
const PrintableSubEntry kU00E7[] = {
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::SEMICOLON, 1, 1, 0x00C7, 0x00DE, VKEY_OEM_3},  // C ced., Thorn
    {DomCode::SEMICOLON, 1, 0, 0x00C7, kAny, VKEY_OEM_1}};   // C ced., NoSy

// U+00E8 e grave
const PrintableSubEntry kU00E8[] = {
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_3}};

// U+00E9 e acute
const PrintableSubEntry kU00E9[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::SLASH, 0, 0, kAny, kAny, VKEY_OEM_2},
    {DomCode::SEMICOLON, 1, 0, 0x00C9, kAny, VKEY_OEM_1},   // E acute
    {DomCode::SEMICOLON, 1, 0, 0x00F6, kAny, VKEY_OEM_7}};  // o diaeresis

// U+00ED i acute
const PrintableSubEntry kU00ED[] = {
    {DomCode::DIGIT9, 0, 0, kAny, kAny, VKEY_9},
    {DomCode::BACKQUOTE, 0, 0, kAny, kAny, VKEY_0}};

// U+00F0 eth
const PrintableSubEntry kU00F0[] = {
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_6},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_1}};

// U+00F3 o acute
const PrintableSubEntry kU00F3[] = {
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+00F4 o circumflex
const PrintableSubEntry kU00F4[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1}};

// U+00F6 o diaeresis
const PrintableSubEntry kU00F6[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::MINUS, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::SEMICOLON, 1, 0, 0x00E9, kAny, VKEY_OEM_7},    // e acute
    {DomCode::SEMICOLON, 1, 1, 0x00D6, 0x0162, VKEY_OEM_3},  // O dia., T ced.
    {DomCode::SEMICOLON, 1, 0, 0x00D6, kAny, VKEY_OEM_3}};   // O diaresis

// U+00F8 o stroke
const PrintableSubEntry kU00F8[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+00F9 u grave
const PrintableSubEntry kU00F9[] = {
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_3},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+00FA u acute
const PrintableSubEntry kU00FA[] = {
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+00FC u diaeresis
const PrintableSubEntry kU00FC[] = {
    {DomCode::KEY_W, 0, 0, kAny, kAny, VKEY_W},
    {DomCode::BRACKET_LEFT, 1, 0, 0x00E8, kAny, VKEY_OEM_1},    // e grave
    {DomCode::MINUS, 1, 0, 0x00DC, kAny, VKEY_OEM_2},           // U diaresis
    {DomCode::BRACKET_LEFT, 1, 1, 0x00DC, 0x0141, VKEY_OEM_3},  // U dia., L-
    {DomCode::BRACKET_LEFT, 1, 0, 0x00DC, kAny, VKEY_OEM_1}};   // U diaresis

// U+0103 a breve
const PrintableSubEntry kU0103[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4}};

// U+0105 a ogonek
const PrintableSubEntry kU0105[] = {
    {DomCode::DIGIT1, 0, 0, kAny, kAny, VKEY_1},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_Q},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+010D c caron
const PrintableSubEntry kU010D[] = {
    {DomCode::DIGIT2, 0, 0, kAny, kAny, VKEY_2},
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::KEY_P, 0, 0, kAny, kAny, VKEY_X},
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::COMMA, 0, 0, kAny, kAny, VKEY_OEM_COMMA}};

// U+0111 d stroke
const PrintableSubEntry kU0111[] = {
    {DomCode::DIGIT0, 0, 0, kAny, kAny, VKEY_0},
    {DomCode::BRACKET_RIGHT, 0, 0, kAny, kAny, VKEY_OEM_6}};

// U+0117 e dot above
const PrintableSubEntry kU0117[] = {
    {DomCode::DIGIT4, 0, 0, kAny, kAny, VKEY_4},
    {DomCode::QUOTE, 0, 0, kAny, kAny, VKEY_OEM_7}};

// U+0119 e ogonek
const PrintableSubEntry kU0119[] = {
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3},
    {DomCode::SLASH, 1, 1, 0x0118, 0x006E, VKEY_OEM_2},     // E ogonek, n
    {DomCode::SLASH, 1, 0, 0x0118, kAny, VKEY_OEM_MINUS}};  // E ogonek

// U+012F i ogonek
const PrintableSubEntry kU012F[] = {
    {DomCode::DIGIT5, 0, 0, kAny, kAny, VKEY_5},
    {DomCode::BRACKET_LEFT, 1, 0, 0x012E, kAny, VKEY_OEM_4}};  // Iogonek

// U+0142 l stroke
const PrintableSubEntry kU0142[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_2}};

// U+015F s cedilla
const PrintableSubEntry kU015F[] = {
    {DomCode::SEMICOLON, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_PERIOD}};

// U+0161 s caron
const PrintableSubEntry kU0161[] = {
    {DomCode::DIGIT3, 0, 0, kAny, kAny, VKEY_3},
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::KEY_A, 0, 0, kAny, kAny, VKEY_OEM_1},
    {DomCode::KEY_F, 0, 0, kAny, kAny, VKEY_F},
    {DomCode::PERIOD, 0, 0, kAny, kAny, VKEY_OEM_PERIOD}};

// U+016B u macron
const PrintableSubEntry kU016B[] = {
    {DomCode::DIGIT8, 0, 0, kAny, kAny, VKEY_8},
    {DomCode::KEY_Q, 0, 0, kAny, kAny, VKEY_Q},
    {DomCode::KEY_X, 0, 0, kAny, kAny, VKEY_X}};

// U+0173 u ogonek
const PrintableSubEntry kU0173[] = {
    {DomCode::DIGIT7, 0, 0, kAny, kAny, VKEY_7},
    {DomCode::SEMICOLON, 1, 1, 0x0172, 0x0162, VKEY_OEM_1},  // U ogo., T ced.
    {DomCode::SEMICOLON, 1, 0, 0x0172, kAny, VKEY_OEM_3}};   // U ogonek

// U+017C z dot above
const PrintableSubEntry kU017C[] = {
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_OEM_4},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// U+017E z caron
const PrintableSubEntry kU017E[] = {
    {DomCode::DIGIT6, 0, 0, kAny, kAny, VKEY_6},
    {DomCode::EQUAL, 0, 0, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::KEY_W, 0, 0, kAny, kAny, VKEY_W},
    {DomCode::BRACKET_LEFT, 0, 0, kAny, kAny, VKEY_Y},
    {DomCode::BACKSLASH, 0, 0, kAny, kAny, VKEY_OEM_5}};

// Table mapping unshifted characters to PrintableSubEntry tables.
struct PrintableMultiEntry {
  base::char16 plain_character;
  const PrintableSubEntry* subtable;
  size_t subtable_size;
};

// Entries are ordered by character value.
const PrintableMultiEntry kMultiMap[] = {
    {0x0021, kU0021, arraysize(kU0021)},  // exclamation mark
    {0x0022, kU0022, arraysize(kU0022)},  // quotation mark
    {0x0023, kU0023, arraysize(kU0023)},  // number sign
    {0x0024, kU0024, arraysize(kU0024)},  // dollar sign
    {0x0027, kU0027, arraysize(kU0027)},  // apostrophe
    {0x0028, kU0028, arraysize(kU0028)},  // left parenthesis
    {0x0029, kU0029, arraysize(kU0029)},  // right parenthesis
    {0x002A, kU002A, arraysize(kU002A)},  // asterisk
    {0x002B, kU002B, arraysize(kU002B)},  // plus sign
    {0x002C, kU002C, arraysize(kU002C)},  // comma
    {0x002D, kU002D, arraysize(kU002D)},  // hyphen-minus
    {0x002E, kU002E, arraysize(kU002E)},  // full stop
    {0x002F, kU002F, arraysize(kU002F)},  // solidus
    {0x003A, kU003A, arraysize(kU003A)},  // colon
    {0x003B, kU003B, arraysize(kU003B)},  // semicolon
    {0x003D, kU003D, arraysize(kU003D)},  // equals sign
    {0x003F, kU003F, arraysize(kU003F)},  // question mark
    {0x0040, kU0040, arraysize(kU0040)},  // commercial at
    {0x005B, kU005B, arraysize(kU005B)},  // left square bracket
    {0x005C, kU005C, arraysize(kU005C)},  // reverse solidus
    {0x005D, kU005D, arraysize(kU005D)},  // right square bracket
    {0x005F, kU005F, arraysize(kU005F)},  // low line
    {0x0060, kU0060, arraysize(kU0060)},  // grave accent
    {0x00A7, kU00A7, arraysize(kU00A7)},  // section sign
    {0x00AB, kU00AB, arraysize(kU00AB)},  // left double angle quotation mark
    {0x00B0, kU00B0, arraysize(kU00B0)},  // degree sign
    {0x00BA, kU00BA, arraysize(kU00BA)},  // masculine ordinal indicator
    {0x00E0, kU00E0, arraysize(kU00E0)},  // a grave
    {0x00E1, kU00E1, arraysize(kU00E1)},  // a acute
    {0x00E2, kU00E2, arraysize(kU00E2)},  // a circumflex
    {0x00E4, kU00E4, arraysize(kU00E4)},  // a diaeresis
    {0x00E6, kU00E6, arraysize(kU00E6)},  // ae
    {0x00E7, kU00E7, arraysize(kU00E7)},  // c cedilla
    {0x00E8, kU00E8, arraysize(kU00E8)},  // e grave
    {0x00E9, kU00E9, arraysize(kU00E9)},  // e acute
    {0x00ED, kU00ED, arraysize(kU00ED)},  // i acute
    {0x00F0, kU00F0, arraysize(kU00F0)},  // eth
    {0x00F3, kU00F3, arraysize(kU00F3)},  // o acute
    {0x00F4, kU00F4, arraysize(kU00F4)},  // o circumflex
    {0x00F6, kU00F6, arraysize(kU00F6)},  // o diaeresis
    {0x00F8, kU00F8, arraysize(kU00F8)},  // o stroke
    {0x00F9, kU00F9, arraysize(kU00F9)},  // u grave
    {0x00FA, kU00FA, arraysize(kU00FA)},  // u acute
    {0x00FC, kU00FC, arraysize(kU00FC)},  // u diaeresis
    {0x0103, kU0103, arraysize(kU0103)},  // a breve
    {0x0105, kU0105, arraysize(kU0105)},  // a ogonek
    {0x010D, kU010D, arraysize(kU010D)},  // c caron
    {0x0111, kU0111, arraysize(kU0111)},  // d stroke
    {0x0117, kU0117, arraysize(kU0117)},  // e dot above
    {0x0119, kU0119, arraysize(kU0119)},  // e ogonek
    {0x012F, kU012F, arraysize(kU012F)},  // i ogonek
    {0x0142, kU0142, arraysize(kU0142)},  // l stroke
    {0x015F, kU015F, arraysize(kU015F)},  // s cedilla
    {0x0161, kU0161, arraysize(kU0161)},  // s caron
    {0x016B, kU016B, arraysize(kU016B)},  // u macron
    {0x0173, kU0173, arraysize(kU0173)},  // u ogonek
    {0x017C, kU017C, arraysize(kU017C)},  // z dot above
    {0x017E, kU017E, arraysize(kU017E)},  // z caron
};

// Table mapping unshifted characters to VKEY values.
struct PrintableSimpleEntry {
  base::char16 plain_character;
  KeyboardCode key_code;
};

// Entries are ordered by character value.
const PrintableSimpleEntry kSimpleMap[] = {
    {0x0025, VKEY_5},          // percent sign
    {0x0026, VKEY_1},          // ampersand
    {0x003C, VKEY_OEM_5},      // less-than sign
    {0x007B, VKEY_OEM_7},      // left curly bracket
    {0x007C, VKEY_OEM_5},      // vertical line
    {0x007D, VKEY_OEM_2},      // right curly bracket
    {0x007E, VKEY_OEM_5},      // tilde
    {0x00A1, VKEY_OEM_6},      // inverted exclamation mark
    {0x00AD, VKEY_OEM_3},      // soft hyphen
    {0x00B2, VKEY_OEM_7},      // superscript two
    {0x00B5, VKEY_OEM_5},      // micro sign
    {0x00BB, VKEY_9},          // right-pointing double angle quotation mark
    {0x00BD, VKEY_OEM_5},      // vulgar fraction one half
    {0x00BF, VKEY_OEM_6},      // inverted question mark
    {0x00DF, VKEY_OEM_4},      // sharp s
    {0x00E5, VKEY_OEM_6},      // a ring above
    {0x00EA, VKEY_3},          // e circumflex
    {0x00EB, VKEY_OEM_1},      // e diaeresis
    {0x00EC, VKEY_OEM_6},      // i grave
    {0x00EE, VKEY_OEM_6},      // i circumflex
    {0x00F1, VKEY_OEM_3},      // n tilde
    {0x00F2, VKEY_OEM_3},      // o grave
    {0x00F5, VKEY_OEM_4},      // o tilde
    {0x00F7, VKEY_OEM_6},      // division sign
    {0x00FD, VKEY_7},          // y acute
    {0x00FE, VKEY_OEM_MINUS},  // thorn
    {0x0101, VKEY_OEM_8},      // a macron
    {0x0107, VKEY_OEM_7},      // c acute
    {0x010B, VKEY_OEM_3},      // c dot above
    {0x0113, VKEY_W},          // e macron
    {0x011B, VKEY_2},          // e caron
    {0x011F, VKEY_OEM_6},      // g breve
    {0x0121, VKEY_OEM_4},      // g dot above
    {0x0127, VKEY_OEM_6},      // h stroke
    {0x012B, VKEY_OEM_6},      // i macron
    {0x0131, VKEY_OEM_1},      // dotless i
    {0x0137, VKEY_OEM_5},      // k cedilla
    {0x013C, VKEY_OEM_2},      // l cedilla
    {0x013E, VKEY_2},          // l caron
    {0x0146, VKEY_OEM_4},      // n cedilla
    {0x0148, VKEY_OEM_5},      // n caron
    {0x0151, VKEY_OEM_4},      // o double acute
    {0x0159, VKEY_5},          // r caron
    {0x0163, VKEY_OEM_7},      // t cedilla
    {0x0165, VKEY_5},          // t caron
    {0x016F, VKEY_OEM_1},      // u ring above
    {0x0171, VKEY_OEM_5},      // u double acute
    {0x01A1, VKEY_OEM_6},      // o horn
    {0x01B0, VKEY_OEM_4},      // u horn
    {0x01B6, VKEY_OEM_6},      // z stroke
    {0x0259, VKEY_OEM_3},      // schwa
};

#if defined(OS_CHROMEOS)
void LoadKeymap(const std::string& layout_name,
                scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
                const LoadKeymapCallback& reply_callback) {
  std::string layout_id;
  std::string layout_variant;
  XkbKeyboardLayoutEngine::ParseLayoutName(layout_name, &layout_id,
                                           &layout_variant);
  xkb_rule_names names = {.rules = NULL,
                          .model = "pc101",
                          .layout = layout_id.c_str(),
                          .variant = layout_variant.c_str(),
                          .options = ""};
  scoped_ptr<xkb_context, XkbContextDeleter> context;
  context.reset(xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES));
  xkb_context_include_path_append(context.get(), "/usr/share/X11/xkb");
  scoped_ptr<xkb_keymap, XkbKeymapDeleter> keymap;
  keymap.reset(xkb_keymap_new_from_names(context.get(), &names,
                                         XKB_KEYMAP_COMPILE_NO_FLAGS));
  if (keymap) {
    scoped_ptr<char, base::FreeDeleter> keymap_str(
        xkb_keymap_get_as_string(keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    reply_runner->PostTask(FROM_HERE, base::Bind(reply_callback, layout_name,
                                                 base::Passed(&keymap_str)));
  } else {
    LOG(FATAL) << "Keymap file failed to load: " << layout_name;
  }
}
#endif

}  // anonymous namespace

XkbKeyCodeConverter::XkbKeyCodeConverter() {
}

XkbKeyCodeConverter::~XkbKeyCodeConverter() {
}

XkbKeyboardLayoutEngine::XkbKeyboardLayoutEngine(
    const XkbKeyCodeConverter& converter)
    : key_code_converter_(converter), weak_ptr_factory_(this) {
  // TODO: add XKB_CONTEXT_NO_ENVIRONMENT_NAMES
  xkb_context_.reset(xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES));
  xkb_context_include_path_append(xkb_context_.get(),
                                  "/usr/share/X11/xkb");
}

XkbKeyboardLayoutEngine::~XkbKeyboardLayoutEngine() {
  for (const auto& entry : xkb_keymaps_) {
    xkb_keymap_unref(entry.keymap);
  }
}

bool XkbKeyboardLayoutEngine::CanSetCurrentLayout() const {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool XkbKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name) {
#if defined(OS_CHROMEOS)
  current_layout_name_ = layout_name;
  for (const auto& entry : xkb_keymaps_) {
    if (entry.layout_name == layout_name) {
      SetKeymap(entry.keymap);
      return true;
    }
  }
  LoadKeymapCallback reply_callback = base::Bind(
      &XkbKeyboardLayoutEngine::OnKeymapLoaded, weak_ptr_factory_.GetWeakPtr());
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&LoadKeymap, layout_name, base::ThreadTaskRunnerHandle::Get(),
                 reply_callback),
      true);
#else
  xkb_keymap* keymap = xkb_map_new_from_string(
      xkb_context_.get(), layout_name.c_str(), XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap)
    return false;
  SetKeymap(keymap);
#endif  // defined(OS_CHROMEOS)
  return true;
}

void XkbKeyboardLayoutEngine::OnKeymapLoaded(
    const std::string& layout_name,
    scoped_ptr<char, base::FreeDeleter> keymap_str) {
  if (keymap_str) {
    xkb_keymap* keymap = xkb_map_new_from_string(
        xkb_context_.get(), keymap_str.get(), XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    XkbKeymapEntry entry = {layout_name, keymap};
    xkb_keymaps_.push_back(entry);
    if (layout_name == current_layout_name_)
      SetKeymap(keymap);
  } else {
    LOG(FATAL) << "Keymap file failed to load: " << layout_name;
  }
}

bool XkbKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  // NOTIMPLEMENTED();
  return false;
}

bool XkbKeyboardLayoutEngine::UsesAltGr() const {
  // NOTIMPLEMENTED();
  return false;
}

bool XkbKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                     int flags,
                                     DomKey* dom_key,
                                     base::char16* character,
                                     KeyboardCode* key_code,
                                     uint32* platform_keycode) const {
  if (dom_code == DomCode::NONE)
    return false;
  // Convert DOM physical key to XKB representation.
  xkb_keycode_t xkb_keycode = key_code_converter_.DomCodeToXkbKeyCode(dom_code);
  if (xkb_keycode == key_code_converter_.InvalidXkbKeyCode()) {
    LOG(ERROR) << "No XKB keycode for DomCode 0x" << std::hex
               << static_cast<int>(dom_code) << " '"
               << KeycodeConverter::DomCodeToCodeString(dom_code) << "'";
    return false;
  }
  xkb_mod_mask_t xkb_flags = EventFlagsToXkbFlags(flags);
  // Obtain keysym and character.
  xkb_keysym_t xkb_keysym;
  if (!XkbLookup(xkb_keycode, xkb_flags, &xkb_keysym, character))
    return false;
  *platform_keycode = xkb_keysym;
  // Classify the keysym and convert to DOM and VKEY representations.
  *dom_key = NonPrintableXKeySymToDomKey(xkb_keysym);
  if (*dom_key == DomKey::NONE) {
    *dom_key = CharacterToDomKey(*character);
    *key_code = AlphanumericKeyboardCode(*character);
    if (*key_code == VKEY_UNKNOWN) {
      *key_code = DifficultKeyboardCode(dom_code, flags, xkb_keycode, xkb_flags,
                                        xkb_keysym, *dom_key, *character);
      if (*key_code == VKEY_UNKNOWN) {
        *key_code = LocatedToNonLocatedKeyboardCode(
            DomCodeToUsLayoutKeyboardCode(dom_code));
      }
    }
    // If the Control key is down, only allow ASCII control characters to be
    // returned, regardless of the key layout. crbug.com/450849
    if ((flags & EF_CONTROL_DOWN) && (*character >= 0x20))
      *character = 0;
  } else if (*dom_key == DomKey::DEAD) {
    *character = DeadXkbKeySymToCombiningCharacter(xkb_keysym);
    *key_code = LocatedToNonLocatedKeyboardCode(
        DomCodeToUsLayoutKeyboardCode(dom_code));
  } else {
    *key_code = NonPrintableDomKeyToKeyboardCode(*dom_key);
    if (*key_code == VKEY_UNKNOWN) {
      *key_code = LocatedToNonLocatedKeyboardCode(
          DomCodeToUsLayoutKeyboardCode(dom_code));
    }
  }
  return true;
}

void XkbKeyboardLayoutEngine::SetKeymap(xkb_keymap* keymap) {
  xkb_state_.reset(xkb_state_new(keymap));
  // Update flag map.
  static const struct {
    int ui_flag;
    const char* xkb_name;
  } flags[] = {{ui::EF_CONTROL_DOWN, XKB_MOD_NAME_CTRL},
               {ui::EF_SHIFT_DOWN, XKB_MOD_NAME_SHIFT},
               {ui::EF_ALT_DOWN, XKB_MOD_NAME_ALT},
               {ui::EF_CAPS_LOCK_DOWN, XKB_MOD_NAME_CAPS},
               {ui::EF_COMMAND_DOWN, XKB_MOD_NAME_LOGO},
               {ui::EF_MOD3_DOWN, "Mod3"},
               {ui::EF_ALTGR_DOWN, "Mod5"}};
  xkb_flag_map_.clear();
  xkb_flag_map_.resize(arraysize(flags));
  for (size_t i = 0; i < arraysize(flags); ++i) {
    xkb_mod_index_t index = xkb_keymap_mod_get_index(keymap, flags[i].xkb_name);
    if (index == XKB_MOD_INVALID) {
      DVLOG(3) << "XKB keyboard layout does not contain " << flags[i].xkb_name;
    } else {
      xkb_mod_mask_t flag = static_cast<xkb_mod_mask_t>(1) << index;
      XkbFlagMapEntry e = {flags[i].ui_flag, flag};
      xkb_flag_map_.push_back(e);
    }
  }

  // Update num lock mask.
  num_lock_mod_mask_ = 0;
  xkb_mod_index_t num_mod_index =
      xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_NUM);
  if (num_mod_index != XKB_MOD_INVALID)
    num_lock_mod_mask_ = static_cast<xkb_mod_mask_t>(1) << num_mod_index;
}

xkb_mod_mask_t XkbKeyboardLayoutEngine::EventFlagsToXkbFlags(
    int ui_flags) const {
  xkb_mod_mask_t xkb_flags = 0;
  for (const auto& entry : xkb_flag_map_) {
    if (ui_flags & entry.ui_flag)
      xkb_flags |= entry.xkb_flag;
  }
  // NumLock is always on.
  xkb_flags |= num_lock_mod_mask_;
  return xkb_flags;
}

bool XkbKeyboardLayoutEngine::XkbLookup(xkb_keycode_t xkb_keycode,
                                        xkb_mod_mask_t xkb_flags,
                                        xkb_keysym_t* xkb_keysym,
                                        base::char16* character) const {
  if (!xkb_state_) {
    LOG(ERROR) << "No current XKB state";
    return false;
  }
  xkb_state_update_mask(xkb_state_.get(), xkb_flags, 0, 0, 0, 0, 0);
  *xkb_keysym = xkb_state_key_get_one_sym(xkb_state_.get(), xkb_keycode);
  if (*xkb_keysym == XKB_KEY_NoSymbol)
    return false;
  uint32_t c = xkb_state_key_get_utf32(xkb_state_.get(), xkb_keycode);
  DLOG_IF(ERROR, c != (c & 0xFFFF)) << "Non-BMP character:" << c;
  *character = static_cast<base::char16>(c);
  return true;
}

KeyboardCode XkbKeyboardLayoutEngine::DifficultKeyboardCode(
    DomCode dom_code,
    int ui_flags,
    xkb_keycode_t xkb_keycode,
    xkb_mod_mask_t xkb_flags,
    xkb_keysym_t xkb_keysym,
    DomKey dom_key,
    base::char16 character) const {
  // Get the layout interpretation without modifiers, so that
  // e.g. Ctrl+D correctly generates VKEY_D.
  xkb_keysym_t plain_keysym;
  base::char16 plain_character;
  if (!XkbLookup(xkb_keycode, 0, &plain_keysym, &plain_character))
    return VKEY_UNKNOWN;

  // If the plain key is non-printable, that determines the VKEY.
  DomKey plain_key = NonPrintableXKeySymToDomKey(plain_keysym);
  if (plain_key != ui::DomKey::NONE)
    return NonPrintableDomKeyToKeyboardCode(dom_key);

  // Plain ASCII letters and digits map directly to VKEY values.
  KeyboardCode key_code = AlphanumericKeyboardCode(plain_character);
  if (key_code != VKEY_UNKNOWN)
    return key_code;

  // Check the multi-character tables.
  const PrintableMultiEntry* multi_end = kMultiMap + arraysize(kMultiMap);
  const PrintableMultiEntry* multi =
      std::lower_bound(kMultiMap, multi_end, plain_character,
                       [](const PrintableMultiEntry& e, base::char16 c) {
        return e.plain_character < c;
      });
  if ((multi != multi_end) && (multi->plain_character == plain_character)) {
    const base::char16 kNonCharacter = kAny;
    base::char16 shift_character = kNonCharacter;
    base::char16 altgr_character = kNonCharacter;
    for (size_t i = 0; i < multi->subtable_size; ++i) {
      if (multi->subtable[i].dom_code != dom_code)
        continue;
      if (multi->subtable[i].test_shift) {
        if (shift_character == kNonCharacter) {
          shift_character = XkbSubCharacter(xkb_keycode, xkb_flags, character,
                                            ui::EF_SHIFT_DOWN);
        }
        if (shift_character != multi->subtable[i].shift_character)
          continue;
      }
      if (multi->subtable[i].test_altgr) {
        if (altgr_character == kNonCharacter) {
          altgr_character = XkbSubCharacter(xkb_keycode, xkb_flags, character,
                                            ui::EF_ALTGR_DOWN);
        }
        if (altgr_character != multi->subtable[i].altgr_character)
          continue;
      }
      return multi->subtable[i].key_code;
    }
  }

  // Check the simple character table.
  const PrintableSimpleEntry* simple_end = kSimpleMap + arraysize(kSimpleMap);
  const PrintableSimpleEntry* simple =
      std::lower_bound(kSimpleMap, simple_end, plain_character,
                       [](const PrintableSimpleEntry& e, base::char16 c) {
        return e.plain_character < c;
      });
  if ((simple != simple_end) && (simple->plain_character == plain_character))
    return simple->key_code;

  return VKEY_UNKNOWN;
}

base::char16 XkbKeyboardLayoutEngine::XkbSubCharacter(
    xkb_keycode_t xkb_keycode,
    xkb_mod_mask_t base_flags,
    base::char16 base_character,
    int ui_flags) const {
  xkb_mod_mask_t flags = EventFlagsToXkbFlags(ui_flags);
  if (flags == base_flags)
    return base_character;
  xkb_keysym_t keysym;
  base::char16 character = 0;
  if (!XkbLookup(xkb_keycode, flags, &keysym, &character))
    character = kNone;
  return character;
}

void XkbKeyboardLayoutEngine::ParseLayoutName(const std::string& layout_name,
                                              std::string* layout_id,
                                              std::string* layout_variant) {
  size_t dash_index = layout_name.find('-');
  size_t parentheses_index = layout_name.find('(');
  *layout_id = layout_name;
  *layout_variant = "";
  if (parentheses_index != std::string::npos) {
    *layout_id = layout_name.substr(0, parentheses_index);
    size_t close_index = layout_name.find(')', parentheses_index);
    if (close_index == std::string::npos)
      close_index = layout_name.size();
    *layout_variant = layout_name.substr(parentheses_index + 1,
                                         close_index - parentheses_index - 1);
  } else if (dash_index != std::string::npos) {
    *layout_id = layout_name.substr(0, dash_index);
    *layout_variant = layout_name.substr(dash_index + 1);
  }
}

}  // namespace ui
