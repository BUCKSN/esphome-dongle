#include "learn_wizard.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::learn_wizard {

static const char *const TAG = "learn_wizard";

void LearnWizard::setup() { this->core_->add_handler(this); }

void LearnWizard::loop() {
  bool wanted = this->session_.load();
  if (wanted && (!this->core_->is_window_open() || millis() - this->last_client_.load() > CLIENT_TIMEOUT_MS)) {
    this->session_.store(false);
    wanted = false;
  }
  if (wanted != this->period_active_.load()) {
    this->period_active_.store(wanted);
    if (this->period_callback_)
      this->period_callback_(wanted ? this->active_period_ : this->idle_period_);
    ESP_LOGI(TAG, "Мастер %s", wanted ? "начал работу" : "закончил работу");
  }
}

void LearnWizard::feed(const uint8_t *data, size_t size) {
  if (!this->session_.load() || size == 0)
    return;
  LockGuard guard(this->lock_);
  uint32_t seq = this->next_seq_++;
  Frame &frame = this->ring_[seq % RING_SIZE];
  frame.seq = seq;
  frame.ms = millis();
  frame.size = size > FRAME_SIZE ? FRAME_SIZE : size;
  std::memcpy(frame.data, data, frame.size);
}

bool LearnWizard::canHandle(AsyncWebServerRequest *request) const {
  char buffer[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = request->url_to(buffer);
  return url.size() >= 13 && std::strncmp(url.c_str(), "/dongle/learn", 13) == 0;
}

void LearnWizard::send_frames_(AsyncWebServerRequest *request) {
  uint32_t since = std::strtoul(request->arg("since").c_str(), nullptr, 10);
  std::string out;
  out.reserve(160 + MAX_RESPONSE_FRAMES * (FRAME_SIZE * 2 + 28));
  char number[80];
  {
    LockGuard guard(this->lock_);
    uint32_t last = this->next_seq_ - 1;
    // После перезагрузки донгла нумерация начинается заново.
    if (since > last)
      since = 0;
    uint32_t oldest = last >= RING_SIZE ? last - RING_SIZE + 1 : 1;
    uint32_t seq = since + 1 < oldest ? oldest : since + 1;
    uint32_t end = seq + MAX_RESPONSE_FRAMES - 1 < last ? seq + MAX_RESPONSE_FRAMES - 1 : last;
    std::snprintf(number, sizeof(number), "{\"next\":%u,\"lost\":%u,\"now\":%u,", (unsigned) (seq > end ? since : end),
                  (unsigned) (since == 0 ? 0 : seq - since - 1), (unsigned) millis());
    out += number;
    std::snprintf(number, sizeof(number), "\"period\":%u,\"frames\":[", (unsigned) this->active_period_);
    out += number;
    for (; seq <= end; seq++) {
      const Frame &frame = this->ring_[seq % RING_SIZE];
      std::snprintf(number, sizeof(number), "[%u,\"", (unsigned) frame.ms);
      if (out.back() == ']')
        out += ',';
      out += number;
      for (uint8_t i = 0; i < frame.size; i++) {
        std::snprintf(number, sizeof(number), "%02X", frame.data[i]);
        out += number;
      }
      out += "\"]";
    }
  }
  out += "]}";
  request->send(200, "application/json", out.c_str());
}

void LearnWizard::handleRequest(AsyncWebServerRequest *request) {
  char buffer[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = request->url_to(buffer);
  bool post = request->method() == HTTP_POST;

  if (url == "/dongle/learn" && !post) {
    this->core_->touch();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html; charset=utf-8", this->page_, this->page_size_);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    return;
  }
  if (url == "/dongle/learn/frames" && !post) {
    // Опрос страницы окно не продлевает, но держит ускоренный опрос кондиционера.
    this->last_client_.store(millis());
    this->session_.store(true);
    this->send_frames_(request);
    return;
  }
  if (url == "/dongle/learn/touch" && post) {
    this->core_->touch();
    request->send(200, "application/json", "{}");
    return;
  }
  if (url == "/dongle/learn/stop" && post) {
    this->session_.store(false);
    request->send(200, "application/json", "{}");
    return;
  }
  request->send(404, "text/plain", "Not found");
}

}  // namespace esphome::learn_wizard
