// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANIFEST_MANIFEST_ICON_DOWNLOADER_H_
#define CHROME_BROWSER_MANIFEST_MANIFEST_ICON_DOWNLOADER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

class GURL;

// Helper class which downloads the icon located at a specified. If the icon
// file contains multiple icons then it attempts to pick the one closest in size
// bigger than or equal to ideal_icon_size_in_dp, taking into account the
// density of the device. If a bigger icon is chosen then, the icon is scaled
// down to be equal to ideal_icon_size_in_dp.
class ManifestIconDownloader final {
 public:
  using IconFetchCallback = base::Callback<void(const SkBitmap&)>;

  ManifestIconDownloader() = delete;
  ~ManifestIconDownloader() = delete;

  // Returns whether the download has started.
  // It will return false if the current context or information do not allow to
  // download the image.
  static bool Download(content::WebContents* web_contents,
                       const GURL& icon_url,
                       int ideal_icon_size_in_dp,
                       const IconFetchCallback& callback);

 private:
  // Callback run after the manifest icon downloaded successfully or the
  // download failed.
  static void OnIconFetched(int ideal_icon_size_in_px,
                            int minimum_icon_size_in_px,
                            const IconFetchCallback& callback,
                            int id,
                            int http_status_code,
                            const GURL& url,
                            const std::vector<SkBitmap>& bitmaps,
                            const std::vector<gfx::Size>& sizes);

  static void ScaleIcon(int ideal_icon_size_in_px,
                        const SkBitmap& bitmap,
                        const IconFetchCallback& callback);

  static int FindClosestBitmapIndex(int ideal_icon_size_in_px,
                                    int minimum_icon_size_in_px,
                                    const std::vector<SkBitmap>& bitmaps);

  friend class ManifestIconDownloaderTest;

  DISALLOW_COPY_AND_ASSIGN(ManifestIconDownloader);
};

#endif  // CHROME_BROWSER_MANIFEST_MANIFEST_ICON_DOWNLOADER_H_
