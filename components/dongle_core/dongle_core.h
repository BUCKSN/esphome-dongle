#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "esphome/components/ota/ota_backend_factory.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::dongle_core {

// Веб-интерфейс доступен только пока открыто окно: кнопкой или переключателем в Home Assistant.
// Запросы приходят из задачи HTTP-сервера, поэтому общие поля атомарные.
class DongleCore : public Component, public AsyncWebHandler {
 public:
  explicit DongleCore(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  // Раньше Wi-Fi и страницы настройки сети: наш обработчик должен стоять в очереди первым.
  float get_setup_priority() const override { return setup_priority::WIFI + 2.0f; }

  void set_window_timeout(uint32_t ms) { this->window_timeout_ = ms; }
  void set_marker(const char *marker);
  void set_project_url(const char *url) { this->project_url_ = url; }
  void set_link_callback(std::function<bool()> &&callback) { this->link_callback_ = std::move(callback); }

  void open_window();
  void close_window();
  void toggle_window();
  bool is_window_open() const { return this->open_.load(); }

  bool leds_enabled() const { return this->leds_enabled_.load(); }
  void set_leds_enabled(bool enabled);

  void touch() { this->touch_(); }
  // Дополнительные разделы под /dongle: доступны, только пока открыто окно.
  void add_handler(AsyncWebHandler *handler) { this->extra_handlers_.push_back(handler); }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;
  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  void touch_() { this->last_activity_.store(millis()); }
  bool setup_network_active_() const;
  void wifi_change_loop_();
  std::string networks_json_();
  void ota_fail_(const char *reason);
  void ensure_api_key_();
  std::string state_json_();
  void ota_scan_(const uint8_t *data, size_t len);

  web_server_base::WebServerBase *base_;
  std::vector<AsyncWebHandler *> extra_handlers_;
  ESPPreferenceObject leds_pref_;
  uint32_t window_timeout_{3600000};
  std::atomic<bool> open_{false};
  std::atomic<bool> leds_enabled_{true};
  std::atomic<bool> leds_dirty_{false};
  std::atomic<uint32_t> last_activity_{0};

  const char *marker_{""};
  const char *project_url_{""};
  std::function<bool()> link_callback_{nullptr};
  std::atomic<bool> new_key_requested_{false};
  bool key_checked_{false};
  bool server_started_{false};

  // Смена сети: старая сеть хранится во flash, пока новая не подтвердится.
  struct WifiRollback {
    char ssid[33];
    char password[65];
    bool armed;
  };
  ESPPreferenceObject rollback_pref_;
  WifiRollback rollback_{};
  std::string new_ssid_;
  std::string new_password_;
  std::atomic<bool> wifi_change_requested_{false};
  std::atomic<bool> scan_requested_{false};
  uint32_t last_scan_{0};
  std::string expected_;  // метка до версии: совместимая прошивка содержит ту же
  ota::OTABackendPtr ota_backend_{nullptr};
  const char *ota_error_{nullptr};
  size_t ota_match_{0};
  bool ota_marker_found_{false};
  std::atomic<bool> reboot_pending_{false};
  uint32_t reboot_at_{0};
};

}  // namespace esphome::dongle_core
