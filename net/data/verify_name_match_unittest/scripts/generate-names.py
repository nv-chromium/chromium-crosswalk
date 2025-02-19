#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import copy
import os
import subprocess
import tempfile


class RDN:
  def __init__(self):
    self.attrs = []

  def add_attr(self, attr_type, attr_value_type, attr_value,
               attr_modifier=None):
    self.attrs.append((attr_type, attr_value_type, attr_value, attr_modifier))
    return self

  def __str__(self):
    s = ''
    for n, attr in enumerate(self.attrs):
      s += 'attrTypeAndValue%i=SEQUENCE:attrTypeAndValueSequence%i_%i\n' % (
          n, id(self), n)

    s += '\n'
    for n, attr in enumerate(self.attrs):
      attr_type, attr_value_type, attr_value, attr_modifier = attr
      s += '[attrTypeAndValueSequence%i_%i]\n' % (id(self), n)
      # Note the quotes around the string value here, which is necessary for
      # trailing whitespace to be included by openssl.
      s += 'type=OID:%s\n' % attr_type
      s += 'value='
      if attr_modifier:
        s += attr_modifier + ','
      s += '%s:"%s"\n' % (attr_value_type, attr_value)

    return s


class NameGenerator:
  def __init__(self):
    self.rdns = []

  def add_rdn(self):
    rdn = RDN()
    self.rdns.append(rdn)
    return rdn

  def __str__(self):
    s = 'asn1 = SEQUENCE:rdnSequence\n\n[rdnSequence]\n'
    for n, rdn in enumerate(self.rdns):
      s += 'rdn%i = SET:rdnSet%i\n' % (n, n)

    s += '\n'

    for n, rdn in enumerate(self.rdns):
      s += '[rdnSet%i]\n%s\n' % (n, rdn)

    return s


def generate(s, fn):
  out_fn = os.path.join('..', 'names', fn + '.pem')
  conf_tempfile = tempfile.NamedTemporaryFile()
  conf_tempfile.write(str(s))
  conf_tempfile.flush()
  der_tmpfile = tempfile.NamedTemporaryFile()
  description_tmpfile = tempfile.NamedTemporaryFile()
  subprocess.check_call(['openssl', 'asn1parse', '-genconf', conf_tempfile.name,
                         '-i', '-out', der_tmpfile.name],
                        stdout=description_tmpfile)
  conf_tempfile.close()

  output_file = open(out_fn, 'w')
  description_tmpfile.seek(0)
  output_file.write(description_tmpfile.read())
  output_file.write('-----BEGIN NAME-----\n')
  output_file.write(base64.encodestring(der_tmpfile.read()))
  output_file.write('-----END NAME-----\n')
  output_file.close()


def unmangled(s):
  return s


def extra_whitespace(s):
  return '  ' + s.replace(' ', '   ') + '  '


def case_swap(s):
  return s.swapcase()


def main():
  for valuetype in ('PRINTABLESTRING', 'T61STRING', 'UTF8', 'BMPSTRING',
                    'UNIVERSALSTRING'):
    for string_mangler in (unmangled, extra_whitespace, case_swap):
      n=NameGenerator()
      n.add_rdn().add_attr('countryName', 'PRINTABLESTRING', 'US')
      n.add_rdn().add_attr('stateOrProvinceName',
                           valuetype,
                           string_mangler('New York'))
      n.add_rdn().add_attr('localityName',
                           valuetype,
                           string_mangler("ABCDEFGHIJKLMNOPQRSTUVWXYZ "
                                          "abcdefghijklmnopqrstuvwxyz "
                                          "0123456789 '()+,-./:=?"))

      n_extra_attr = copy.deepcopy(n)
      n_extra_attr.rdns[-1].add_attr('organizationName',
                                     valuetype,
                                     string_mangler('Name of company'))

      n_extra_rdn = copy.deepcopy(n)
      n_extra_rdn.add_rdn().add_attr('organizationName',
                                     valuetype,
                                     string_mangler('Name of company'))

      filename_base = 'ascii-' + valuetype + '-' + string_mangler.__name__

      generate(n, filename_base)
      generate(n_extra_attr, filename_base + '-extra_attr')
      generate(n_extra_rdn, filename_base + '-extra_rdn')

  for valuetype in ('UTF8', 'BMPSTRING', 'UNIVERSALSTRING'):
    n=NameGenerator()
    n.add_rdn().add_attr('countryName', 'PRINTABLESTRING', 'JP')
    n.add_rdn().add_attr('localityName',
                         valuetype,
                         "\xe6\x9d\xb1\xe4\xba\xac",
                         "FORMAT:UTF8")

    filename_base = 'unicode_bmp-' + valuetype + '-' + 'unmangled'
    generate(n, filename_base)

  for valuetype in ('UTF8', 'UNIVERSALSTRING'):
    n=NameGenerator()
    n.add_rdn().add_attr('countryName', 'PRINTABLESTRING', 'JP')
    n.add_rdn().add_attr('localityName',
                         valuetype,
                         "\xf0\x9d\x90\x80\xf0\x9d\x90\x99",
                         "FORMAT:UTF8")

    filename_base = 'unicode_supplementary-' + valuetype + '-' + 'unmangled'
    generate(n, filename_base)

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=OID:countryName
value=PRINTABLESTRING:"US"
extra=PRINTABLESTRING:"hello world"
""", "invalid-AttributeTypeAndValue-extradata")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=OID:countryName
""", "invalid-AttributeTypeAndValue-onlyOneElement")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
""", "invalid-AttributeTypeAndValue-empty")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=PRINTABLESTRING:"hello world"
value=PRINTABLESTRING:"US"
""", "invalid-AttributeTypeAndValue-badAttributeType")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SET:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=OID:countryName
value=PRINTABLESTRING:"US"
""", "invalid-AttributeTypeAndValue-setNotSequence")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SEQUENCE:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=OID:countryName
value=PRINTABLESTRING:"US"
""", "invalid-RDN-sequenceInsteadOfSet")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
""", "invalid-RDN-empty")

  generate("""asn1 = SET:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=OID:countryName
