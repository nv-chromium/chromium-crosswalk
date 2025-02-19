// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_name_match.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/cert/internal/test_helpers.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

der::Input SequenceValueFromString(const std::string* s) {
  der::Parser parser(InputFromString(s));
  der::Input data;
  if (!parser.ReadTag(der::kSequence, &data)) {
    ADD_FAILURE();
    return der::Input();
  }
  if (parser.HasMore()) {
    ADD_FAILURE();
    return der::Input();
  }
  return data;
}

// Loads test data from file. The filename is constructed from the parameters:
// |prefix| describes the type of data being tested, e.g. "ascii",
// "unicode_bmp", "unicode_supplementary", and "invalid".
// |value_type| indicates what ASN.1 type is used to encode the data.
// |suffix| indicates any additional modifications, such as caseswapping,
// whitespace adding, etc.
::testing::AssertionResult LoadTestData(const std::string& prefix,
                                        const std::string& value_type,
                                        const std::string& suffix,
                                        std::string* result) {
  std::string path = "net/data/verify_name_match_unittest/names/" + prefix +
                     "-" + value_type + "-" + suffix + ".pem";

  const PemBlockMapping mappings[] = {
      {"NAME", result},
  };

  return ReadTestDataFromPemFile(path, mappings);
}

bool TypesAreComparable(const std::string& type_1, const std::string& type_2) {
  if (type_1 == type_2)
    return true;
  if ((type_1 == "PRINTABLESTRING" || type_1 == "UTF8" ||
       type_1 == "BMPSTRING" || type_1 == "UNIVERSALSTRING") &&
      (type_2 == "PRINTABLESTRING" || type_2 == "UTF8" ||
       type_2 == "BMPSTRING" || type_2 == "UNIVERSALSTRING")) {
    return true;
  }
  return false;
}

// All string types.
static const char* kValueTypes[] = {"PRINTABLESTRING", "T61STRING", "UTF8",
                                    "BMPSTRING", "UNIVERSALSTRING"};
// String types that can encode the Unicode Basic Multilingual Plane.
static const char* kUnicodeBMPValueTypes[] = {"UTF8", "BMPSTRING",
                                              "UNIVERSALSTRING"};
// String types that can encode the Unicode Supplementary Planes.
static const char* kUnicodeSupplementaryValueTypes[] = {"UTF8",
                                                        "UNIVERSALSTRING"};

static const char* kMangleTypes[] = {"unmangled", "case_swap",
                                     "extra_whitespace"};

}  // namespace

class VerifyNameMatchSimpleTest
    : public ::testing::TestWithParam<
          ::testing::tuple<const char*, const char*>> {
 public:
  std::string value_type() const { return ::testing::get<0>(GetParam()); }
  std::string suffix() const { return ::testing::get<1>(GetParam()); }
};

// Compare each input against itself, verifies that all input data is parsed
// successfully.
TEST_P(VerifyNameMatchSimpleTest, ExactEquality) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix(), &der));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&der),
                              SequenceValueFromString(&der)));

  std::string der_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_attr",
                           &der_extra_attr));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&der_extra_attr),
                              SequenceValueFromString(&der_extra_attr)));

  std::string der_extra_rdn;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_rdn",
                           &der_extra_rdn));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&der_extra_rdn),
                              SequenceValueFromString(&der_extra_rdn)));
}

// Ensure that a Name does not match another Name which is exactly the same but
// with an extra attribute in one Relative Distinguished Name.
TEST_P(VerifyNameMatchSimpleTest, ExtraAttrDoesNotMatch) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix(), &der));
  std::string der_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_attr",
                           &der_extra_attr));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&der),
                               SequenceValueFromString(&der_extra_attr)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&der_extra_attr),
                               SequenceValueFromString(&der)));
}

// Ensure that a Name does not match another Name which is exactly the same but
// with an extra Relative Distinguished Name.
TEST_P(VerifyNameMatchSimpleTest, ExtraRdnDoesNotMatch) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix(), &der));
  std::string der_extra_rdn;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_rdn",
                           &der_extra_rdn));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&der),
                               SequenceValueFromString(&der_extra_rdn)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&der_extra_rdn),
                               SequenceValueFromString(&der)));
}

// Runs VerifyNameMatchSimpleTest for all combinations of value_type and and
// suffix.
INSTANTIATE_TEST_CASE_P(InstantiationName,
                        VerifyNameMatchSimpleTest,
                        ::testing::Combine(::testing::ValuesIn(kValueTypes),
                                           ::testing::ValuesIn(kMangleTypes)));

class VerifyNameMatchNormalizationTest
    : public ::testing::TestWithParam<::testing::tuple<bool, const char*>> {
 public:
  bool expected_result() const { return ::testing::get<0>(GetParam()); }
  std::string value_type() const { return ::testing::get<1>(GetParam()); }
};

// Verify matching is case insensitive (for the types which currently support
// normalization).
TEST_P(VerifyNameMatchNormalizationTest, CaseInsensitivity) {
  std::string normal;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), "unmangled", &normal));
  std::string case_swap;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), "case_swap", &case_swap));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(&normal),
                            SequenceValueFromString(&case_swap)));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(&case_swap),
                            SequenceValueFromString(&normal)));
}

