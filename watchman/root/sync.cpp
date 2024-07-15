/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "watchman/PerfSample.h"
#include "watchman/QueryableView.h"
#include "watchman/root/Root.h"
#include "watchman/telemetry/LogEvent.h"
#include "watchman/telemetry/WatchmanStructuredLogger.h"

using namespace watchman;

folly::SemiFuture<folly::Unit> Root::waitForSettle(
    std::chrono::milliseconds settle_period) {
  return view()->waitForSettle(settle_period);
}

CookieSync::SyncResult Root::syncToNow(std::chrono::milliseconds timeout) {
  PerfSample sample("sync_to_now");
  SyncToNow syncToNow;
  auto root = shared_from_this();
  try {
    auto result = view()->syncToNow(root, timeout);
    auto root_metadata = getRootMetadata();

    if (sample.finish()) {
      sample.add_root_metadata(root_metadata);
      sample.add_meta(
          "sync_to_now",
          json_object(
              {{"success", json_boolean(true)},
               {"timeoutms", json_integer(timeout.count())}}));
      sample.log();
    }

    const auto& [samplingRate, eventCount] =
        getLogEventCounters(LogEventType::SyncToNowType);
    // Log if override set, or if we have hit the sample rate
    if (sample.will_log || eventCount == samplingRate) {
      syncToNow.meta.base.root = root_metadata.root_path.string();
      syncToNow.meta.base.event_count =
          eventCount != samplingRate ? 0 : eventCount;
      syncToNow.meta.recrawl = root_metadata.recrawl_count;
      syncToNow.meta.case_sensitive = root_metadata.case_sensitive;
      syncToNow.meta.watcher = root_metadata.watcher.string();
      syncToNow.timeoutms = timeout.count();
      getLogger()->logEvent(syncToNow);
    }
    return result;
  } catch (const std::exception& exc) {
    auto root_metadata = getRootMetadata();
    sample.force_log();
    sample.finish();
    sample.add_root_metadata(root_metadata);
    sample.add_meta(
        "sync_to_now",
        json_object(
            {{"success", json_boolean(false)},
             {"reason", w_string_to_json(exc.what())},
             {"timeoutms", json_integer(timeout.count())}}));
    sample.log();

    const auto& [samplingRate, eventCount] =
        getLogEventCounters(LogEventType::SyncToNowType);
    syncToNow.meta.base.root = root_metadata.root_path.string();
    syncToNow.meta.base.error = exc.what();
    syncToNow.meta.base.event_count =
        eventCount != samplingRate ? 0 : eventCount;
    syncToNow.meta.recrawl = root_metadata.recrawl_count;
    syncToNow.meta.case_sensitive = root_metadata.case_sensitive;
    syncToNow.meta.watcher = root_metadata.watcher.string();
    syncToNow.success = false;
    syncToNow.timeoutms = timeout.count();
    getLogger()->logEvent(syncToNow);

    throw;
  }
}

/* vim:ts=2:sw=2:et:
 */
