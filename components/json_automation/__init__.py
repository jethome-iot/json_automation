import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CONF_JSON_DATA = "json_data"
CONF_ON_AUTOMATION_LOADED = "on_automation_loaded"
CONF_ON_JSON_ERROR = "on_json_error"

json_automation_ns = cg.esphome_ns.namespace("json_automation")
JsonAutomationComponent = json_automation_ns.class_("JsonAutomationComponent", cg.Component)

AutomationLoadedTrigger = json_automation_ns.class_(
    "AutomationLoadedTrigger", automation.Trigger.template(cg.std_string)
)

JsonErrorTrigger = json_automation_ns.class_(
    "JsonErrorTrigger", automation.Trigger.template(cg.std_string)
)

LoadJsonAction = json_automation_ns.class_("LoadJsonAction", automation.Action)
SaveJsonAction = json_automation_ns.class_("SaveJsonAction", automation.Action)
ExecuteAutomationAction = json_automation_ns.class_("ExecuteAutomationAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(JsonAutomationComponent),
        cv.Optional(CONF_JSON_DATA): cv.string,
        cv.Optional(CONF_ON_AUTOMATION_LOADED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(AutomationLoadedTrigger),
            }
        ),
        cv.Optional(CONF_ON_JSON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(JsonErrorTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_JSON_DATA in config:
        cg.add(var.set_json_data(config[CONF_JSON_DATA]))

    for conf in config.get(CONF_ON_AUTOMATION_LOADED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "data")], conf)

    for conf in config.get(CONF_ON_JSON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "error")], conf)


@automation.register_action(
    "json_automation.load_json",
    LoadJsonAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(JsonAutomationComponent),
            cv.Required(CONF_JSON_DATA): cv.templatable(cv.string),
        }
    ),
)
async def load_json_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, paren)
    template_ = await cg.templatable(config[CONF_JSON_DATA], args, cg.std_string)
    cg.add(var.set_json_data(template_))
    return var


@automation.register_action(
    "json_automation.save_json",
    SaveJsonAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(JsonAutomationComponent),
        }
    ),
)
async def save_json_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, paren)
    return var


@automation.register_action(
    "json_automation.execute",
    ExecuteAutomationAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(JsonAutomationComponent),
            cv.Required("automation_id"): cv.templatable(cv.string),
        }
    ),
)
async def execute_automation_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, paren)
    template_ = await cg.templatable(config["automation_id"], args, cg.std_string)
    cg.add(var.set_automation_id(template_))
    return var
