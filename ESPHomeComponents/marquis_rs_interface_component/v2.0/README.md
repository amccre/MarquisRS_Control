# Marquis Recreational Series Hot Tub Controller

## Overview

This project provides a complete ESPHome-based control and monitoring system for Marquis Recreational Series hot tubs (Quest RS, Destiny RS, Reward RS, circa 2000+). It interfaces with the hot tub's topside control panel to read display information and heating status, while also providing button control and current monitoring capabilities for various hot tub functions.

### Key Features

- **Non-invasive LCD Display Reading**: Taps into the serial communication between the hot tub controller and LCD display
- **Real-time Status Monitoring**: Tracks heating status, temperature display, and component operation
- **Button Control**: Simulates physical button presses for lights, jets, temperature, and soak timer
- **Current Monitoring**: Uses CT clamps to detect operation of lights, ozonator, and jet pumps
- **Home Assistant Integration**: Full integration via ESPHome API for automation and monitoring

---

## Hardware Architecture

### Required Components

1. **ESP32 Development Board** (esp32dev)
2. **ADS1115 16-bit ADC** (I2C address 0x48)
3. **CT Clamp Current Sensors** (4x)
   - Lights monitoring  (SCT-013-005   5A:1V)
   - Ozonator monitoring  (SCT-013-005   5A:1V)
   - Jets low speed monitoring (SCT-013-010   10A:1V)
   - Jets high speed monitoring (SCT-013-010   10A:1V)
   - Heating (Not Needed, detected via LCD display logic)
4. **GPIO Output Drivers** for button simulation

### GPIO Pin Assignments

#### LCD Signal Tapping
- **GPIO25**: Clock signal from hot tub controller to LCD
- **GPIO26**: Data signal from hot tub controller to LCD

These pins passively read the serial communication without interfering with normal operation.

#### Button Control Outputs
- **GPIO19**: Lights button output
- **GPIO18**: Soak timer button output
- **GPIO17**: Temperature set button output
- **GPIO16**: Jets button output

These outputs simulate button presses with 150ms duration pulses.

#### I2C Bus (ADS1115 ADC)
- **GPIO21**: I2C SDA
- **GPIO22**: I2C SCL

#### ADS1115 ADC Channels
- **A0**: Lights current sensing
- **A1**: Ozonator current sensing
- **A2**: Jets low speed current sensing
- **A3**: Jets high speed current sensing

All channels configured with 6.144V gain for safety and wide dynamic range.

---

## Custom Component: marquis_rs_interface

### Purpose

The custom ESPHome component decodes the serial protocol between the hot tub's main controller and the topside LCD display. This protocol carries temperature readings, heating status, and error codes.

### Communication Protocol

#### Frame Structure
- **21-bit serial frames** transmitted on clock edges
- **MSB-first** bit ordering
- **Frame timing**: >1000μs gap indicates new frame start
- **Bit timing**: ~16μs data line stabilization delay after clock edge

#### Frame Bit Layout (21 bits)

```
Bit Position:   20 | 19 18 | 17 | 16 | 15  14 | 13 12 11 10 9 8 7 | 6 5 4 3 2 1 0
               ----+-------+----+----+--------+-------------------+---------------
Content:       null| First |null|Heat|  null  |  Middle 7-segment | Last 7-segment
                   | Digit |    |    |        |                   |
               ----|-------|----|----|--------|-------------------|---------------
               1bit| 2bits |1bit|1bit| 2bits  |    7 bits         |    7 bits
               ----|-------|----|----|--------|-------------------|---------------
                 a |  b c  |  d | e  | f   g  | a  b  c  d  e f g | a b c d e f g
               ----|-------|----|----|--------|-------------------|---------------
```

- **Bits 19-18**: First digit (hundreds place)
  - `00` = blank space (no hundreds digit shown)
  - `!= 00` = '1' displayed (for temperatures ≥100°F)
  
- **Bit 16**: Heating indicator
  - `1` = Heater ON
  - `0` = Heater OFF
  
- **Bits 13-7**: Middle digit (7-segment encoded)
  
- **Bits 6-0**: Last digit (7-segment encoded)

#### 7-Segment Encoding

Each digit uses standard 7-segment display encoding:

```
    A
   ---
F |   | B
   -G-
E |   | C
   ---
    D

Bit order: A-B-C-D-E-F-G (MSB to LSB)
```

