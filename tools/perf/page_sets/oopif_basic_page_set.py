# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page
from telemetry import story

class OopifBasicPageSet(story.StorySet):
  """ Basic set of pages used to measure performance of out-of-process
  iframes.
  """

  def __init__(self):
    super(OopifBasicPageSet, self).__init__(
        archive_data_file='data/oopif_basic.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    urls = [
        'http://www.cnn.com',
        'http://www.ebay.com',
        'http://booking.com',
        'http://www.rei.com/',
        'http://www.fifa.com/',
        # Disabled because it is flaky on Windows and Android
        #'http://arstechnica.com/',
        'http://www.nationalgeographic.com/',
        # Cross-site heavy! Enable them once they render without crashing.
        #'http://www.nba.com/',
        #'http://www.phonearena.com/',
        #'http://slickdeals.net/',
        #'http://www.163.com/',
    ]

    for url in urls:
      self.AddStory(page.Page(url, self))
