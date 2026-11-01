#pragma once

#include <array>
#include <functional>

#include "esphome/components/dongle_core/dongle_core.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::learn_wizard {

// Хранит последние принятые кадры. Разбор и шаги находятся в профиле на странице,
// поэтому компонент не зависит от типа устройства и ничего ему не отправляет.
class LearnWizard : public Component, public AsyncWebHandler {
 public:
  explicit LearnWizard(dongle_core::DongleCore *core) : core_(core) {}

  void setup() override;
  void loop() override;

  void set_page(const uint8_t *page, size_t size) {
    this->page_ = page;
    this->page_size_ = size;
  }
  void set_periods(uint32_t active, uint32_t idle) {
    this->active_period_ = active;
    this->idle_period_ = idle;
  }
  void set_period_callback(std::function<void(uint32_t)> &&callback) { this->period_callback_ = std::move(callback); }

  // Вызывается из основного цикла драйвером.
  void feed(const uint8_t *data, size_t size);

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  static constexpr size_t RING_SIZE = 48;
  static constexpr size_t FRAME_SIZE = 36;
  static constexpr size_t MAX_RESPONSE_FRAMES = 12;
  static constexpr uint32_t CLIENT_TIMEOUT_MS = 30000;

  struct Frame {
    uint32_t seq;
    uint32_t ms;
    uint8_t size;
    uint8_t data[FRAME_SIZE];
  };

  void send_frames_(AsyncWebServerRequest *request);

  dongle_core::DongleCore *core_;
  const uint8_t *page_{nullptr};
  size_t page_size_{0};
  uint32_t active_period_{2000};
  uint32_t idle_period_{7000};
  std::function<void(uint32_t)> period_callback_{nullptr};

  Mutex lock_;
  std::array<Frame, RING_SIZE> ring_{};
  uint32_t next_seq_{1};
  std::atomic<bool> session_{false};
  std::atomic<bool> period_active_{false};
  std::atomic<uint32_t> last_client_{0};
};

}  // namespace esphome::learn_wizard