value=PRINTABLESTRING:"US"
""", "invalid-Name-setInsteadOfSequence")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
""", "valid-Name-empty")

  # Certs with a RDN that is sorted differently due to length of the values, but
  # which should compare equal when normalized.
  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
attrTypeAndValue1=SEQUENCE:attrTypeAndValueSequence0_1
[attrTypeAndValueSequence0_0]
type=OID:stateOrProvinceName
value=PRINTABLESTRING:"    state"
[attrTypeAndValueSequence0_1]
type=OID:localityName
value=PRINTABLESTRING:"locality"
""", "ascii-PRINTABLESTRING-rdn_sorting_1")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
attrTypeAndValue1=SEQUENCE:attrTypeAndValueSequence0_1
[attrTypeAndValueSequence0_0]
type=OID:stateOrProvinceName
value=PRINTABLESTRING:"state"
[attrTypeAndValueSequence0_1]
type=OID:localityName
value=PRINTABLESTRING:" locality"
""", "ascii-PRINTABLESTRING-rdn_sorting_2")

  # Certs with a RDN that is sorted differently due to length of the values, and
  # also contains multiple values with the same type.
  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
attrTypeAndValue1=SEQUENCE:attrTypeAndValueSequence0_1
attrTypeAndValue2=SEQUENCE:attrTypeAndValueSequence0_2
attrTypeAndValue3=SEQUENCE:attrTypeAndValueSequence0_3
attrTypeAndValue4=SEQUENCE:attrTypeAndValueSequence0_4
[attrTypeAndValueSequence0_0]
type=OID:domainComponent
value=IA5STRING:"     cOm"
[attrTypeAndValueSequence0_1]
type=OID:domainComponent
value=IA5STRING:"eXaMple"
[attrTypeAndValueSequence0_2]
type=OID:domainComponent
value=IA5STRING:"wWw"
[attrTypeAndValueSequence0_3]
type=OID:localityName
value=PRINTABLESTRING:"NEw"
[attrTypeAndValueSequence0_4]
type=OID:localityName
value=PRINTABLESTRING:"   yORk    "
""", "ascii-mixed-rdn_dupetype_sorting_1")

  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
attrTypeAndValue1=SEQUENCE:attrTypeAndValueSequence0_1
attrTypeAndValue2=SEQUENCE:attrTypeAndValueSequence0_2
attrTypeAndValue3=SEQUENCE:attrTypeAndValueSequence0_3
attrTypeAndValue4=SEQUENCE:attrTypeAndValueSequence0_4
[attrTypeAndValueSequence0_0]
type=OID:domainComponent
value=IA5STRING:"cOM"
[attrTypeAndValueSequence0_1]
type=OID:domainComponent
value=IA5STRING:"eXampLE"
[attrTypeAndValueSequence0_2]
type=OID:domainComponent
value=IA5STRING:"    Www  "
[attrTypeAndValueSequence0_3]
type=OID:localityName
value=PRINTABLESTRING:"   nEw            "
[attrTypeAndValueSequence0_4]
type=OID:localityName
value=PRINTABLESTRING:"yoRK"
""", "ascii-mixed-rdn_dupetype_sorting_2")

  # Minimal valid config. Copy and modify this one when generating new invalid
  # configs.
  generate("""asn1 = SEQUENCE:rdnSequence
[rdnSequence]
rdn0 = SET:rdnSet0
[rdnSet0]
attrTypeAndValue0=SEQUENCE:attrTypeAndValueSequence0_0
[attrTypeAndValueSequence0_0]
type=OID:countryName
value=PRINTABLESTRING:"US"
""", "valid-minimal")

if __name__ == '__main__':
  main()
