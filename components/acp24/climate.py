import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

CODEOWNERS = ["@NorbertHD"]
AUTO_LOAD = ["climate_ir"]

acp24_ns = cg.esphome_ns.namespace("acp24")
Acp24Climate = acp24_ns.class_("Acp24Climate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Acp24Climate),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_supports_heat(False))
    await climate_ir.register_climate_ir(var, config)
