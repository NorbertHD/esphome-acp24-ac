#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/time.h"

#include "esphome/components/time/real_time_clock.h"

#include <cinttypes>

namespace esphome {
namespace acp24 {

// Temperature
const uint8_t ACP24_TEMP_MIN = 18;  // Celsius
const uint8_t ACP24_TEMP_MAX = 30;  // Celsius

class Acp24Climate : public climate_ir::ClimateIR {
 public:
  Acp24Climate()
      : climate_ir::ClimateIR(ACP24_TEMP_MIN, ACP24_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {},
                              {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP}) {}
 void set_time(time::RealTimeClock *time) { time_ = time; }
 time::RealTimeClock *get_time() const { return time_; }

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  /// Set the time of the last transmission.
  int32_t last_transmit_time_{};

  bool parse_state_frame_(const uint8_t frame[]);
  climate::ClimateTraits traits() override;

  time::RealTimeClock *time_;
};

}  // namespace acp24
}  // namespace esphome
