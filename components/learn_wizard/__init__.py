"""Мастер обучения: копит принятые кадры и отдаёт их странице. Команд устройству не отправляет."""
import gzip
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

from ..dongle_core import DongleCore

DEPENDENCIES = ["dongle_core"]

CONF_CORE_ID = "core_id"
CONF_PROFILE = "profile"
CONF_ACTIVE_PERIOD = "active_period"
CONF_IDLE_PERIOD = "idle_period"

learn_wizard_ns = cg.esphome_ns.namespace("learn_wizard")
LearnWizard = learn_wizard_ns.class_("LearnWizard", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LearnWizard),
        cv.GenerateID(CONF_CORE_ID): cv.use_id(DongleCore),
        cv.Required(CONF_PROFILE): cv.string_strict,
        cv.Optional(CONF_ACTIVE_PERIOD, default="2s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_IDLE_PERIOD, default="7s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    core = await cg.get_variable(config[CONF_CORE_ID])
    var = cg.new_Pvariable(config[CONF_ID], core)
    await cg.register_component(var, config)

    page = (Path(__file__).parent / "page.html").read_text(encoding="utf-8")
    profile = Path(CORE.relative_config_path(config[CONF_PROFILE])).read_text(encoding="utf-8")
    assert "/*PROFILE*/" in page
    data = gzip.compress(page.replace("/*PROFILE*/", profile).encode("utf-8"), mtime=0)
    numbers = ",".join(str(b) for b in data)
    cg.add_global(cg.RawExpression(f"static const uint8_t LEARN_PAGE_GZ[{len(data)}] = {{{numbers}}}"))
    cg.add(var.set_page(cg.RawExpression("LEARN_PAGE_GZ"), len(data)))
    cg.add(var.set_periods(config[CONF_ACTIVE_PERIOD].total_milliseconds, config[CONF_IDLE_PERIOD].total_milliseconds))
