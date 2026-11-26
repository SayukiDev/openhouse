// Unit tests for per-device optical-only tracking (IMU disabled).
#include <gtest/gtest.h>

#include <cmath>

#include "tracking/direct_filter.hpp"
#include "tracking/lighthouse_direct.hpp"
#include "tracking/lighthouse_model.hpp"

namespace {

lh::TrackedObjectModel Object() {
  lh::TrackedObjectModel model;
  model.points = {{{-0.08, -0.07, -0.02}}, {{0.08, -0.07, 0.02}},
                  {{-0.08, 0.07, 0.03}}, {{0.08, 0.07, -0.03}},
                  {{0, 0, 0.08}}, {{0.04, 0.01, -0.06}}};
  for (size_t i = 0; i < model.points.size(); ++i) {
    model.normals.push_back({0, 0, 1});
    model.channelToIndex[int(i)] = int(i);
  }
  return model;
}

lh::OpticalFrame FrameAt(const lh::TrackedObjectModel& m, int step, double tx, double ty, double tz,
                         uint32_t serial = 123) {
  lh::OpticalFrame f;
  f.baseSerial = serial;
  f.axis = step % 2;
  f.syncTime = 1.0 + step / 120.0;
  f.nextSyncTime = f.syncTime + 1.0 / 120.0;
  f.idealPeriod = f.averagePeriod = 1.0 / 60.0;
  f.leftSyncIsReal = f.rightSyncIsReal = true;
  const double cal[7]{};
  for (size_t i = 0; i < m.points.size(); ++i) {
    const auto& p = m.points[i];
    lh::OpticalSample s;
    s.sensor = int(i);
    s.axis = f.axis;
    s.width = 5e-6;
    s.time = f.syncTime + 0.004 + i * 1e-5;
    s.angle = lh::model::Lh1Angle(s.axis, p[0] + tx, p[1] + ty, p[2] + tz, cal);
    f.samples.push_back(s);
  }
  return f;
}

TEST(DisableImu, TracksWithDeadGyroAndCorruptedAccel) {
  const auto model = Object();
  lh::LighthouseDirect tracker(model);
  tracker.SetImprovements(true, 4.0, true);
  tracker.SetImuDisabled(true);
  ASSERT_TRUE(tracker.ImuDisabled());
  EXPECT_TRUE(tracker.Filter().imuDisabled);
  EXPECT_TRUE(tracker.Filter().holdImuCalibration);

  lh::BaseConfig base;
  base.valid = true;
  base.serial = 123;
  base.cal.resize(14);
  lh::BaseDynamicState dynamic;
  dynamic.accelDir = {0, 1, 0};
  tracker.OnBaseConfig(base, dynamic);

  double imuTime = 1.0;
  for (int step = 0; step < 200; ++step) {
    const auto f = FrameAt(model, step, 0.0, 0.0, -2.0);
    while (imuTime < f.nextSyncTime) {
      lh::ImuSample imu;
      imu.time = imuTime;
      imu.gyro = {0.0, 0.0, 0.0};
      imu.accel = {-8.24, -13.19, -2.45};
      tracker.OnImu(imu);
      imuTime += 0.001;
    }
    tracker.OnFrame(f);
  }

  EXPECT_TRUE(tracker.PendingImu().empty());
  const auto pose = tracker.Pose();
  ASSERT_TRUE(pose);
  EXPECT_EQ(pose->status, lh::TrackingStatus::Optical);
  EXPECT_GT(pose->opticalUpdates, 150u);
  EXPECT_NEAR(pose->pose.position[0], 0.0, 1e-4);
  EXPECT_NEAR(pose->pose.position[1], 0.0, 1e-4);
  EXPECT_NEAR(pose->pose.position[2], -2.0, 1e-4);
  EXPECT_EQ(tracker.HealthCheck(imuTime), "");

  const auto predicted = tracker.PredictNow();
  EXPECT_DOUBLE_EQ(predicted.a[0], 0.0);
  EXPECT_DOUBLE_EQ(predicted.a[1], 0.0);
  EXPECT_DOUBLE_EQ(predicted.a[2], 0.0);
  EXPECT_NEAR(predicted.p[0], 0.0, 1e-4);
  EXPECT_NEAR(predicted.p[1], 0.0, 1e-4);
  EXPECT_NEAR(predicted.p[2], -2.0, 1e-4);

  const auto& filter = tracker.Filter();
  EXPECT_TRUE(filter.CalibrationHealthy());
  EXPECT_TRUE(filter.P.allFinite());
  EXPECT_DOUBLE_EQ(filter.x[18], 9.80665);
  EXPECT_DOUBLE_EQ(filter.x[28], 0.0);
  EXPECT_DOUBLE_EQ(filter.x[29], 0.0);
  EXPECT_DOUBLE_EQ(filter.x[30], 0.0);
}

TEST(DisableImu, TracksMovingObjectWithoutImu) {
  const auto model = Object();
  lh::LighthouseDirect tracker(model);
  tracker.SetImprovements(true, 4.0, true);
  tracker.SetImuDisabled(true);

  lh::BaseConfig base;
  base.valid = true;
  base.serial = 123;
  base.cal.resize(14);
  lh::BaseDynamicState dynamic;
  dynamic.accelDir = {0, 1, 0};
  tracker.OnBaseConfig(base, dynamic);

  double imuTime = 1.0;
  double lastX = 0.0;
  for (int step = 0; step < 240; ++step) {
    const double t = step / 120.0;
    const double tx = step < 30 ? 0.0 : 0.15 * std::sin(2.0 * 3.141592653589793 * 0.5 * (t - 0.25));
    lastX = tx;
    const auto f = FrameAt(model, step, tx, 0.0, -2.0);
    while (imuTime < f.nextSyncTime) {
      lh::ImuSample imu;
      imu.time = imuTime;
      imu.gyro = {0.0, 0.0, 0.0};
      imu.accel = {-8.24, -13.19, -2.45};
      tracker.OnImu(imu);
      imuTime += 0.001;
    }
    tracker.OnFrame(f);
  }

  const auto pose = tracker.Pose();
  ASSERT_TRUE(pose);
  EXPECT_EQ(pose->status, lh::TrackingStatus::Optical);
  EXPECT_NEAR(pose->pose.position[0], lastX, 0.01);
  EXPECT_NEAR(pose->pose.position[1], 0.0, 0.01);
  EXPECT_NEAR(pose->pose.position[2], -2.0, 0.01);
  EXPECT_EQ(tracker.HealthCheck(imuTime), "");
}

TEST(DisableImu, DefaultEnabledModeUnaffected) {
  const auto model = Object();
  lh::LighthouseDirect tracker(model);
  EXPECT_FALSE(tracker.ImuDisabled());
  EXPECT_FALSE(tracker.Filter().imuDisabled);
  EXPECT_FALSE(tracker.Filter().holdImuCalibration);

  lh::BaseConfig base;
  base.valid = true;
  base.serial = 123;
  base.cal.resize(14);
  lh::BaseDynamicState dynamic;
  dynamic.accelDir = {0, 1, 0};
  tracker.OnBaseConfig(base, dynamic);

  double imuTime = 1.0;
  for (int step = 0; step < 120; ++step) {
    const auto f = FrameAt(model, step, 0.0, 0.0, -2.0);
    while (imuTime < f.nextSyncTime) {
      lh::ImuSample imu;
      imu.time = imuTime;
      imu.accel = {0.0, 9.80665, 0.0};
      tracker.OnImu(imu);
      imuTime += 0.001;
    }
    tracker.OnFrame(f);
  }

  const auto pose = tracker.Pose();
  ASSERT_TRUE(pose);
  EXPECT_EQ(pose->status, lh::TrackingStatus::Optical);
  EXPECT_NEAR(pose->pose.position[0], 0.0, 1e-5);
  EXPECT_NEAR(pose->pose.position[1], 0.0, 1e-5);
  EXPECT_NEAR(pose->pose.position[2], -2.0, 1e-5);
  EXPECT_EQ(tracker.HealthCheck(imuTime), "");

  // DirectFilter rejection checks: active by default, bypassed for IMU/gravity when disabled.
  lh::direct::DirectFilter f;
  f.updateCount = 300;
  lh::direct::Vec dx = lh::direct::Vec::Zero(49);
  dx[28] = 0.5;   // large IMU rotation
  dx[18] = 2.0;   // large gravity magnitude
  dx[16] = 0.5;   // large gravity tilt
  EXPECT_TRUE(f.RejectCorrection(dx));
  f.imuDisabled = true;
  EXPECT_FALSE(f.RejectCorrection(dx));
  // NaN and angular rate limits remain active when IMU is disabled.
  f.omegaCorrectionLimit2 = 0.01;
  dx[13] = 0.2;
  EXPECT_TRUE(f.RejectCorrection(dx));
  EXPECT_EQ(f.lastRejectReason, lh::direct::kRejectOmega);
}

}  // namespace
