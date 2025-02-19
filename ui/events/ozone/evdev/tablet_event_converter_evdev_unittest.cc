// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"

namespace {

static int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

const char kTestDevicePath[] = "/dev/input/test-device";

const ui::DeviceAbsoluteAxis kWacomIntuos5SPenAbsAxes[] = {
    {ABS_X, {0, 0, 31496, 4, 0, 200}},
    {ABS_Y, {0, 0, 19685, 4, 0, 200}},
    {ABS_Z, {0, -900, 899, 0, 0, 0}},
    {ABS_RZ, {0, -900, 899, 0, 0, 0}},
    {ABS_THROTTLE, {0, -1023, 1023, 0, 0, 0}},
    {ABS_WHEEL, {0, 0, 1023, 0, 0, 0}},
    {ABS_PRESSURE, {0, 0, 2047, 0, 0, 0}},
    {ABS_DISTANCE, {0, 0, 63, 0, 0, 0}},
    {ABS_TILT_X, {0, 0, 127, 0, 0, 0}},
    {ABS_TILT_Y, {0, 0, 127, 0, 0, 0}},
    {ABS_MISC, {0, 0, 0, 0, 0, 0}},
};

const ui::DeviceCapabilities kWacomIntuos5SPen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:14.0/usb1/"
               "1-1/1-1:1.0/input/input19/event5",
    /* name */ "Wacom Intuos5 touch S Pen",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0026",
    /* version */ "0107",
    /* prop */ "1",
    /* ev */ "1f",
    /* key */ "1cdf 1f007f 0 0 0 0",
    /* rel */ "100",
    /* abs */ "1000f000167",
    /* msc */ "1",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kWacomIntuos5SPenAbsAxes,
    arraysize(kWacomIntuos5SPenAbsAxes),
};

}  // namespace

namespace ui {

class MockTabletEventConverterEvdev : public TabletEventConverterEvdev {
 public:
  MockTabletEventConverterEvdev(int fd,
                                base::FilePath path,
                                CursorDelegateEvdev* cursor,
                                const EventDeviceInfo& devinfo,
                                DeviceEventDispatcherEvdev* dispatcher);
  ~MockTabletEventConverterEvdev() override {};

  void ConfigureReadMock(struct input_event* queue,
                         long read_this_many,
                         long queue_index);

  // Actually dispatch the event reader code.
  void ReadNow() {
    OnFileCanReadWithoutBlocking(read_pipe_);
    base::RunLoop().RunUntilIdle();
  }

 private:
  int read_pipe_;
  int write_pipe_;

  ScopedVector<Event> dispatched_events_;

  DISALLOW_COPY_AND_ASSIGN(MockTabletEventConverterEvdev);
};

class MockTabletCursorEvdev : public CursorDelegateEvdev {
 public:
  MockTabletCursorEvdev() { cursor_confined_bounds_ = gfx::Rect(1024, 768); }
  ~MockTabletCursorEvdev() override {}

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location) override {
    NOTREACHED();
  }
  void MoveCursorTo(const gfx::PointF& location) override {
    cursor_location_ = location;
  }
  void MoveCursor(const gfx::Vector2dF& delta) override { NOTREACHED(); }
  bool IsCursorVisible() override { return 1; }
  gfx::PointF GetLocation() override { return cursor_location_; }
  gfx::Rect GetCursorConfinedBounds() override {
    return cursor_confined_bounds_;
  }

 private:
  gfx::PointF cursor_location_;
  gfx::Rect cursor_confined_bounds_;
  DISALLOW_COPY_AND_ASSIGN(MockTabletCursorEvdev);
};

MockTabletEventConverterEvdev::MockTabletEventConverterEvdev(
    int fd,
    base::FilePath path,
    CursorDelegateEvdev* cursor,
    const EventDeviceInfo& devinfo,
    DeviceEventDispatcherEvdev* dispatcher)
    : TabletEventConverterEvdev(fd,
                                path,
                                1,
                                cursor,
                                devinfo,
                                dispatcher) {
  int fds[2];

  if (pipe(fds))
    PLOG(FATAL) << "failed pipe";

  EXPECT_FALSE(SetNonBlocking(fds[0]) || SetNonBlocking(fds[1]))
    << "failed to set non-blocking: " << strerror(errno);

  read_pipe_ = fds[0];
  write_pipe_ = fds[1];
}

