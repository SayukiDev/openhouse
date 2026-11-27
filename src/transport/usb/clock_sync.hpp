// Host <-> device clock offset estimator (original: 0x3cf2c0).
//
// offset ≈ hostTime - deviceTime. Follows decreases (lower latency) 10x faster
// than increases, so it tracks the minimum transport latency.
#pragma once

#include <cmath>
#include <deque>
#include <mutex>
#include <utility>

namespace lh::usb {

class ClockSync {
 public:
  static constexpr double kUsbLatency = 0.0009;    // 0x1a9a20
  static constexpr double kResetError = 0.05;      // 0x1a96e8
  static constexpr double kStaleAfter = 0.5;       // 0x1a9b90
  static constexpr double kGainDown = 0.025;       // 0x1a9640
  static constexpr double kGainUp = 0.0025;        // 0x1a95c8

  // hostT: host read time minus transport latency (0.9 ms wired, 4.4 ms wireless);
  // deviceTime: newest sample stamp.
  // Doorman Dongles: samples come in bursts, one per radio cycle, so most arrive late by part of a
  // cycle. The gains above settled ~2 ms (more with more devices) above the fastest arrival, and
  // poses looked that much fresher than they were; the minimum over the last kWindow is the
  // fastest path itself (the devices' clocks drift ~30 ppm: 0.06 ms over the window).
  static constexpr double kWindow = 2.0;
  void SetMinimumWindow(bool on) {
    std::lock_guard<std::mutex> lock(mutex_);
    minWindow_ = on;
  }

  void Update(double hostT, double deviceTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (minWindow_) {
      const double o = hostT - deviceTime;
      if (hostT - lastUpdate_ > kStaleAfter || (offset_ != 0.0 && std::fabs(o - offset_) > kResetError)) window_.clear();
      while (!window_.empty() && window_.back().second >= o) window_.pop_back();   // ascending: front is the minimum
      window_.emplace_back(hostT, o);
      while (window_.front().first < hostT - kWindow) window_.pop_front();
      offset_ = window_.front().second;
      lastUpdate_ = hostT;
      return;
    }
    const double err = (hostT - deviceTime) - offset_;
    if (offset_ == 0.0 || hostT - lastUpdate_ > kStaleAfter || std::fabs(err) > kResetError) {
      offset_ = hostT - deviceTime;
    } else {
      offset_ += (err < 0.0 ? kGainDown : kGainUp) * err;
    }
    lastUpdate_ = hostT;
  }

  double Offset() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return offset_;
  }

 private:
  mutable std::mutex mutex_;
  double offset_ = 0.0;
  double lastUpdate_ = 0.0;
  bool minWindow_ = false;
  std::deque<std::pair<double, double>> window_;   // (host time, offset), offsets ascending
};

}  // namespace lh::usb
