// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_request_scheduler.h"

#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

namespace {

// Returns the time interval between variations seed fetches.
base::TimeDelta GetFetchPeriod() {
  // The fetch interval can be overridden by a variation param.
  std::string period_min_str =
      variations::GetVariationParamValue("VarationsServiceControl",
                                         "fetch_period_min");
  size_t period_min;
  if (base::StringToSizeT(period_min_str, &period_min))
    return base::TimeDelta::FromMinutes(period_min);

  // The default fetch interval is every 5 hours.
  return base::TimeDelta::FromHours(5);
}

}  // namespace

VariationsRequestScheduler::VariationsRequestScheduler(
    const base::Closure& task) : task_(task) {
}

VariationsRequestScheduler::~VariationsRequestScheduler() {
}

void VariationsRequestScheduler::Start() {
  task_.Run();
  timer_.Start(FROM_HERE, GetFetchPeriod(), task_);
}

void VariationsRequestScheduler::Reset() {
  if (timer_.IsRunning())
    timer_.Reset();
  one_shot_timer_.Stop();
}

void VariationsRequestScheduler::ScheduleFetchShortly() {
  // Reset the regular timer to avoid it triggering soon after.
  Reset();
  // The delay before attempting a fetch shortly, in minutes.
  const int kFetchShortlyDelayMinutes = 5;
  one_shot_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMinutes(kFetchShortlyDelayMinutes),
                        task_);
}

void VariationsRequestScheduler::OnAppEnterForeground() {
  NOTREACHED() << "Attempted to OnAppEnterForeground on non-mobile device";
}

base::Closure VariationsRequestScheduler::task() const {
  return task_;
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// static
VariationsRequestScheduler* VariationsRequestScheduler::Create(
    const base::Closure& task,
    PrefService* local_state) {
  return new VariationsRequestScheduler(task);
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace chrome_variations
