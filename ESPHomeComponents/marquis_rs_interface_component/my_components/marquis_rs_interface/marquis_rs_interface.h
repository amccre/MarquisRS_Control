#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

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

  // Ring buffer for frames (thread-safe ISR to main loop communication)
  static const uint8_t FRAME_BUFFER_SIZE = 16;
  volatile uint32_t frame_buffer_[FRAME_BUFFER_SIZE];
  volatile uint8_t frame_write_index_{0};
  volatile uint8_t frame_read_index_{0};

  volatile uint32_t current_frame_{0};
  volatile uint8_t bit_index_{0};
  volatile uint32_t last_edge_time_us_{0};

  uint32_t last_publish_time_{0};
  uint32_t last_display_change_time_{0};
  uint32_t last_valid_frame_{0};
  uint8_t repeat_count_{0};

  std::string last_display_;
  uint8_t same_count_{0};

  char segment_to_char(uint8_t seg);
};

}  // namespace marquis_rs_interface
}  // namespace esphome