Complete mappings:
- `0b1111110` = '0'
- `0b0110000` = '1'
- `0b1101101` = '2'
- `0b1111001` = '3'
- `0b0110011` = '4'
- `0b1011011` = '5'
- `0b1011111` = '6'
- `0b1110000` = '7'
- `0b1111111` = '8'
- `0b1111011` = '9'
- `0b1110111` = 'A'
- `0b0011111` = 'b'
- `0b1001110` = 'C'
- `0b0111101` = 'd'
- `0b1001111` = 'E'
- `0b1000111` = 'F'
- `0b1011110` = 'G'
- `0b0110111` = 'H'
- `0b0000110` = 'I'
- `0b0111100` = 'J'
- `0b0111011` = 'K'
- `0b0001110` = 'L'
- `0b0010101` = 'n'
- `0b0011101` = 'o'
- `0b1100111` = 'P'
- `0b1110011` = 'q'
- `0b0000101` = 'r'
- `0b0001111` = 't'
- `0b0111110` = 'U'
- `0b0111111` = 'Y'
- `0b0000000` = ' ' (space)
- `default` = '?' (unknown pattern)

### Component Architecture

#### Class Structure

```cpp
class MarquisRSInterface : public Component {
 public:
  void setup() override;      // Initialize GPIO and ISR
  void loop() override;       // Process decoded frames
  void isr_read_bit();        // ISR for clock edge detection

  // Configuration setters
  void set_clock_pin(InternalGPIOPin *pin);
  void set_data_pin(InternalGPIOPin *pin);
  void set_heating_sensor(binary_sensor::BinarySensor *sensor);
  void set_display_sensor(text_sensor::TextSensor *sensor);

 protected:
  // GPIO pins for LCD signal tapping
  InternalGPIOPin *clock_pin_;
  InternalGPIOPin *data_pin_;

  // ESPHome sensors to publish data
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
```

#### Interrupt Service Routine (ISR)

The `isr_read_bit()` function is called on every rising edge of the clock signal:

1. **Frame Detection**: Measures time gap since last edge
   - Gap >1000μs indicates new frame start
   - Resets bit counter and current frame buffer

2. **Data Sampling**: After 16μs stabilization delay
   - Reads data pin state (HIGH or LOW)
   - Shifts bit into appropriate position in 21-bit frame

3. **Frame Completion**: After 21 bits collected
   - Adds completed frame to decode queue
   - Resets for next frame

#### Main Loop Processing

The `loop()` function runs in the main ESPHome event loop:

1. **Frame Retrieval**: Processes frames from ISR queue (FIFO)

2. **Bit Extraction**: Extracts heating status and 7-segment data

3. **Character Decoding**: Converts 7-segment codes to ASCII characters

4. **Validation**: Uses repeat count for noise rejection
   - Requires 3 consecutive identical frames before publishing
   - Prevents spurious readings from electrical noise

5. **State Publishing**: Updates ESPHome sensors
   - `heating_sensor`: Binary state (ON/OFF)
   - `display_sensor`: 3-character text string

### Character Decoding

The `segment_to_char()` function maps 7-segment bit patterns to characters:

- **Digits 0-9**: Standard numeric display
- **Letters**: A, b, C, d, E, F, G, H, I, J, K, L, n, o, P, q, r, t, U, Y
- **Special**: Space (all segments off), '?' (unknown pattern)

This allows decoding of:
- Temperature readings (e.g., "102", " 98")
- Error codes (e.g., "FLo", "dr ", "SnS")
- Status messages

---

## ESPHome Configuration

### Component Declaration

```yaml
marquis_rs_interface:
  id: marquis_spa              # Component instance ID
  clock_pin: GPIO25            # LCD clock signal input
  data_pin: GPIO26             # LCD data signal input
  heating_sensor: heating      # Binary sensor for heater status
  display_sensor: display_text # Text sensor for display reading
```

### Binary Sensors

#### Heating Status Sensor
```yaml
binary_sensor:
  - platform: template
    name: "Hot Tub Heating"
    id: heating
    disabled_by_default: false
```
Updated by the custom component with real-time heating status.

#### Current-Based Detection Sensors

These sensors use current monitoring to detect component operation:

```yaml
  - platform: template
    name: Lights
    id: lights
    condition:
      sensor.in_range:
        id: lights_current
        above: 0.75           # Threshold: 0.75A

  - platform: template
    name: Ozonator
    condition:
      sensor.in_range:
        id: ozonator_current
        above: 0.05           # Threshold: 0.05A

  - platform: template
    name: Jets Low
    condition:
      sensor.in_range:
        id: jets_low_current
        above: 2              # Threshold: 2A

  - platform: template
    name: Jets High
    condition:
      sensor.in_range:
        id: jets_high_current
        above: 2              # Threshold: 2A
```