// Verify matching folds whitespace (for the types which currently support
// normalization).
TEST_P(VerifyNameMatchNormalizationTest, CollapseWhitespace) {
  std::string normal;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), "unmangled", &normal));
  std::string whitespace;
  ASSERT_TRUE(
      LoadTestData("ascii", value_type(), "extra_whitespace", &whitespace));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(&normal),
                            SequenceValueFromString(&whitespace)));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(&whitespace),
                            SequenceValueFromString(&normal)));
}

// Runs VerifyNameMatchNormalizationTest for each (expected_result, value_type)
// tuple.
INSTANTIATE_TEST_CASE_P(
    InstantiationName,
    VerifyNameMatchNormalizationTest,
    ::testing::Values(
        ::testing::make_tuple(true,
                              static_cast<const char*>("PRINTABLESTRING")),
        ::testing::make_tuple(false, static_cast<const char*>("T61STRING")),
        ::testing::make_tuple(true, static_cast<const char*>("UTF8")),
        ::testing::make_tuple(true, static_cast<const char*>("BMPSTRING")),
        ::testing::make_tuple(true,
                              static_cast<const char*>("UNIVERSALSTRING"))));

class VerifyNameMatchDifferingTypesTest
    : public ::testing::TestWithParam<
          ::testing::tuple<const char*, const char*>> {
 public:
  std::string value_type_1() const { return ::testing::get<0>(GetParam()); }
  std::string value_type_2() const { return ::testing::get<1>(GetParam()); }
};

TEST_P(VerifyNameMatchDifferingTypesTest, NormalizableTypesAreEqual) {
  std::string der_1;
  ASSERT_TRUE(LoadTestData("ascii", value_type_1(), "unmangled", &der_1));
  std::string der_2;
  ASSERT_TRUE(LoadTestData("ascii", value_type_2(), "unmangled", &der_2));
  if (TypesAreComparable(value_type_1(), value_type_2())) {
    EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&der_1),
                                SequenceValueFromString(&der_2)));
  } else {
    EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&der_1),
                                 SequenceValueFromString(&der_2)));
  }
}

// Runs VerifyNameMatchDifferingTypesTest for all combinations of value types in
// value_type1 and value_type_2.
INSTANTIATE_TEST_CASE_P(InstantiationName,
                        VerifyNameMatchDifferingTypesTest,
                        ::testing::Combine(::testing::ValuesIn(kValueTypes),
                                           ::testing::ValuesIn(kValueTypes)));

class VerifyNameMatchUnicodeConversionTest
    : public ::testing::TestWithParam<
          ::testing::tuple<const char*,
                           ::testing::tuple<const char*, const char*>>> {
 public:
  std::string prefix() const { return ::testing::get<0>(GetParam()); }
  std::string value_type_1() const {
    return ::testing::get<0>(::testing::get<1>(GetParam()));
  }
  std::string value_type_2() const {
    return ::testing::get<1>(::testing::get<1>(GetParam()));
  }
};

TEST_P(VerifyNameMatchUnicodeConversionTest, UnicodeConversionsAreEqual) {
  std::string der_1;
  ASSERT_TRUE(LoadTestData(prefix(), value_type_1(), "unmangled", &der_1));
  std::string der_2;
  ASSERT_TRUE(LoadTestData(prefix(), value_type_2(), "unmangled", &der_2));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&der_1),
                              SequenceValueFromString(&der_2)));
}

// Runs VerifyNameMatchUnicodeConversionTest with prefix="unicode_bmp" for all
// combinations of Basic Multilingual Plane-capable value types in value_type1
// and value_type_2.
INSTANTIATE_TEST_CASE_P(
    BMPConversion,
    VerifyNameMatchUnicodeConversionTest,
    ::testing::Combine(
        ::testing::Values("unicode_bmp"),
        ::testing::Combine(::testing::ValuesIn(kUnicodeBMPValueTypes),
                           ::testing::ValuesIn(kUnicodeBMPValueTypes))));

// Runs VerifyNameMatchUnicodeConversionTest with prefix="unicode_supplementary"
// for all combinations of Unicode Supplementary Plane-capable value types in
// value_type1 and value_type_2.
INSTANTIATE_TEST_CASE_P(
    SMPConversion,
    VerifyNameMatchUnicodeConversionTest,
    ::testing::Combine(
        ::testing::Values("unicode_supplementary"),
        ::testing::Combine(
            ::testing::ValuesIn(kUnicodeSupplementaryValueTypes),
            ::testing::ValuesIn(kUnicodeSupplementaryValueTypes))));

