// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"

namespace ui {

namespace {

// This XkbKeyCodeConverter simply uses the numeric value of the DomCode.
class VkTestXkbKeyCodeConverter : public XkbKeyCodeConverter {
 public:
  VkTestXkbKeyCodeConverter() {
    invalid_xkb_keycode_ = static_cast<xkb_keycode_t>(DomCode::NONE);
  }
  ~VkTestXkbKeyCodeConverter() override {}
  xkb_keycode_t DomCodeToXkbKeyCode(DomCode dom_code) const override {
    return static_cast<xkb_keycode_t>(dom_code);
  }
};

// This mock XkbKeyboardLayoutEngine fakes a layout that succeeds only when the
// supplied keycode matches its |Entry|, in which case it supplies DomKey::NONE
// and a character that depends on the flags.
class VkTestXkbKeyboardLayoutEngine : public XkbKeyboardLayoutEngine {
 public:
  enum class EntryType { NONE, PRINTABLE, KEYSYM };
  struct PrintableEntry {
    base::char16 plain_character;
    base::char16 shift_character;
    base::char16 altgr_character;
    DomCode dom_code;
  };
  struct KeysymEntry {
    DomCode dom_code;
    xkb_keysym_t keysym;
  };

  struct RuleNames {
    std::string layout_name;
    std::string layout;
    std::string variant;
  };

 public:
  VkTestXkbKeyboardLayoutEngine(const XkbKeyCodeConverter& keycode_converter)
      : XkbKeyboardLayoutEngine(keycode_converter),
        entry_type_(EntryType::NONE),
        printable_entry_(nullptr) {
    // For testing, use the same modifier values as ui::EventFlags.
    static const int kTestFlags[] = {EF_SHIFT_DOWN, EF_ALTGR_DOWN,
                                     EF_MOD3_DOWN};
    xkb_flag_map_.clear();
    xkb_flag_map_.resize(arraysize(kTestFlags));
    for (size_t i = 0; i < arraysize(kTestFlags); ++i) {
      XkbFlagMapEntry e = {kTestFlags[i], kTestFlags[i]};
      xkb_flag_map_.push_back(e);
    }
  }
  ~VkTestXkbKeyboardLayoutEngine() override {}

  void SetEntry(const PrintableEntry* entry) {
    entry_type_ = EntryType::PRINTABLE;
    printable_entry_ = entry;
  }

  void SetEntry(const KeysymEntry* entry) {
    entry_type_ = EntryType::KEYSYM;
    keysym_entry_ = entry;
  }

  xkb_keysym_t CharacterToKeySym(base::char16 c) const {
    return 0x01000000u + c;
  }

  KeyboardCode GetKeyboardCode(DomCode dom_code,
                               int flags,
                               base::char16 character) const {
    KeyboardCode key_code = DifficultKeyboardCode(
        dom_code, flags, key_code_converter_.DomCodeToXkbKeyCode(dom_code),
        flags, CharacterToKeySym(character), DomKey::CHARACTER, character);
    if (key_code == VKEY_UNKNOWN) {
      DomKey dummy_dom_key;
      base::char16 dummy_character;
      // If this fails, key_code remains VKEY_UNKNOWN.
      ignore_result(DomCodeToUsLayoutMeaning(dom_code, EF_NONE, &dummy_dom_key,
                                             &dummy_character, &key_code));
    }
    return key_code;
  }

  // XkbKeyboardLayoutEngine overrides:
  bool XkbLookup(xkb_keycode_t xkb_keycode,
                 xkb_mod_mask_t xkb_flags,
                 xkb_keysym_t* xkb_keysym,
                 base::char16* character) const override {
    switch (entry_type_) {
      case EntryType::NONE:
        break;
      case EntryType::PRINTABLE:
        if (!printable_entry_ ||
            (xkb_keycode !=
             static_cast<xkb_keycode_t>(printable_entry_->dom_code))) {
          return false;
        }
        if (xkb_flags & EF_ALTGR_DOWN)
          *character = printable_entry_->altgr_character;
        else if (xkb_flags & EF_SHIFT_DOWN)
          *character = printable_entry_->shift_character;
        else
          *character = printable_entry_->plain_character;
        *xkb_keysym = CharacterToKeySym(*character);
        return *character != 0;
      case EntryType::KEYSYM:
        if (!keysym_entry_ ||
            (xkb_keycode !=
             static_cast<xkb_keycode_t>(keysym_entry_->dom_code))) {
          return false;
        }
        *xkb_keysym = keysym_entry_->keysym;
        *character = 0;
        return true;
    }
    return false;
  }

 private:
  EntryType entry_type_;
  const PrintableEntry* printable_entry_;
  const KeysymEntry* keysym_entry_;
};

}  // anonymous namespace

class XkbLayoutEngineVkTest : public testing::Test {
 public:
  XkbLayoutEngineVkTest() {}
  ~XkbLayoutEngineVkTest() override {}