### Text Sensors

```yaml
text_sensor:
  - platform: template
    id: display_text
    name: "Hot Tub Display"
    disabled_by_default: false
    update_interval: 250ms    # Rapid updates for responsive display
```

### Current Monitoring Sensors

Each current sensor follows this pattern:

```yaml
sensor:
  # Raw ADC reading
  - platform: ads1115
    multiplexer: 'A0_GND'     # ADC channel
    gain: 6.144               # Voltage range
    name: "A0 RAW"
    accuracy_decimals: 2
    internal: True            # Hidden from UI
    id: a0_raw
    update_interval: 2000ms

  # CT clamp processing
  - platform: ct_clamp
    sensor: a0_raw            # Source sensor
    id: lights_current
    name: Lights Current
    sample_duration: 150ms    # RMS calculation window
    update_interval: 2000ms
    accuracy_decimals: 2
    filters:
      - calibrate_linear:
        - 0 -> 0              # Calibration points
        - 1 -> 5              # Raw -> Actual current (A, or 1 -> 10 for jets)
```

**Note**: Calibration values should be adjusted based on actual CT clamp specifications and load measurements. Jets sensors use 1 -> 10 calibration.

### Button Controls

```yaml
output:
  - platform: gpio
    id: lights_output
    pin: GPIO19
  - platform: gpio
    id: soak_timer_output
    pin: GPIO18
  - platform: gpio
    id: temp_set_output
    pin: GPIO17
  - platform: gpio
    id: jets_output
    pin: GPIO16

button:
  - platform: output
    name: "Soak Timer Button"
    output: soak_timer_output
    duration: 150ms
  - platform: output
    name: "Temp Set Button"
    output: temp_set_output
    duration: 150ms
  - platform: output
    name: "Lights Button"
    output: lights_output
    duration: 150ms
  - platform: output
    name: "Jets Button"
    output: jets_output
    duration: 150ms
```

---

## Installation and Setup

### 1. Hardware Installation

1. **Identify LCD connector** on hot tub control board
2. **Tap clock and data lines** with high-impedance connections
   - Use voltage dividers if levels exceed 3.3V
   - Ensure proper grounding between ESP32 and hot tub controller
3. **Install CT clamps** around power lines for each monitored component
4. **Connect button outputs** to hot tub control panel button pads
   - Use optocouplers or transistors for isolation
5. **Verify I2C connection** to ADS1115
6. **Ensure proper power supply** for ESP32 (stable 3.3V/5V)

### 2. Software Configuration

1. **Install ESPHome** on your system
2. **Copy custom component** files to `my_components/marquis_rs_interface/`
3. **Update WiFi credentials** in `hot-tub-control.yaml`:
   ```yaml
   wifi:
     ssid: !secret wifi_ssid
     password: !secret wifi_password
   ```
4. **Adjust API encryption key** for Home Assistant integration
5. **Calibrate current sensors** with known loads
6. **Flash ESP32** with ESPHome:
   ```bash
   esphome run hot-tub-control.yaml
   ```

### 3. Home Assistant Integration

1. The device will auto-discover via ESPHome API
2. Add to Home Assistant dashboard
3. Create automations based on:
   - Temperature readings
   - Heating status
   - Component operation states

---

## Troubleshooting

### Display Shows "???" or Garbage

- **Cause**: Signal levels incompatible or noise on data line
- **Solution**: 
  - Check voltage levels (should be 3.3V or 5V logic)
  - Add pull-up/pull-down resistors
  - Verify clock and data pin connections
  - Ensure common ground with hot tub controller

### Heating Status Not Updating

- **Cause**: Frame validation failing or bit 16 not being read correctly
- **Solution**:
  - Enable debug logging: `logger: level: DEBUG`
  - Check for frame repeat count issues
  - Verify clock edge detection in ISR

### Current Sensors Always Off/On

- **Cause**: Calibration issues or threshold problems
- **Solution**:
  - Monitor raw ADC values and actual current readings
  - Adjust `calibrate_linear` filter values
  - Modify threshold values in binary sensor conditions
  - Verify CT clamp orientation and placement

### Buttons Not Working

- **Cause**: Insufficient current/voltage to trigger button circuit
- **Solution**:
  - Use transistor or relay drivers for higher current
  - Adjust pulse duration (increase from 150ms if needed)
  - Verify connection to correct button pads

