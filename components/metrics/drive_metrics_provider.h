// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/proto/system_profile.pb.h"

namespace base {
class FilePath;
}

namespace metrics {

// Provides metrics about the local drives on a user's computer. Currently only
// checks to see if they incur a seek-time penalty (e.g. if they're SSDs).
//
// Defers gathering metrics until after "rush hour" (startup) so as to not bog
// down the file thread.
class DriveMetricsProvider : public metrics::MetricsProvider {
 public:
  DriveMetricsProvider(scoped_refptr<base::SequencedTaskRunner> file_thread,
                       int local_state_path_key);
  ~DriveMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

  // Called to start gathering metrics.
  void GetDriveMetrics(const base::Closure& done);

 private:
  FRIEND_TEST_ALL_PREFIXES(DriveMetricsProviderTest, HasSeekPenalty);

  // A response to querying a drive as to whether it incurs a seek penalty.
  // |has_seek_penalty| is set if |success| is true.
  struct SeekPenaltyResponse {
    SeekPenaltyResponse();
    bool success;
    bool has_seek_penalty;
  };

  struct DriveMetrics {
    SeekPenaltyResponse app_drive;
    SeekPenaltyResponse user_data_drive;
  };

  // Determine whether the device that services |path| has a seek penalty.
  // Returns false if it couldn't be determined (e.g., |path| doesn't exist).
  static bool HasSeekPenalty(const base::FilePath& path,
                             bool* has_seek_penalty);

  // Gather metrics about various drives on |file_thread_|.
  static DriveMetrics GetDriveMetricsOnFileThread(int local_state_path_key);

  // Tries to determine whether there is a penalty for seeking on the drive that
  // hosts |path_service_key| (for example: the drive that holds "Local State").
  static void QuerySeekPenalty(int path_service_key,
                               SeekPenaltyResponse* response);

  // Called when metrics are done being gathered from the FILE thread.
  void GotDriveMetrics(const DriveMetrics& metrics);

  // Fills |drive| with information from successful |response|s.
  void FillDriveMetrics(const SeekPenaltyResponse& response,
                        metrics::SystemProfileProto::Hardware::Drive* drive);

  // The thread on which file operations are performed (supplied by the
  // embedder).
  scoped_refptr<base::SequencedTaskRunner> file_thread_;

  // The key to give to base::PathService to obtain the path to local state
  // (supplied by the embedder).
  int local_state_path_key_;

  // Information gathered about various important drives.
  DriveMetrics metrics_;

  // Called when metrics are done being collected.
  base::Closure got_metrics_callback_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<DriveMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DRIVE_METRICS_PROVIDER_H_