  void SetUp() override {
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        make_scoped_ptr(new VkTestXkbKeyboardLayoutEngine(keycode_converter_)));
    layout_engine_ = static_cast<VkTestXkbKeyboardLayoutEngine*>(
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine());
  }
  void TearDown() override {}

 protected:
  VkTestXkbKeyCodeConverter keycode_converter_;
  VkTestXkbKeyboardLayoutEngine* layout_engine_;
};

TEST_F(XkbLayoutEngineVkTest, KeyboardCodeForPrintable) {
  // This table contains U+2460 CIRCLED DIGIT ONE, U+2461 CIRCLED DIGIT TWO,
  // and DomCode::NONE where the result should not depend on those values.
  static const struct {
    VkTestXkbKeyboardLayoutEngine::PrintableEntry test;
    KeyboardCode key_code;
  } kVkeyTestCase[] = {
      // Cases requiring mapping tables.
      // exclamation mark, *, *
      /* 0 */ {{0x0021, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // exclamation mark, *, *
      /* 1 */ {{0x0021, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // exclamation mark, *, *
      /* 2 */ {{0x0021, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_8},
      // quotation mark, *, *
      /* 3 */ {{0x0022, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // quotation mark, *, *
      /* 4 */ {{0x0022, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // number sign, apostrophe, *
      /* 5 */ {{0x0023, 0x0027, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // number sign, tilde, unmapped
      /* 6 */ {{0x0023, 0x007E, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_7},
      // number sign, *, *
      /* 7 */ {{0x0023, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // dollar sign, *, *
      /* 8 */ {{0x0024, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_8},
      // dollar sign, *, *
      /* 9 */ {{0x0024, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // percent sign, *, *
      /* 10 */ {{0x0025, 0x2460, 0x2461, DomCode::NONE}, VKEY_5},
      // ampersand, *, *
      /* 11 */ {{0x0026, 0x2460, 0x2461, DomCode::NONE}, VKEY_1},
      // apostrophe, unmapped, *
      /* 12 */ {{0x0027, 0x0000, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // apostrophe, quotation mark, unmapped
      /* 13 */ {{0x0027, 0x0022, 0x2461, DomCode::KEY_Z}, VKEY_Z},
      // apostrophe, quotation mark, R caron
      /* 14 */ {{0x0027, 0x0022, 0x0158, DomCode::KEY_Z}, VKEY_OEM_7},
      // apostrophe, quotation mark, *
      /* 15 */ {{0x0027, 0x0022, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // apostrophe, quotation mark, *
      /* 16 */ {{0x0027, 0x0022, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // apostrophe, asterisk, unmapped
      /* 17 */ {{0x0027, 0x002A, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // apostrophe, asterisk, unmapped
      /* 18 */ {{0x0027, 0x002A, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // apostrophe, asterisk, vulgar fraction one half
      /* 19 */ {{0x0027, 0x002A, 0x00BD, DomCode::BACKSLASH}, VKEY_OEM_5},
      // apostrophe, asterisk, L stroke
      /* 20 */ {{0x0027, 0x002A, 0x0141, DomCode::BACKSLASH}, VKEY_OEM_2},
      // apostrophe, question mark, unmapped
      /* 21 */ {{0x0027, 0x003F, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // apostrophe, question mark, Y acute
      /* 22 */ {{0x0027, 0x003F, 0x00DD, DomCode::MINUS}, VKEY_OEM_4},
      // apostrophe, commercial at, unmapped
      /* 23 */ {{0x0027, 0x0040, 0x2461, DomCode::QUOTE}, VKEY_OEM_3},
      // apostrophe, middle dot, *
      /* 24 */ {{0x0027, 0x00B7, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // apostrophe, *, *
      /* 25 */ {{0x0027, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // apostrophe, *, *
      /* 26 */ {{0x0027, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // apostrophe, *, *
      /* 27 */ {{0x0027, 0x2460, 0x2461, DomCode::KEY_Q}, VKEY_OEM_7},
      // apostrophe, *, *
      /* 28 */ {{0x0027, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_7},
      // left parenthesis, *, *
      /* 29 */ {{0x0028, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // left parenthesis, *, *
      /* 30 */ {{0x0028, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // left parenthesis, *, *
      /* 31 */ {{0x0028, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // right parenthesis, *, *
      /* 32 */ {{0x0029, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // right parenthesis, *, *
      /* 33 */ {{0x0029, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // right parenthesis, *, *
      /* 34 */ {{0x0029, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // asterisk, *, *
      /* 35 */ {{0x002A, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // asterisk, *, *
      /* 36 */ {{0x002A, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // plus sign, question mark, unmapped
      /* 37 */ {{0x002B, 0x003F, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // plus sign, question mark, reverse solidus
      /* 38 */ {{0x002B, 0x003F, 0x005C, DomCode::MINUS}, VKEY_OEM_MINUS},
      // plus sign, question mark, o double acute
      /* 39 */ {{0x002B, 0x003F, 0x0151, DomCode::MINUS}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 40 */ {{0x002B, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // plus sign, *, *
      /* 41 */ {{0x002B, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 42 */ {{0x002B, 0x2460, 0x2461, DomCode::BRACKET_RIGHT},
                VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 43 */ {{0x002B, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // plus sign, *, *
      /* 44 */ {{0x002B, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 45 */ {{0x002B, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_PLUS},
      // comma, *, *
      /* 46 */ {{0x002C, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_COMMA},
      // comma, *, *
      /* 47 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // comma, *, *
      /* 48 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // comma, *, *
      /* 49 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // comma, *, *
      /* 50 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // comma, *, *
      /* 51 */ {{0x002C, 0x2460, 0x2461, DomCode::KEY_M}, VKEY_OEM_COMMA},
      // comma, *, *
      /* 52 */ {{0x002C, 0x2460, 0x2461, DomCode::KEY_V}, VKEY_OEM_COMMA},
      // comma, *, *
      /* 53 */ {{0x002C, 0x2460, 0x2461, DomCode::KEY_W}, VKEY_OEM_COMMA},
      // hyphen-minus, equals sign, *
      /* 54 */ {{0x002D, 0x003D, 0x2461, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, unmapped
      /* 55 */ {{0x002D, 0x005F, 0x2461, DomCode::EQUAL}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, unmapped
      /* 56 */ {{0x002D, 0x005F, 0x2461, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, asterisk
      /* 57 */ {{0x002D, 0x005F, 0x002A, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, solidus
      /* 58 */ {{0x002D, 0x005F, 0x002F, DomCode::SLASH}, VKEY_OEM_2},
      // hyphen-minus, low line, n
      /* 59 */ {{0x002D, 0x005F, 0x006E, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, r cedilla
      /* 60 */ {{0x002D, 0x005F, 0x0157, DomCode::EQUAL}, VKEY_OEM_4},
      // hyphen-minus, *, *
      /* 61 */ {{0x002D, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // hyphen-minus, *, *
      /* 62 */ {{0x002D, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // hyphen-minus, *, *
      /* 63 */ {{0x002D, 0x2460, 0x2461, DomCode::KEY_A}, VKEY_OEM_MINUS},
      // hyphen-minus, *, *
      /* 64 */ {{0x002D, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_MINUS},
      // hyphen-minus, *, *
      /* 65 */ {{0x002D, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_MINUS},
      // full stop, *, *
      /* 66 */ {{0x002E, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // full stop, *, *
      /* 67 */ {{0x002E, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // full stop, *, *
      /* 68 */ {{0x002E, 0x2460, 0x2461, DomCode::KEY_E}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 69 */ {{0x002E, 0x2460, 0x2461, DomCode::KEY_O}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 70 */ {{0x002E, 0x2460, 0x2461, DomCode::KEY_R}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 71 */ {{0x002E, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 72 */ {{0x002E, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // full stop, *, *
      /* 73 */ {{0x002E, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // solidus, digit zero, *
      /* 74 */ {{0x002F, 0x0030, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // solidus, digit three, *
      /* 75 */ {{0x002F, 0x0033, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // solidus, question mark, *
      /* 76 */ {{0x002F, 0x003F, 0x2461, DomCode::DIGIT0}, VKEY_OEM_2},
      // solidus, question mark, *
      /* 77 */ {{0x002F, 0x003F, 0x2461, DomCode::DIGIT3}, VKEY_OEM_2},
      // solidus, *, *
      /* 78 */ {{0x002F, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // solidus, *, *
      /* 79 */ {{0x002F, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // solidus, *, *
      /* 80 */ {{0x002F, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_2},
      // solidus, *, *
      /* 81 */ {{0x002F, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // solidus, *, *
      /* 82 */ {{0x002F, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // colon, *, *
      /* 83 */ {{0x003A, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // colon, *, *
      /* 84 */ {{0x003A, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // colon, *, *
      /* 85 */ {{0x003A, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // colon, *, *
      /* 86 */ {{0x003A, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_2},
      // semicolon, *, *
      /* 87 */ {{0x003B, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // semicolon, *, *
      /* 88 */ {{0x003B, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // semicolon, *, *
      /* 89 */ {{0x003B, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // semicolon, *, *
      /* 90 */ {{0x003B, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_PERIOD},
      // semicolon, *, *
      /* 91 */ {{0x003B, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // semicolon, *, *
      /* 92 */ {{0x003B, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // semicolon, *, *
      /* 93 */ {{0x003B, 0x2460, 0x2461, DomCode::KEY_Q}, VKEY_OEM_1},
      // semicolon, *, *
      /* 94 */ {{0x003B, 0x2460, 0x2461, DomCode::KEY_Z}, VKEY_OEM_1},
      // semicolon, *, *
      /* 95 */ {{0x003B, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // semicolon, *, *
      /* 96 */ {{0x003B, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // less-than sign, *, *
      /* 97 */ {{0x003C, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // equals sign, percent sign, unmapped
      /* 98 */ {{0x003D, 0x0025, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // equals sign, percent sign, hyphen-minus
      /* 99 */ {{0x003D, 0x0025, 0x002D, DomCode::MINUS}, VKEY_OEM_MINUS},
      // equals sign, percent sign, *
      /* 100 */ {{0x003D, 0x0025, 0x2461, DomCode::SLASH}, VKEY_OEM_8},
      // equals sign, plus sign, *
      /* 101 */ {{0x003D, 0x002B, 0x2461, DomCode::SLASH}, VKEY_OEM_PLUS},
      // equals sign, *, *
      /* 102 */ {{0x003D, 0x2460, 0x2461, DomCode::BRACKET_RIGHT},
                 VKEY_OEM_PLUS},
      // equals sign, *, *
      /* 103 */ {{0x003D, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // equals sign, *, *
      /* 104 */ {{0x003D, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // question mark, *, *
      /* 105 */ {{0x003F, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // question mark, *, *
      /* 106 */ {{0x003F, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // question mark, *, *
      /* 107 */ {{0x003F, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // question mark, *, *
      /* 108 */ {{0x003F, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // commercial at, *, *
      /* 109 */ {{0x0040, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // commercial at, *, *
      /* 110 */ {{0x0040, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // left square bracket, *, *
      /* 111 */ {{0x005B, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // left square bracket, *, *
      /* 112 */ {{0x005B, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // left square bracket, *, *
      /* 113 */ {{0x005B, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_OEM_4},
      // left square bracket, *, *
      /* 114 */ {{0x005B, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // left square bracket, *, *
      /* 115 */ {{0x005B, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // reverse solidus, solidus, *
      /* 116 */ {{0x005C, 0x002F, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_7},
      // reverse solidus, vertical line, digit one
      /* 117 */ {{0x005C, 0x007C, 0x0031, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // reverse solidus, vertical line, N cedilla
      /* 118 */ {{0x005C, 0x007C, 0x0145, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // reverse solidus, vertical line, *
      /* 119 */ {{0x005C, 0x007C, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // reverse solidus, *, *
      /* 120 */ {{0x005C, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_4},
      // right square bracket, *, *
      /* 121 */ {{0x005D, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // right square bracket, *, *
      /* 122 */ {{0x005D, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // right square bracket, *, *
      /* 123 */ {{0x005D, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // right square bracket, *, *
      /* 124 */ {{0x005D, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_OEM_6},
      // right square bracket, *, *
      /* 125 */ {{0x005D, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_6},
      // low line, *, *
      /* 126 */ {{0x005F, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // low line, *, *
      /* 127 */ {{0x005F, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_MINUS},
      // grave accent, unmapped, *
      /* 128 */ {{0x0060, 0x0000, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, unmapped
      /* 129 */ {{0x0060, 0x007E, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, digit one
      /* 130 */ {{0x0060, 0x007E, 0x0031, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, semicolon
      /* 131 */ {{0x0060, 0x007E, 0x003B, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, grave accent
      /* 132 */ {{0x0060, 0x007E, 0x0060, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, inverted question mark
      /* 133 */ {{0x0060, 0x007E, 0x00BF, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, o double acute
      /* 134 */ {{0x0060, 0x007E, 0x0151, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, not sign, *
      /* 135 */ {{0x0060, 0x00AC, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_8},
      // left curly bracket, *, *
      /* 136 */ {{0x007B, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // vertical line, *, *
      /* 137 */ {{0x007C, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // right curly bracket, *, *
      /* 138 */ {{0x007D, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_2},
      // tilde, *, *
      /* 139 */ {{0x007E, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // inverted exclamation mark, *, *
      /* 140 */ {{0x00A1, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // section sign, degree sign, *
      /* 141 */ {{0x00A7, 0x00B0, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_2},
      // section sign, vulgar fraction one half, *
      /* 142 */ {{0x00A7, 0x00BD, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // section sign, *, *
      /* 143 */ {{0x00A7, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // section sign, *, *
      /* 144 */ {{0x00A7, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // section sign, *, *
      /* 145 */ {{0x00A7, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // left-pointing double angle quotation mark, *, *
      /* 146 */ {{0x00AB, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // left-pointing double angle quotation mark, *, *
      /* 147 */ {{0x00AB, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_6},
      // soft hyphen, *, *
      /* 148 */ {{0x00AD, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // degree sign, *, *
      /* 149 */ {{0x00B0, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // degree sign, *, *
      /* 150 */ {{0x00B0, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_2},
      // superscript two, *, *
      /* 151 */ {{0x00B2, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // micro sign, *, *
      /* 152 */ {{0x00B5, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // masculine ordinal indicator, *, *
      /* 153 */ {{0x00BA, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // masculine ordinal indicator, *, *
      /* 154 */ {{0x00BA, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // right-pointing double angle quotation mark, *, *
      /* 155 */ {{0x00BB, 0x2460, 0x2461, DomCode::NONE}, VKEY_9},
      // vulgar fraction one half, *, *
      /* 156 */ {{0x00BD, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // inverted question mark, *, *
      /* 157 */ {{0x00BF, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // sharp s, *, *
      /* 158 */ {{0x00DF, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // a grave, degree sign, *
      /* 159 */ {{0x00E0, 0x00B0, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // a grave, a diaeresis, *
      /* 160 */ {{0x00E0, 0x00E4, 0x2461, DomCode::QUOTE}, VKEY_OEM_5},
      // a grave, *, *
      /* 161 */ {{0x00E0, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // a grave, *, *
      /* 162 */ {{0x00E0, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // a acute, *, *
      /* 163 */ {{0x00E1, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // a acute, *, *
      /* 164 */ {{0x00E1, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // a circumflex, *, *
      /* 165 */ {{0x00E2, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // a circumflex, *, *
      /* 166 */ {{0x00E2, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // a diaeresis, A diaeresis, unmapped
      /* 167 */ {{0x00E4, 0x00C4, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // a diaeresis, A diaeresis, r caron
      /* 168 */ {{0x00E4, 0x00C4, 0x0159, DomCode::QUOTE}, VKEY_OEM_7},
      // a diaeresis, A diaeresis, S acute
      /* 169 */ {{0x00E4, 0x00C4, 0x015A, DomCode::QUOTE}, VKEY_OEM_7},
      // a diaeresis, a grave, *
      /* 170 */ {{0x00E4, 0x00E0, 0x2461, DomCode::QUOTE}, VKEY_OEM_5},
      // a diaeresis, *, *
      /* 171 */ {{0x00E4, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // a ring above, *, *
      /* 172 */ {{0x00E5, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // ae, *, *
      /* 173 */ {{0x00E6, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // ae, *, *
      /* 174 */ {{0x00E6, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // c cedilla, C cedilla, unmapped
      /* 175 */ {{0x00E7, 0x00C7, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // c cedilla, C cedilla, Thorn
      /* 176 */ {{0x00E7, 0x00C7, 0x00DE, DomCode::SEMICOLON}, VKEY_OEM_3},
      // c cedilla, *, *
      /* 177 */ {{0x00E7, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // c cedilla, *, *
      /* 178 */ {{0x00E7, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // c cedilla, *, *
      /* 179 */ {{0x00E7, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // c cedilla, *, *
      /* 180 */ {{0x00E7, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_COMMA},
      // c cedilla, *, *
      /* 181 */ {{0x00E7, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // c cedilla, *, *
      /* 182 */ {{0x00E7, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // e grave, *, *
      /* 183 */ {{0x00E8, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // e grave, *, *
      /* 184 */ {{0x00E8, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // e grave, *, *
      /* 185 */ {{0x00E8, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_3},
      // e acute, E acute, *
      /* 186 */ {{0x00E9, 0x00C9, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // e acute, o diaeresis, *
      /* 187 */ {{0x00E9, 0x00F6, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_7},
      // e acute, *, *
      /* 188 */ {{0x00E9, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // e acute, *, *
      /* 189 */ {{0x00E9, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // e acute, *, *
      /* 190 */ {{0x00E9, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // e circumflex, *, *
      /* 191 */ {{0x00EA, 0x2460, 0x2461, DomCode::NONE}, VKEY_3},
      // e diaeresis, *, *
      /* 192 */ {{0x00EB, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_1},
      // i grave, *, *
      /* 193 */ {{0x00EC, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // i acute, *, *
      /* 194 */ {{0x00ED, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_0},
      // i acute, *, *
      /* 195 */ {{0x00ED, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // i circumflex, *, *
      /* 196 */ {{0x00EE, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // eth, *, *
      /* 197 */ {{0x00F0, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_6},
      // eth, *, *
      /* 198 */ {{0x00F0, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // n tilde, *, *
      /* 199 */ {{0x00F1, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // o grave, *, *
      /* 200 */ {{0x00F2, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // o acute, *, *
      /* 201 */ {{0x00F3, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // o acute, *, *
      /* 202 */ {{0x00F3, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // o circumflex, *, *
      /* 203 */ {{0x00F4, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // o circumflex, *, *
      /* 204 */ {{0x00F4, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // o tilde, *, *
      /* 205 */ {{0x00F5, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // o diaeresis, O diaeresis, unmapped
      /* 206 */ {{0x00F6, 0x00D6, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // o diaeresis, O diaeresis, T cedilla
      /* 207 */ {{0x00F6, 0x00D6, 0x0162, DomCode::SEMICOLON}, VKEY_OEM_3},
      // o diaeresis, e acute, *
      /* 208 */ {{0x00F6, 0x00E9, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_7},
      // o diaeresis, *, *
      /* 209 */ {{0x00F6, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // o diaeresis, *, *
      /* 210 */ {{0x00F6, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_OEM_3},
      // o diaeresis, *, *
      /* 211 */ {{0x00F6, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // division sign, *, *
      /* 212 */ {{0x00F7, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // o stroke, *, *
      /* 213 */ {{0x00F8, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // o stroke, *, *
      /* 214 */ {{0x00F8, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // u grave, *, *
      /* 215 */ {{0x00F9, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // u grave, *, *
      /* 216 */ {{0x00F9, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_3},
      // u acute, *, *
      /* 217 */ {{0x00FA, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // u acute, *, *
      /* 218 */ {{0x00FA, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // u diaeresis, U diaeresis, unmapped
      /* 219 */ {{0x00FC, 0x00DC, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // u diaeresis, U diaeresis, unmapped
      /* 220 */ {{0x00FC, 0x00DC, 0x2461, DomCode::MINUS}, VKEY_OEM_2},
      // u diaeresis, U diaeresis, L stroke
      /* 221 */ {{0x00FC, 0x00DC, 0x0141, DomCode::BRACKET_LEFT}, VKEY_OEM_3},
      // u diaeresis, e grave, *
      /* 222 */ {{0x00FC, 0x00E8, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // u diaeresis, *, *
      /* 223 */ {{0x00FC, 0x2460, 0x2461, DomCode::KEY_W}, VKEY_W},
      // y acute, *, *
      /* 224 */ {{0x00FD, 0x2460, 0x2461, DomCode::NONE}, VKEY_7},
      // thorn, *, *
      /* 225 */ {{0x00FE, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_MINUS},
      // a macron, *, *
      /* 226 */ {{0x0101, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_8},
      // a breve, *, *
      /* 227 */ {{0x0103, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // a breve, *, *
      /* 228 */ {{0x0103, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // a ogonek, *, *
      /* 229 */ {{0x0105, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // a ogonek, *, *
      /* 230 */ {{0x0105, 0x2460, 0x2461, DomCode::KEY_Q}, VKEY_Q},
      // a ogonek, *, *
      /* 231 */ {{0x0105, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // c acute, *, *
      /* 232 */ {{0x0107, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // c dot above, *, *
      /* 233 */ {{0x010B, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // c caron, *, *
      /* 234 */ {{0x010D, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_COMMA},
      // c caron, *, *
      /* 235 */ {{0x010D, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // c caron, *, *
      /* 236 */ {{0x010D, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // c caron, *, *
      /* 237 */ {{0x010D, 0x2460, 0x2461, DomCode::KEY_P}, VKEY_X},
      // c caron, *, *
      /* 238 */ {{0x010D, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // d stroke, *, *
      /* 239 */ {{0x0111, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // d stroke, *, *
      /* 240 */ {{0x0111, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // e macron, *, *
      /* 241 */ {{0x0113, 0x2460, 0x2461, DomCode::NONE}, VKEY_W},
      // e dot above, *, *
      /* 242 */ {{0x0117, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // e dot above, *, *
      /* 243 */ {{0x0117, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // e ogonek, E ogonek, unmapped
      /* 244 */ {{0x0119, 0x0118, 0x2461, DomCode::SLASH}, VKEY_OEM_MINUS},
      // e ogonek, E ogonek, n
      /* 245 */ {{0x0119, 0x0118, 0x006E, DomCode::SLASH}, VKEY_OEM_2},
      // e ogonek, *, *
      /* 246 */ {{0x0119, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // e caron, *, *
      /* 247 */ {{0x011B, 0x2460, 0x2461, DomCode::NONE}, VKEY_2},
      // g breve, *, *
      /* 248 */ {{0x011F, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // g dot above, *, *
      /* 249 */ {{0x0121, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // h stroke, *, *
      /* 250 */ {{0x0127, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // i macron, *, *
      /* 251 */ {{0x012B, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // i ogonek, I ogonek, unmapped
      /* 252 */ {{0x012F, 0x012E, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // i ogonek, *, *
      /* 253 */ {{0x012F, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // dotless i, *, *
      /* 254 */ {{0x0131, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_1},
      // k cedilla, *, *
      /* 255 */ {{0x0137, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // l cedilla, *, *
      /* 256 */ {{0x013C, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_2},
      // l caron, *, *
      /* 257 */ {{0x013E, 0x2460, 0x2461, DomCode::NONE}, VKEY_2},
      // l stroke, *, *
      /* 258 */ {{0x0142, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // l stroke, *, *
      /* 259 */ {{0x0142, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // n cedilla, *, *
      /* 260 */ {{0x0146, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // n caron, *, *
      /* 261 */ {{0x0148, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // o double acute, *, *
      /* 262 */ {{0x0151, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // r caron, *, *
      /* 263 */ {{0x0159, 0x2460, 0x2461, DomCode::NONE}, VKEY_5},
      // s cedilla, *, *
      /* 264 */ {{0x015F, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_PERIOD},
      // s cedilla, *, *
      /* 265 */ {{0x015F, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // s caron, *, *
      /* 266 */ {{0x0161, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // s caron, *, *
      /* 267 */ {{0x0161, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // s caron, *, *
      /* 268 */ {{0x0161, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // s caron, *, *
      /* 269 */ {{0x0161, 0x2460, 0x2461, DomCode::KEY_A}, VKEY_OEM_1},
      // s caron, *, *
      /* 270 */ {{0x0161, 0x2460, 0x2461, DomCode::KEY_F}, VKEY_F},
      // s caron, *, *
      /* 271 */ {{0x0161, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_PERIOD},
      // t cedilla, *, *
      /* 272 */ {{0x0163, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // t caron, *, *
      /* 273 */ {{0x0165, 0x2460, 0x2461, DomCode::NONE}, VKEY_5},
      // u macron, *, *
      /* 274 */ {{0x016B, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // u macron, *, *
      /* 275 */ {{0x016B, 0x2460, 0x2461, DomCode::KEY_Q}, VKEY_Q},
      // u macron, *, *
      /* 276 */ {{0x016B, 0x2460, 0x2461, DomCode::KEY_X}, VKEY_X},
      // u ring above, *, *
      /* 277 */ {{0x016F, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_1},
      // u double acute, *, *
      /* 278 */ {{0x0171, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // u ogonek, U ogonek, unmapped
      /* 279 */ {{0x0173, 0x0172, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // u ogonek, U ogonek, T cedilla
      /* 280 */ {{0x0173, 0x0172, 0x0162, DomCode::SEMICOLON}, VKEY_OEM_1},
      // u ogonek, *, *
      /* 281 */ {{0x0173, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // z dot above, *, *
      /* 282 */ {{0x017C, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // z dot above, *, *
      /* 283 */ {{0x017C, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // z caron, *, *
      /* 284 */ {{0x017E, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // z caron, *, *
      /* 285 */ {{0x017E, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_Y},
      // z caron, *, *
      /* 286 */ {{0x017E, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // z caron, *, *
      /* 287 */ {{0x017E, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // z caron, *, *
      /* 288 */ {{0x017E, 0x2460, 0x2461, DomCode::KEY_W}, VKEY_W},
      // o horn, *, *
      /* 289 */ {{0x01A1, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // u horn, *, *
      /* 290 */ {{0x01B0, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // z stroke, *, *
      /* 291 */ {{0x01B6, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // schwa, *, *
      /* 292 */ {{0x0259, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},

      // Simple alphanumeric cases.
      /* 293 */ {{'a', 'A', '?', DomCode::NONE}, VKEY_A},
      /* 294 */ {{'z', 'Z', '!', DomCode::NONE}, VKEY_Z},
      /* 295 */ {{'9', '(', '+', DomCode::NONE}, VKEY_9},
      /* 296 */ {{'0', ')', '-', DomCode::NONE}, VKEY_0},
  };

  for (size_t i = 0; i < arraysize(kVkeyTestCase); ++i) {
    SCOPED_TRACE(i);
    const auto& e = kVkeyTestCase[i];
    layout_engine_->SetEntry(&e.test);

    // Test with predetermined plain character.
    KeyboardCode key_code = layout_engine_->GetKeyboardCode(
        e.test.dom_code, EF_NONE, e.test.plain_character);
    EXPECT_EQ(e.key_code, key_code);

    if (e.test.shift_character) {
      // Test with predetermined shifted character.
      key_code = layout_engine_->GetKeyboardCode(e.test.dom_code, EF_SHIFT_DOWN,
                                                 e.test.shift_character);
      EXPECT_EQ(e.key_code, key_code);
    }

    if (e.test.altgr_character) {
      // Test with predetermined AltGr character.
      key_code = layout_engine_->GetKeyboardCode(e.test.dom_code, EF_ALTGR_DOWN,
                                                 e.test.altgr_character);
      EXPECT_EQ(e.key_code, key_code);
    }

    // Test with unrelated predetermined character.
    key_code =
        layout_engine_->GetKeyboardCode(e.test.dom_code, EF_MOD3_DOWN, 0xFFFFu);
    EXPECT_EQ(e.key_code, key_code);
  }
}

TEST_F(XkbLayoutEngineVkTest, KeyboardCodeForNonPrintable) {
  static const struct {
    VkTestXkbKeyboardLayoutEngine::KeysymEntry test;
    KeyboardCode key_code;
  } kVkeyTestCase[] = {
    {{DomCode::CONTROL_LEFT, XKB_KEY_Control_L}, VKEY_CONTROL},
    {{DomCode::CONTROL_RIGHT, XKB_KEY_Control_R}, VKEY_CONTROL},
    {{DomCode::SHIFT_LEFT, XKB_KEY_Shift_L}, VKEY_SHIFT},
    {{DomCode::SHIFT_RIGHT, XKB_KEY_Shift_R}, VKEY_SHIFT},
    {{DomCode::OS_LEFT, XKB_KEY_Super_L}, VKEY_LWIN},
    {{DomCode::OS_RIGHT, XKB_KEY_Super_R}, VKEY_LWIN},
    {{DomCode::ALT_LEFT, XKB_KEY_Alt_L}, VKEY_MENU},
    {{DomCode::ALT_RIGHT, XKB_KEY_Alt_R}, VKEY_MENU},
    {{DomCode::ALT_RIGHT, XKB_KEY_ISO_Level3_Shift}, VKEY_ALTGR},
    {{DomCode::DIGIT1, XKB_KEY_1}, VKEY_1},
    {{DomCode::NUMPAD1, XKB_KEY_KP_1}, VKEY_1},
    {{DomCode::CAPS_LOCK, XKB_KEY_Caps_Lock}, VKEY_CAPITAL},
    {{DomCode::ENTER, XKB_KEY_Return}, VKEY_RETURN},
    {{DomCode::NUMPAD_ENTER, XKB_KEY_KP_Enter}, VKEY_RETURN},
    {{DomCode::SLEEP, XKB_KEY_XF86Sleep}, VKEY_SLEEP},
  };
  for (const auto& e : kVkeyTestCase) {
    SCOPED_TRACE(static_cast<int>(e.test.dom_code));
    layout_engine_->SetEntry(&e.test);
    DomKey dom_key = DomKey::NONE;
    base::char16 character = 0;
    KeyboardCode key_code = VKEY_UNKNOWN;
    uint32_t keysym;
    EXPECT_TRUE(layout_engine_->Lookup(e.test.dom_code, EF_NONE, &dom_key,
                                       &character, &key_code, &keysym));
    EXPECT_EQ(e.test.keysym, keysym);
    EXPECT_EQ(e.key_code, key_code);
  }
}


TEST_F(XkbLayoutEngineVkTest, XkbRuleNamesForLayoutName) {
  static const VkTestXkbKeyboardLayoutEngine::RuleNames kVkeyTestCase[] = {
      /* 0 */ {"us", "us", ""},
      /* 1 */ {"jp", "jp", ""},
      /* 2 */ {"us(intl)", "us", "intl"},
      /* 3 */ {"us(altgr-intl)", "us", "altgr-intl"},
      /* 4 */ {"us(dvorak)", "us", "dvorak"},
      /* 5 */ {"us(colemak)", "us", "colemak"},
      /* 6 */ {"be", "be", ""},
      /* 7 */ {"fr", "fr", ""},
      /* 8 */ {"ca", "ca", ""},
      /* 9 */ {"ch(fr)", "ch", "fr"},
      /* 10 */ {"ca(multix)", "ca", "multix"},
      /* 11 */ {"de", "de", ""},
      /* 12 */ {"de(neo)", "de", "neo"},
      /* 13 */ {"ch", "ch", ""},
      /* 14 */ {"ru", "ru", ""},
      /* 15 */ {"ru(phonetic)", "ru", "phonetic"},
      /* 16 */ {"br", "br", ""},
      /* 17 */ {"bg", "bg", ""},
      /* 18 */ {"bg(phonetic)", "bg", "phonetic"},
      /* 19 */ {"ca(eng)", "ca", "eng"},
      /* 20 */ {"cz", "cz", ""},
      /* 21 */ {"cz(qwerty)", "cz", "qwerty"},
      /* 22 */ {"ee", "ee", ""},
      /* 23 */ {"es", "es", ""},
      /* 24 */ {"es(cat)", "es", "cat"},
      /* 25 */ {"dk", "dk", ""},
      /* 26 */ {"gr", "gr", ""},
      /* 27 */ {"il", "il", ""},
      /* 28 */ {"latam", "latam", ""},
      /* 29 */ {"lt", "lt", ""},
      /* 30 */ {"lv(apostrophe)", "lv", "apostrophe"},
      /* 31 */ {"hr", "hr", ""},
      /* 32 */ {"gb(extd)", "gb", "extd"},
      /* 33 */ {"gb(dvorak)", "gb", "dvorak"},
      /* 34 */ {"fi", "fi", ""},
      /* 35 */ {"hu", "hu", ""},
      /* 36 */ {"it", "it", ""},
      /* 37 */ {"is", "is", ""},
      /* 38 */ {"no", "no", ""},
      /* 39 */ {"pl", "pl", ""},
      /* 40 */ {"pt", "pt", ""},
      /* 41 */ {"ro", "ro", ""},
      /* 42 */ {"se", "se", ""},
      /* 43 */ {"sk", "sk", ""},
      /* 44 */ {"si", "si", ""},
      /* 45 */ {"rs", "rs", ""},
      /* 46 */ {"tr", "tr", ""},
      /* 47 */ {"ua", "ua", ""},
      /* 48 */ {"by", "by", ""},
      /* 49 */ {"am", "am", ""},
      /* 50 */ {"ge", "ge", ""},
      /* 51 */ {"mn", "mn", ""},
      /* 52 */ {"ie", "ie", ""}};
  for (size_t i = 0; i < arraysize(kVkeyTestCase); ++i) {
    SCOPED_TRACE(i);
    const VkTestXkbKeyboardLayoutEngine::RuleNames* e = &kVkeyTestCase[i];
    std::string layout_id;
    std::string layout_variant;
    XkbKeyboardLayoutEngine::ParseLayoutName(e->layout_name, &layout_id,
                                             &layout_variant);
    EXPECT_EQ(layout_id, e->layout);
    EXPECT_EQ(layout_variant, e->variant);
  }
}

}  // namespace ui
