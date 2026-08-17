# Marquis Recreational Series Hot Tub Control

ESPHome and Home Assistant control/monitoring for Marquis Recreational Series hot tubs. The project uses an ESP32 to passively decode the topside panel display, report heating and equipment state, and electronically simulate the panel's button presses.

> [!WARNING]
> Hot tubs contain lethal **120/240 V AC**, high-current loads, water, and wet-location wiring. This repository is an educational/hobby project—not a certified spa controller. Installation should be performed or reviewed by a qualified electrician, must retain GFCI protection, and must use suitable isolation, fusing, enclosures, strain relief, grounding, and moisture protection. Disconnect and verify power before opening the spa equipment bay. Use this project at your own risk.

![Home Assistant hot tub controls](Photos/HotTubControlPanel_HomeAssistant.PNG)

## What It Does

- Passively reads the clock and data signals sent to the original topside display.
- Decodes 21-bit frames into the three-character temperature/status display.
- Reports the heater indicator as a Home Assistant binary sensor.
- Exposes **Soak Timer**, **Temp Set**, **Lights**, and **Jets** controls.
- Measures four loads through an ADS1115 and split-core CT sensors:
  - lights;
  - ozonator;
  - jets low speed; and
  - jets high speed.
- Integrates with Home Assistant through ESPHome's native API.
- Includes circuit drawings, printable enclosure models, build photos, and protocol-research material.

The work was developed for circa-2000-and-newer Marquis Recreational Series models including the **Quest RS**, **Destiny RS**, and **Reward RS**. Hardware and protocol revisions may differ, so verify every signal before connecting your equipment.

## Project Status

This is a working, hardware-specific DIY implementation rather than a universal plug-and-play product.

- **v2.0** is the current ESPHome component and recommended starting point.
- **v1.0** is retained as historical/reference material.
- Current-transformer calibration and state thresholds must be validated for each installation.
- No automated hardware-in-the-loop test suite is included.

## Hardware

### Main Components

| Quantity | Component | Purpose |
| ---: | --- | --- |
| 1 | ESP32 development board (`esp32dev`) | ESPHome controller and display decoder |
| 1 | ADS1115, address `0x48` | Four-channel current-sensor ADC |
| 2 | SCT-013-005 (5 A : 1 V) CT sensors | Lights and ozonator monitoring |
| 2 | SCT-013-010 (10 A : 1 V) CT sensors | Low- and high-speed jet monitoring |
| As required | Optocouplers/transistor output stages | Electrically isolated button simulation |
| As required | Protected power supply, fuse, connectors, enclosure | Safe installation |

See [`Circuit_Diagram.pdf`](Circuit_Diagram.pdf) for the circuit drawing and [`Buttons&CTs.txt`](Buttons%26CTs.txt) for the original component notes.

### ESP32 Pin Map

| ESP32 pin | Function |
| --- | --- |
| GPIO25 | Topside display clock input |
| GPIO26 | Topside display data input |
| GPIO19 | Lights button output |
| GPIO18 | Soak Timer button output |
| GPIO17 | Temp Set button output |
| GPIO16 | Jets button output |
| GPIO21 | ADS1115 I²C SDA |
| GPIO22 | ADS1115 I²C SCL |

The button outputs issue 150 ms pulses. They are logic signals and should operate a correctly designed isolated interface—not be connected directly to an unknown-voltage panel circuit.

### ADS1115 Channels

| Channel | Monitored load | Example threshold |
| --- | --- | ---: |
| A0 | Lights | 0.75 A |
| A1 | Ozonator | 0.05 A |
| A2 | Jets low speed | 2 A |
| A3 | Jets high speed | 2 A |

The checked-in values document one installation only. Calibrate against known loads and adjust the `calibrate_linear` filters and thresholds before relying on them.

## Repository Layout

```text
.
├── ESPHomeComponents/
│   └── marquis_rs_interface_component/
│       ├── v1.0/                     # Historical component/configuration
│       └── v2.0/                     # Current component/configuration
├── Photos/                            # Build and Home Assistant images
├── ReverseEngineering/                # Protocol captures and research
├── Circuit_Diagram.odg                # Editable circuit drawing
├── Circuit_Diagram.pdf                # Viewable circuit drawing
├── Enclosure-Blank.stl                # Blank enclosure model
├── HotTubControlEnclosure.stl         # Finished printable enclosure
└── Buttons&CTs.txt                     # Original controls/CT notes
```

The current firmware lives in:

```text
ESPHomeComponents/marquis_rs_interface_component/v2.0/
├── hot-tub-control.yaml
├── secrets.example.yaml
└── my_components/
    └── marquis_rs_interface/
        ├── __init__.py
        ├── component.yaml
        ├── marquis_rs_interface.cpp
        └── marquis_rs_interface.h
```

## ESPHome Setup

### 1. Get the Repository

Clone this private repository on the system where ESPHome is installed:

```bash
git clone https://github.com/amccre/MarquisRS_Control.git
cd MarquisRS_Control/ESPHomeComponents/marquis_rs_interface_component/v2.0
```