void MockTabletEventConverterEvdev::ConfigureReadMock(struct input_event* queue,
                                                      long read_this_many,
                                                      long queue_index) {
  int nwrite = HANDLE_EINTR(write(write_pipe_, queue + queue_index,
                                  sizeof(struct input_event) * read_this_many));
  DCHECK(nwrite ==
         static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed, errno: " << errno;
}

}  // namespace ui

// Test fixture.
class TabletEventConverterEvdevTest : public testing::Test {
 public:
  TabletEventConverterEvdevTest() {}

  // Overridden from testing::Test:
  void SetUp() override {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    events_in_ = evdev_io[0];
    events_out_ = evdev_io[1];

    cursor_.reset(new ui::MockTabletCursorEvdev());
    device_manager_ = ui::CreateDeviceManagerForTest();
    event_factory_ = ui::CreateEventFactoryEvdevForTest(
        cursor_.get(), device_manager_.get(),
        ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
        base::Bind(&TabletEventConverterEvdevTest::DispatchEventForTest,
                   base::Unretained(this)));
    dispatcher_ =
        ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get());
  }

  void TearDown() override {
    cursor_.reset();
  }

  ui::MockTabletEventConverterEvdev* CreateDevice(
      const ui::DeviceCapabilities& caps) {
    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    return new ui::MockTabletEventConverterEvdev(
        events_in_, base::FilePath(kTestDevicePath), cursor_.get(), devinfo,
        dispatcher_.get());
  }

  ui::CursorDelegateEvdev* cursor() { return cursor_.get(); }

  unsigned size() { return dispatched_events_.size(); }
  ui::MouseEvent* dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index];
    DCHECK(ev->IsMouseEvent());
    return static_cast<ui::MouseEvent*>(ev);
  }

  void DispatchEventForTest(ui::Event* event) {
    scoped_ptr<ui::Event> cloned_event = ui::Event::Clone(*event);
    dispatched_events_.push_back(cloned_event.Pass());
  }

 private:
  scoped_ptr<ui::MockTabletCursorEvdev> cursor_;
  scoped_ptr<ui::DeviceManager> device_manager_;
  scoped_ptr<ui::EventFactoryEvdev> event_factory_;
  scoped_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;

  ScopedVector<ui::Event> dispatched_events_;

  int events_out_;
  int events_in_;

  DISALLOW_COPY_AND_ASSIGN(TabletEventConverterEvdevTest);
};

#define EPSILON 20

// Uses real data captured from Wacom Intuos 5 Pen
TEST_F(TabletEventConverterEvdevTest, MoveTopLeft) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 477},
      {{0, 0}, EV_ABS, ABS_TILT_X, 66},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 62},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveTopRight) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 31496},
      {{0, 0}, EV_ABS, ABS_Y, 109},
      {{0, 0}, EV_ABS, ABS_TILT_X, 66},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 61},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorConfinedBounds().width() - EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomLeft) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_Y, 19685},
      {{0, 0}, EV_ABS, ABS_TILT_X, 64},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 61},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomRight) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 31496},
      {{0, 0}, EV_ABS, ABS_Y, 19685},
      {{0, 0}, EV_ABS, ABS_TILT_X, 67},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 63},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, Tap) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 15456},
      {{0, 0}, EV_ABS, ABS_Y, 8605},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 49},
      {{0, 0}, EV_ABS, ABS_TILT_X, 68},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 64},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 15725},
      {{0, 0}, EV_ABS, ABS_Y, 8755},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 29},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 992},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 15922},
      {{0, 0}, EV_ABS, ABS_Y, 8701},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 32},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(3u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());
  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());
}

TEST_F(TabletEventConverterEvdevTest, StylusButtonPress) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 18372},
      {{0, 0}, EV_ABS, ABS_Y, 9880},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 61},
      {{0, 0}, EV_ABS, ABS_TILT_X, 60},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 63},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 18294},
      {{0, 0}, EV_ABS, ABS_Y, 9723},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 20},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1015},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 18516},
      {{0, 0}, EV_ABS, ABS_Y, 9723},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 23},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(3u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
}

// Should only get an event if BTN_TOOL received
TEST_F(TabletEventConverterEvdevTest, CheckStylusFiltering) {
  scoped_ptr<ui::MockTabletEventConverterEvdev> dev =
      make_scoped_ptr(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(0u, size());
}
