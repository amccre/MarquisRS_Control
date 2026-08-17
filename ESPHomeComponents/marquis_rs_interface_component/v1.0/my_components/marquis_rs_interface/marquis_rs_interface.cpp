#include "marquis_rs_interface.h"
#include "esphome/core/log.h"
#include <Arduino.h>

namespace esphome {
namespace marquis_rs_interface {

static const char *const TAG = "marquis_rs_interface";

// Map 7-segment MSB-to-LSB encoded bits to characters
char MarquisRSInterface::segment_to_char(uint8_t seg) {
  switch (seg) {
    case 0b1111110: return '0';
    case 0b0110000: return '1';
    case 0b1101101: return '2';
    case 0b1111001: return '3';
    case 0b0110011: return '4';
    case 0b1011011: return '5';
    case 0b1011111: return '6';
    case 0b1110000: return '7';
    case 0b1111111: return '8';
    case 0b1111011: return '9';
    case 0b1110111: return 'A';
    case 0b0011111: return 'b';
    case 0b1001110: return 'C';
    case 0b0111101: return 'd';
    case 0b1001111: return 'E';
    case 0b1000111: return 'F';
    case 0b1011110: return 'G';
    case 0b0110111: return 'H';
    case 0b0000110: return 'I';
    case 0b0111100: return 'J';
    case 0b0111011: return 'K';
    case 0b0001110: return 'L';
    case 0b0010101: return 'n';
    case 0b0011101: return 'o';
    case 0b1100111: return 'P';
    case 0b1110011: return 'q';
    case 0b0000101: return 'r';
    case 0b0001111: return 't';
    case 0b0111110: return 'U';
    case 0b0111111: return 'Y';
    case 0b0000000: return ' ';
    default:        return '?';
  }
}

void IRAM_ATTR gpio_isr_handler(void *arg) {
  static_cast<MarquisRSInterface *>(arg)->isr_read_bit();
}

void MarquisRSInterface::setup() {
  this->clock_pin_->setup();
  this->data_pin_->setup();

  gpio_num_t clock_pin_num = static_cast<gpio_num_t>(this->clock_pin_->get_pin());

  gpio_install_isr_service(0);
  gpio_set_intr_type(clock_pin_num, GPIO_INTR_POSEDGE);
  gpio_isr_handler_add(clock_pin_num, gpio_isr_handler, this);
}

void MarquisRSInterface::loop() {
  while (!decoded_frames_.empty()) {
    uint32_t frame = decoded_frames_.back();
    decoded_frames_.pop_back();

    char bits[22];
    for (int i = 0; i < 21; i++) {
      bits[i] = (frame & (1UL << (20 - i))) ? '1' : '0';
    }
    bits[21] = '\0';
    //ESP_LOGD(TAG, "Decoded Frame: %s (hex: 0x%05X)", bits, frame);

    bool heating = (frame >> 16) & 0x01;

    uint8_t first_seg = ((frame >> 18) & 0x03);
    uint8_t middle_seg = (frame >> 7) & 0x7F;
    uint8_t last_seg = frame & 0x7F;

    std::string display;
    // First digit: only shown if either bit 2 or 3 are set
    if (first_seg != 0) {
    display += '1';
    } else {
    display += ' ';
}
    
    // Middle digit
    display += segment_to_char(middle_seg);

    // Last digit
    display += segment_to_char(last_seg);
    
    if (frame == last_valid_frame_) {
      if (++repeat_count_ >= 3) {
        if (heating_sensor_ != nullptr)
          heating_sensor_->publish_state(heating);
        if (display_sensor_ != nullptr)
          display_sensor_->publish_state(display);
        repeat_count_ = 0;
      }
    } else {
      repeat_count_ = 0;
      last_valid_frame_ = frame;
    }
  }
}

void IRAM_ATTR MarquisRSInterface::isr_read_bit() {
  const uint32_t now = micros();
  const uint32_t gap = now - last_edge_time_us_;
  last_edge_time_us_ = now;

  bool new_frame = false;
  if (gap > 1000 || bit_index_ >= 21) {
    bit_index_ = 0;
    current_frame_ = 0;
    new_frame = true;
  }

  if (new_frame) {
    delayMicroseconds(16);  // Stabilize data line
  }

  gpio_num_t data_pin_num = static_cast<gpio_num_t>(this->data_pin_->get_pin());
  bool data_high = digitalRead(data_pin_num);

  if (data_high) {
    current_frame_ |= (1UL << (20 - bit_index_));
  }

  bit_index_++;

  if (bit_index_ >= 21) {
    bit_index_ = 0;
    decoded_frames_.insert(decoded_frames_.begin(), current_frame_);
  }
}

void MarquisRSInterface::set_clock_pin(InternalGPIOPin *pin) {
  clock_pin_ = pin;
}

void MarquisRSInterface::set_data_pin(InternalGPIOPin *pin) {
  data_pin_ = pin;
}

void MarquisRSInterface::set_heating_sensor(binary_sensor::BinarySensor *sensor) {
  heating_sensor_ = sensor;
}

void MarquisRSInterface::set_display_sensor(text_sensor::TextSensor *sensor) {
  display_sensor_ = sensor;
}

}  // namespace marquis_rs_interface
}  // namespace esphome