### 2. Create Local Secrets

Copy the provided template. Do **not** commit the resulting `secrets.yaml` file.

PowerShell:

```powershell
Copy-Item secrets.example.yaml secrets.yaml
```

Linux/macOS:

```bash
cp secrets.example.yaml secrets.yaml
```

Edit `secrets.yaml` and provide unique values for:

- `wifi_ssid`
- `wifi_password`
- `api_encryption_key`
- `ota_password`
- `fallback_ap_password`

Generate an ESPHome-compatible API key with:

```bash
openssl rand -base64 32
```

Use long, unique random values for the OTA and fallback hotspot passwords.

### 3. Review the Configuration

Before flashing, verify these items in [`hot-tub-control.yaml`](ESPHomeComponents/marquis_rs_interface_component/v2.0/hot-tub-control.yaml):

1. ESP32 board type and framework.
2. GPIO assignments against the physical circuit.
3. ADS1115 address and I²C wiring.
4. CT calibration factors.
5. Current thresholds for each binary sensor.
6. Isolation and active-high behavior of every button output.

### 4. Validate and Flash

From the v2.0 directory:

```bash
esphome config hot-tub-control.yaml
esphome run hot-tub-control.yaml
```

The first flash may be performed over USB. Subsequent updates can use ESPHome OTA after the device joins the network.

### 5. Add to Home Assistant

ESPHome normally advertises the device automatically. In Home Assistant:

1. Open **Settings → Devices & services**.
2. Select the discovered ESPHome device, or add the **ESPHome** integration manually.
3. Supply the API encryption key when prompted.
4. Add the display, heating, current, state, and button entities to a dashboard.

## Display Protocol

The custom `marquis_rs_interface` component attaches an interrupt to the display clock line and samples the data line on rising edges. A gap greater than 1,000 µs marks a frame boundary. Each frame contains 21 bits, transmitted most-significant bit first.

```text
Bit:      20 | 19 18 | 17 | 16 | 15 14 | 13 ........ 7 | 6 ......... 0
Meaning:   - | first |  - |heat|   -   | middle digit | final digit
```

- Bits 19–18 indicate whether the first character is `1` or blank.
- Bit 16 is the heater indicator.
- Bits 13–7 and 6–0 contain seven-segment patterns.
- Three repeated identical frames are required before publication to reject noise.
- State publication is rate-limited to 100 ms.
- A 16-frame ring buffer transfers captured frames from the ISR to ESPHome's main loop.

The decoder handles digits and the subset of letters representable by the panel's seven-segment display, allowing values such as `102`, ` 98`, and status/error text. Detailed implementation notes are available in the [v2 component documentation](ESPHomeComponents/marquis_rs_interface_component/v2.0/README.md).

## Troubleshooting

### Display is blank, `???`, or unstable

- Measure signal voltage and confirm it is safe for the ESP32's 3.3 V inputs.
- Confirm clock/data pins and common reference are correct.
- Use appropriate buffering, level shifting, and high-impedance taps.
- Keep low-voltage signal wiring away from pump and heater conductors.
- Temporarily use ESPHome debug logging to inspect behavior.

### Load state is always on or off

- Inspect the raw ADS1115 values.
- Confirm each CT surrounds only one conductor, not an entire cable containing both line and return.
- Check sensor orientation and channel assignment.
- Calibrate against a known current and tune both calibration and threshold values.

### A button does not operate the spa

- Verify the optocoupler/transistor circuit and its polarity.
- Confirm the output is connected to the intended panel switch circuit.
- Check the required switch closure duration and adjust the 150 ms pulse if necessary.
- Never connect an ESP32 GPIO directly to mains or an unverified control-panel signal.

### Wi-Fi is unreliable near the equipment bay

- Improve antenna placement or use an ESP32 with an external antenna.
- Check low-voltage supply quality and decoupling.
- Increase separation from motors, relays, and high-current wiring.

## Security Notes

- Real API, OTA, Wi-Fi, and fallback hotspot credentials are not stored in this repository.
- `secrets.yaml`, ESPHome build state, and generated Python caches are excluded through `.gitignore`.
- Rotate any credential that has previously been shared or committed elsewhere.
- Repository privacy does not replace proper secret management.

## Documentation and Fabrication Files

- [Circuit diagram (PDF)](Circuit_Diagram.pdf)
- [Editable circuit diagram (ODG)](Circuit_Diagram.odg)
- [Finished enclosure STL](HotTubControlEnclosure.stl)
- [Blank enclosure STL](Enclosure-Blank.stl)
- [Build photos](Photos/)
- [Reverse-engineering material](ReverseEngineering/)

## Disclaimer

This project is not affiliated with, endorsed by, or supported by Marquis Corp. Product and model names are used only to describe compatibility. The software, drawings, and models are supplied **as is**, without warranty. The user assumes all risk for property damage, equipment failure, code compliance, warranty impact, electric shock, fire, injury, or death.

No license file is currently included. Unless and until the owner adds an explicit license, the contents remain under the copyright holder's default rights and should not be assumed to be open source solely because the source is visible to repository collaborators.
