#include "acp24.h"
#include "esphome/core/log.h"

namespace esphome {
namespace acp24 {

static const char *const TAG = "acp24.climate";

const uint8_t ACP24_OFF = 0x00;
const uint8_t ACP24_MODE_COOL = 0x02;
const uint8_t ACP24_MODE_HEAT = 0x04;
const uint8_t ACP24_MODE_DRY = 0x06;
const uint8_t ACP24_MODE_FAN_ONLY = 0x08;
const uint8_t ACP24_MODE_AUTO = 0x0a;

const uint8_t ACP24_FAN_LOW = 0x00;
const uint8_t ACP24_FAN_MEDIUM = 0x10;
const uint8_t ACP24_FAN_HIGH = 0x20;
const uint8_t ACP24_FAN_AUTO = 0x30;

// Optional presets used to enable some model features
const uint8_t ACP24_NIGHTMODE = 0x80;

// Pulse parameters in usec
const uint16_t ACP24_BIT_MARK = 380;
const uint16_t ACP24_ONE_SPACE = 1330;
const uint16_t ACP24_ZERO_SPACE = 960;
const uint16_t ACP24_HEADER_MARK = 380;
const uint16_t ACP24_HEADER_SPACE = 960;
const uint16_t ACP24_MIN_GAP = 17500;

climate::ClimateTraits Acp24Climate::traits() {
  // auto traits = climate::ClimateTraits();
  auto traits = climate_ir::ClimateIR::traits();

  traits.set_supports_action(false);
  traits.set_visual_min_temperature(ACP24_TEMP_MIN);
  traits.set_visual_max_temperature(ACP24_TEMP_MAX);
  traits.set_visual_temperature_step(1.0f);
  traits.set_supported_modes(
      {climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_AUTO});
  traits.set_supported_fan_modes(
      {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP});

  return traits;
}

void Acp24Climate::transmit_state() {
  uint32_t remote_state[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
      remote_state[0] = ACP24_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[0] = ACP24_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[0] = ACP24_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[0] = ACP24_MODE_FAN_ONLY;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[0] = ACP24_OFF;
      break;
  }

  // Temperature
  remote_state[8] = ((uint8_t) roundf(
      clamp<float>(this->target_temperature, ACP24_TEMP_MIN, ACP24_TEMP_MAX) - 15)) << 2;

  // Fan Speed
  if  (this->mode == climate::CLIMATE_MODE_AUTO) {
    remote_state[0] |= ACP24_FAN_AUTO;
  } else {
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_LOW:
        remote_state[0] |= ACP24_FAN_LOW;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        remote_state[0] |= ACP24_FAN_MEDIUM;
        break;
      case climate::CLIMATE_FAN_HIGH:
        remote_state[0] |= ACP24_FAN_HIGH;
        break;
      default:
        remote_state[0] |= ACP24_FAN_AUTO;
        break;
    }
  }

  ESP_LOGD(TAG, "fan: %02x state: %02x", this->fan_mode.value(), remote_state[0]);

  // Special modes
  switch (this->preset.value()) {
    case climate::CLIMATE_PRESET_SLEEP:
      remote_state[0] |= ACP24_NIGHTMODE;
      break;
    case climate::CLIMATE_PRESET_NONE:
    default:
      break;
  }

  ESP_LOGD(TAG, "sending: %02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",
           remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
           remote_state[6], remote_state[7], remote_state[8]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);
  // Header
  data->mark(ACP24_HEADER_MARK);
  data->space(ACP24_HEADER_SPACE);
  // Data
  for (uint8_t i : remote_state) {
    for (int8_t j = 7; j >= 0; j--) {
      data->mark(ACP24_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? ACP24_ONE_SPACE : ACP24_ZERO_SPACE);
    }
  }
  data->mark(ACP24_BIT_MARK);

  transmit.perform();
}

bool Acp24Climate::parse_state_frame_(const uint8_t frame[]) { return false; }

bool Acp24Climate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[9] = {};

  if (!data.expect_item(ACP24_HEADER_MARK, ACP24_HEADER_SPACE)) {
    ESP_LOGD(TAG, "Header fail");
    return false;
  }

  for (uint8_t pos = 0; pos < 9; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 7; bit >= 0; bit--) {
      if (pos == 8 && bit == 1) break;
      if (data.expect_item(ACP24_BIT_MARK, ACP24_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(ACP24_BIT_MARK, ACP24_ZERO_SPACE)) {
        ESP_LOGD(TAG, "Byte %d bit %d fail", pos, bit);
        return false;
      }
    }

    state_frame[pos] = byte;
  }

  // On/Off and Mode
    switch (state_frame[0] & 0x0e) {
      case ACP24_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case ACP24_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case ACP24_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case ACP24_MODE_FAN_ONLY:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case ACP24_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_AUTO;
        break;
      case ACP24_OFF:
      default:
        this->mode = climate::CLIMATE_MODE_OFF;
        break;
   }
 
  // Temp
  this->target_temperature = ((state_frame[8] & 0x3c) >> 2) + 15;

  // Fan
  switch (state_frame[0] & 0x30) {
      case ACP24_FAN_AUTO:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        break;
      case ACP24_FAN_LOW:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case ACP24_FAN_MEDIUM:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case ACP24_FAN_HIGH:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
  }    
    
  this->preset = (state_frame[0] & ACP24_NIGHTMODE) ? climate::CLIMATE_PRESET_SLEEP : climate::CLIMATE_PRESET_NONE;

  ESP_LOGD(TAG, "Receiving: %s", format_hex_pretty(state_frame, 9).c_str());
  ESP_LOGD(TAG, "Remote Time: %d%d:%d%d", (state_frame[1] >> 4) & 0x0f, state_frame[1] & 0x0f, (state_frame[2] >> 3) & 0x0f, ((state_frame[2] << 1) & 0x0e) + ((state_frame[3] >> 7) & 0x01));
  ESP_LOGD(TAG, "Timer 1: %s", (state_frame[3] & 0x20) ? "On": "Off");
  ESP_LOGD(TAG, "Timer 1 Start Time: %d%d:%s", (state_frame[3] >> 2) & 0x03, ((state_frame[3] << 2) & 0x0c) + ((state_frame[4] >> 6) & 0x03), (state_frame[3] & 0x10) ? "30": "00");
  ESP_LOGD(TAG, "Timer 1 End Time: %d%d:%s", (state_frame[4] >> 1) & 0x03, ((state_frame[4] << 3) & 0x08) + ((state_frame[5] >> 5) & 0x07), (state_frame[4] & 0x08) ? "30": "00");
  ESP_LOGD(TAG, "Timer 2: %s", (state_frame[5] & 0x08) ? "On": "Off");
  ESP_LOGD(TAG, "Timer 2 Start Time: %d%d:%s", state_frame[5] & 0x03, (state_frame[6] >> 4) & 0x0f, (state_frame[5] & 0x04) ? "30": "00");
  ESP_LOGD(TAG, "Timer 2 End Time: %d%d:%s", ((state_frame[6] << 1) & 0x02) + ((state_frame[7] >> 7) & 0x01), (state_frame[7] >> 3) & 0x0f, (state_frame[6] & 0x02) ? "30": "00");

  this->publish_state();
  return true;
}

}  // namespace acp24
}  // namespace esphome
