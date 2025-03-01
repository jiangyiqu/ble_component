#include <esphome.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>

using namespace esphome;

class BLEBroadcaster : public Component {
 public:
  BLEBroadcaster(int send_count, uint32_t interval, const std::vector<uint8_t> &manufacturer_data, bool start_immediately)
      : send_count_(send_count), interval_(interval), manufacturer_data_(manufacturer_data), enabled_(start_immediately) {}

  void setup() override {
    // 初始化BLE
    if (!bt_controller_initialized) {
      esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
      esp_err_t err = esp_bt_controller_init(&cfg);
      if (err != ESP_OK) {
        ESP_LOGE("BLE", "Bluetooth controller init failed: %s", esp_err_to_name(err));
        return;
      }
      err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
      if (err != ESP_OK) {
        ESP_LOGE("BLE", "Bluetooth controller enable failed: %s", esp_err_to_name(err));
        return;
      }
      bt_controller_initialized = true;
    }

    // 配置BLE广告参数
    esp_ble_gap_config_adv_data(&adv_config_);
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = interval_,
        .adv_int_max = interval_,
        .adv_type = ADV_TYPE_NONCONN_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    // 设置制造商数据
    adv_data_.set_manufacturer_data(manufacturer_data_);

    if (enabled_) {
      start_advertising();
    }
  }

  void loop() override {
    if (!enabled_ || send_count_ == 0) return;

    uint32_t now = millis();
    if (now - last_sent_ >= interval_) {
      if (current_count_ < send_count_ || send_count_ == 0) {
        esp_ble_gap_start_advertising(&adv_params_);
        last_sent_ = now;
        if (send_count_ != 0) {
          current_count_++;
          if (current_count_ >= send_count_) {
            stop_advertising();
          }
        }
      }
    }
  }

  void start_advertising() {
    enabled_ = true;
    current_count_ = 0;
    last_sent_ = 0;
  }

  void stop_advertising() {
    enabled_ = false;
    esp_ble_gap_stop_advertising();
  }

  void set_send_count(int count) { send_count_ = count; }
  void set_interval(uint32_t interval) { interval_ = interval; }
  void set_manufacturer_data(const std::vector<uint8_t> &data) { manufacturer_data_ = data; }

 protected:
  static bool bt_controller_initialized;
  int send_count_;
  uint32_t interval_;
  std::vector<uint8_t> manufacturer_data_;
  bool enabled_;
  uint32_t last_sent_ = 0;
  uint32_t current_count_ = 0;
  esp_ble_adv_params_t adv_params_;
  esp_ble_adv_data_t adv_data_;
};

bool BLEBroadcaster::bt_controller_initialized = false;

// 配置验证
auto validate_manufacturer_data = [](const std::string &value) -> std::vector<uint8_t> {
  std::vector<uint8_t> data;
  for (size_t i = 0; i < value.size(); i += 2) {
    std::string byte = value.substr(i, 2);
    data.push_back(static_cast<uint8_t>(std::stoul(byte, nullptr, 16)));
  }
  return data;
};

// YAML配置
BLECastComponent *create_ble_component(int send_times, uint32_t interval, const std::string &manufacturer_data, bool enabled) {
  auto data = validate_manufacturer_data(manufacturer_data);
  return new BLEBroadcaster(send_times, interval, data, enabled);
}
