#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <vector>

namespace esphome {
namespace marquis_rs_interface {

class MarquisRSInterface : public Component {
 public:
  void setup() override;
  void loop() override;

  void isr_read_bit();

  void set_clock_pin(InternalGPIOPin *pin);
  void set_data_pin(InternalGPIOPin *pin);
  void set_heating_sensor(binary_sensor::BinarySensor *sensor);
  void set_display_sensor(text_sensor::TextSensor *sensor);

 protected:
  InternalGPIOPin *clock_pin_;
  InternalGPIOPin *data_pin_;
  binary_sensor::BinarySensor *heating_sensor_{nullptr};
  text_sensor::TextSensor *display_sensor_{nullptr};

  std::vector<uint32_t> decoded_frames_;
  uint32_t current_frame_{0};
  uint8_t bit_index_{0};
  uint32_t last_edge_time_us_{0};

  uint32_t last_valid_frame_{0};
  uint8_t repeat_count_{0};

  std::string last_display_;
  uint8_t same_count_{0};

  char segment_to_char(uint8_t seg);
};

}  // namespace marquis_rs_interface
}  // namespace esphome