### WiFi Connection Issues

- **Cause**: Interference from hot tub equipment or signal strength
- **Solution**:
  - Use external antenna on ESP32
  - Add capacitors for power supply stability
  - Move ESP32 away from pump motors and heater

---

## Technical Implementation Details

### Why Interrupt Service Routine (ISR)?

The clock signal timing is critical and can occur at varying rates. Using an ISR ensures:
- **No missed bits**: Every clock edge is captured
- **Precise timing**: Microsecond-level accuracy
- **Minimal jitter**: Independent of main loop execution

### Frame Validation Strategy

The component uses several techniques to ensure accurate readings:

1. **Gap Detection**: >1000μs gap reliably indicates frame boundaries
2. **Repeat Counting**: Requires 3 identical consecutive frames
3. **Bit Length Validation**: Expects exactly 21 bits per frame
4. **Checksum Alternative**: Since no checksum exists in protocol, repetition provides validation

### Performance Considerations

- **ISR Execution Time**: Minimized to <50μs per bit
- **Frame Queue**: Uses ring buffer for thread-safe ISR-to-main-loop communication
- **Rate Limiting**: Updates published at most every 100ms to prevent overwhelming Home Assistant
- **Enhanced Flashing Detection**: Tracks display changes within 2-second windows
- **Memory Usage**: Minimal state tracking (few integers and booleans)
- **CPU Load**: Negligible impact on ESP32 with interrupt-driven design

### Future Enhancement Possibilities

1. **Write Capability**: Inject commands to control hot tub directly
2. **Error Code Database**: Decode and explain error messages
3. **Historical Data**: Log temperature trends and usage patterns
4. **Advanced Automation**: Predictive heating based on usage
5. **Multi-Controller Support**: Handle different Marquis series protocols

---

## Code File Structure

```
.
├── hot-tub-control.yaml                          # Main ESPHome configuration
└── my_components/
    └── marquis_rs_interface/
        ├── __init__.py                           # Python integration layer
        ├── component.yaml                        # Component metadata
        ├── marquis_rs_interface.h                # C++ header file
        └── marquis_rs_interface.cpp              # C++ implementation
```

### File Descriptions

- **`hot-tub-control.yaml`**: ESPHome configuration defining all sensors, buttons, and component instances
- **`__init__.py`**: Python code for ESPHome build system integration, validates configuration schema
- **`component.yaml`**: Metadata describing the component for ESPHome ecosystem
- **`marquis_rs_interface.h`**: C++ class declaration and public interface
- **`marquis_rs_interface.cpp`**: Core logic for frame decoding, ISR handling, and sensor publishing

---

## Safety and Warranty Considerations

⚠️ **Important Safety Information**:

1. **Electrical Hazard**: Hot tubs operate at 240V AC. All modifications should be performed by qualified personnel.
2. **Warranty Impact**: Modifying the hot tub control system may void manufacturer warranty.
3. **Isolation**: Use proper electrical isolation (optocouplers) between ESP32 and hot tub circuits.
4. **Water Protection**: Ensure ESP32 and electronics are protected from water and moisture.
5. **GFCI Protection**: Verify all electrical work maintains proper ground fault protection.

This project is provided as-is for educational and hobbyist purposes. The authors assume no liability for damages or injuries resulting from implementation.

---

## License

This project is open source. Please check individual file headers for specific licensing information.

## Credits

Developed for Marquis Recreational Series hot tubs (circa 2000+). Compatible models include Quest RS, Destiny RS, and Reward RS.

## Contributing

Contributions are welcome! Areas for improvement:
- Additional error code mappings
- Support for other Marquis series
- Enhanced filtering algorithms
- Calibration tools

---

## Appendix: Protocol Analysis

### Sample Frame Decoding

**Raw Frame**: `0b111001101111100110000`

Breaking down the 21 bits:
```
Bits 20-19: 11 (First digit = '1')
Bit 16:     1  (Heating = ON)
Bits 18-17, 15-7: 0011101 (Middle = '0')
Bits 6-0:   0110000 (Last = '1')
```

**Result**: Display shows "101", Heater is ON

### Common Display Values

| Display | Meaning |
|---------|---------|
| "102"   | Temperature 102°F |
| " 98"   | Temperature 98°F (no hundreds digit) |
| "FLo"   | Flow error (check circulation pump) |
| "dr "   | Drain mode |
| "SnS"   | Sensor error |
| "---"   | Reading not available |

---

**Last Updated**: December 1, 2025
