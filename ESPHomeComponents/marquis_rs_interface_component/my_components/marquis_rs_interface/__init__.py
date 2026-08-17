import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_DATA_PIN, CONF_CLOCK_PIN

from esphome.components import binary_sensor, text_sensor

# Custom config keys
CONF_HEATING_SENSOR = "heating_sensor"
CONF_DISPLAY_SENSOR = "display_sensor"

CODEOWNERS = ["@your_github"]

marquis_ns = cg.esphome_ns.namespace("marquis_rs_interface")
MarquisRSInterface = marquis_ns.class_("MarquisRSInterface", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MarquisRSInterface),
    cv.Required(CONF_CLOCK_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_DATA_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_HEATING_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(CONF_DISPLAY_SENSOR): cv.use_id(text_sensor.TextSensor),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    clock = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_clock_pin(clock))
    cg.add(var.set_data_pin(data))

    heating = await cg.get_variable(config[CONF_HEATING_SENSOR])
    cg.add(var.set_heating_sensor(heating))

    display = await cg.get_variable(config[CONF_DISPLAY_SENSOR])
    cg.add(var.set_display_sensor(display))
