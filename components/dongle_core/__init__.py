"""Общая основа донгла: окно веб-интерфейса и настройка светодиодов."""
import re

import esphome.codegen as cg
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.components import web_server_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["web_server_base", "ota"]
DEPENDENCIES = ["network"]

CONF_WINDOW_TIMEOUT = "window_timeout"
CONF_BOARD = "board"
CONF_DEVICE = "device"
CONF_VERSION = "version"
CONF_PROJECT_URL = "project_url"


def SAFE_TOKEN(value):
    value = cv.string_strict(value)
    if not re.fullmatch(r"[A-Za-z0-9._-]{1,40}", value):
        raise cv.Invalid("Допустимы латинские буквы, цифры и символы . _ -")
    return value

dongle_core_ns = cg.esphome_ns.namespace("dongle_core")
DongleCore = dongle_core_ns.class_("DongleCore", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DongleCore),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
        cv.Optional(CONF_WINDOW_TIMEOUT, default="1h"): cv.positive_time_period_milliseconds,
        cv.Required(CONF_BOARD): SAFE_TOKEN,
        cv.Required(CONF_DEVICE): SAFE_TOKEN,
        cv.Required(CONF_VERSION): SAFE_TOKEN,
        cv.Optional(CONF_PROJECT_URL, default=""): cv.Any(cv.url, cv.Length(max=0)),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)
    cg.add(var.set_window_timeout(config[CONF_WINDOW_TIMEOUT].total_milliseconds))
    # Метка совместимости лежит в образе, по ней OTA отличает подходящую прошивку.
    marker = f"DONGLEFW1|board={config[CONF_BOARD]}|device={config[CONF_DEVICE]}|version={config[CONF_VERSION]}|"
    cg.add_global(cg.RawExpression(f'__attribute__((used)) static const char DONGLE_FW_MARKER[] = "{marker}"'))
    cg.add(var.set_marker(cg.RawExpression("DONGLE_FW_MARKER")))
    cg.add(var.set_project_url(config[CONF_PROJECT_URL]))