// Matching should fail if a PrintableString contains invalid characters.
TEST(VerifyNameMatchInvalidDataTest, FailOnInvalidPrintableStringChars) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "unmangled", &der));
  // Find a known location inside a PrintableString in the DER-encoded data.
  size_t replace_location = der.find("0123456789");
  ASSERT_NE(std::string::npos, replace_location);
  for (int c = 0; c < 256; ++c) {
    SCOPED_TRACE(base::IntToString(c));
    if (base::IsAsciiAlpha(c) || base::IsAsciiDigit(c))
      continue;
    switch (c) {
      case ' ':
      case '\'':
      case '(':
      case ')':
      case '*':
      case '+':
      case ',':
      case '-':
      case '.':
      case '/':
      case ':':
      case '=':
      case '?':
        continue;
    }
    der.replace(replace_location, 1, 1, c);
    // Verification should fail due to the invalid character.
    EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&der),
                                 SequenceValueFromString(&der)));
  }
}

// Matching should fail if an IA5String contains invalid characters.
TEST(VerifyNameMatchInvalidDataTest, FailOnInvalidIA5StringChars) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", "mixed", "rdn_dupetype_sorting_1", &der));
  // Find a known location inside an IA5String in the DER-encoded data.
  size_t replace_location = der.find("eXaMple");
  ASSERT_NE(std::string::npos, replace_location);
  for (int c = 0; c < 256; ++c) {
    SCOPED_TRACE(base::IntToString(c));
    der.replace(replace_location, 1, 1, c);
    bool expected_result = (c <= 127);
    EXPECT_EQ(expected_result, VerifyNameMatch(SequenceValueFromString(&der),
                                               SequenceValueFromString(&der)));
  }
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueExtraData) {
  std::string invalid;
  ASSERT_TRUE(
      LoadTestData("invalid", "AttributeTypeAndValue", "extradata", &invalid));
  // Verification should fail due to extra element in AttributeTypeAndValue
  // sequence.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueShort) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue", "onlyOneElement",
                           &invalid));
  // Verification should fail due to AttributeTypeAndValue sequence having only
  // one element.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueEmpty) {
  std::string invalid;
  ASSERT_TRUE(
      LoadTestData("invalid", "AttributeTypeAndValue", "empty", &invalid));
  // Verification should fail due to empty AttributeTypeAndValue sequence.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnBadAttributeType) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue",
                           "badAttributeType", &invalid));
  // Verification should fail due to Attribute Type not being an OID.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueNotSequence) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue", "setNotSequence",
                           &invalid));
  // Verification should fail due to AttributeTypeAndValue being a Set instead
  // of a Sequence.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnRdnNotSet) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "sequenceInsteadOfSet", &invalid));
  // Verification should fail due to RDN being a Sequence instead of a Set.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnEmptyRdn) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "empty", &invalid));
  // Verification should fail due to RDN having zero AttributeTypeAndValues.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

// Matching should fail if a BMPString contains surrogates.
TEST(VerifyNameMatchInvalidDataTest, FailOnBmpStringSurrogates) {
  std::string normal;
  ASSERT_TRUE(LoadTestData("unicode_bmp", "BMPSTRING", "unmangled", &normal));
  // Find a known location inside a BMPSTRING in the DER-encoded data.
  size_t replace_location = normal.find("\x67\x71\x4e\xac");
  ASSERT_NE(std::string::npos, replace_location);
  // Replace with U+1D400 MATHEMATICAL BOLD CAPITAL A, which requires surrogates
  // to represent.
  std::string invalid =
      normal.replace(replace_location, 4, std::string("\xd8\x35\xdc\x00", 4));
  // Verification should fail due to the invalid codepoints.
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&invalid),
                               SequenceValueFromString(&invalid)));
}

TEST(VerifyNameMatchTest, EmptyNameMatching) {
  std::string empty;
  ASSERT_TRUE(LoadTestData("valid", "Name", "empty", &empty));
  // Empty names are equal.
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&empty),
                              SequenceValueFromString(&empty)));

  // An empty name is not equal to non-empty name.
  std::string non_empty;
  ASSERT_TRUE(
      LoadTestData("ascii", "PRINTABLESTRING", "unmangled", &non_empty));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&empty),
                               SequenceValueFromString(&non_empty)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(&non_empty),
                               SequenceValueFromString(&empty)));
}

// Matching should succeed when the RDNs are sorted differently but are still
// equal after normalizing.
TEST(VerifyNameMatchRDNSorting, Simple) {
  std::string a;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "rdn_sorting_1", &a));
  std::string b;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "rdn_sorting_2", &b));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&a),
                              SequenceValueFromString(&b)));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&b),
                              SequenceValueFromString(&a)));
}

// Matching should succeed when the RDNs are sorted differently but are still
// equal after normalizing, even in malformed RDNs that contain multiple
// elements with the same type.
TEST(VerifyNameMatchRDNSorting, DuplicateTypes) {
  std::string a;
  ASSERT_TRUE(LoadTestData("ascii", "mixed", "rdn_dupetype_sorting_1", &a));
  std::string b;
  ASSERT_TRUE(LoadTestData("ascii", "mixed", "rdn_dupetype_sorting_2", &b));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&a),
                              SequenceValueFromString(&b)));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(&b),
                              SequenceValueFromString(&a)));
}

}  // namespace net
