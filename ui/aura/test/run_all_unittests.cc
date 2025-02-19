// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_surface_test_support.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  gfx::GLSurfaceTestSupport::InitializeOneOff();

  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
