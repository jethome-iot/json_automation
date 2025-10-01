#include "json_automation.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <algorithm>
#include <cstring>

namespace esphome {
namespace json_automation {

static const char *const TAG = "json_automation";

void JsonAutomationComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up JSON Automation Component...");

  this->pref_ = global_preferences->make_preference<char[MAX_JSON_SIZE]>(fnv1_hash(std::string("json_automation")));

  if (!this->json_data_.empty()) {
    ESP_LOGD(TAG, "Parsing initial JSON data and creating automations");
    if (this->parse_json_automations(this->json_data_)) {
      this->save_json_to_preferences();
      this->create_all_automations();
    }
  } else {
    ESP_LOGD(TAG, "Loading JSON data from preferences");
    if (this->load_json_from_preferences()) {
      this->create_all_automations();
    }
  }
}

void JsonAutomationComponent::loop() {}

void JsonAutomationComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "JSON Automation Component:");
  ESP_LOGCONFIG(TAG, "  Number of parsed automations: %d", this->automations_.size());
  ESP_LOGCONFIG(TAG, "  Active automation objects: %d", this->automation_objects_.size());

  for (const auto &automation : this->automations_) {
    ESP_LOGCONFIG(TAG, "  Automation: %s (%s)", automation.id.c_str(), automation.name.c_str());
    ESP_LOGCONFIG(TAG, "    Enabled: %s", automation.enabled ? "YES" : "NO");
    ESP_LOGCONFIG(TAG, "    Trigger: input_id=%s", automation.trigger.input_id.c_str());
    ESP_LOGCONFIG(TAG, "    Actions: %d", automation.actions.size());
  }
}

void JsonAutomationComponent::set_json_data(const std::string &json_data) { this->json_data_ = json_data; }

bool JsonAutomationComponent::load_json_from_preferences() {
  char buffer[MAX_JSON_SIZE];
  if (this->pref_.load(&buffer)) {
    std::string stored_json(buffer);
    ESP_LOGD(TAG, "Loaded JSON from preferences (%d bytes)", stored_json.size());
    this->json_data_ = stored_json;
    return this->parse_json_automations(stored_json);
  }
  ESP_LOGW(TAG, "No JSON data found in preferences");
  return false;
}

bool JsonAutomationComponent::save_json_to_preferences() {
  if (this->json_data_.empty()) {
    ESP_LOGW(TAG, "Cannot save empty JSON data");
    return false;
  }

  if (this->json_data_.size() >= MAX_JSON_SIZE) {
    ESP_LOGE(TAG, "Cannot save: JSON data too large: %d bytes (max: %d)", this->json_data_.size(), MAX_JSON_SIZE - 1);
    this->trigger_json_error("JSON data exceeds maximum size for saving");
    return false;
  }

  char buffer[MAX_JSON_SIZE];
  memset(buffer, 0, MAX_JSON_SIZE);
  strncpy(buffer, this->json_data_.c_str(), MAX_JSON_SIZE - 1);
  buffer[MAX_JSON_SIZE - 1] = '\0';

  if (this->pref_.save(&buffer)) {
    ESP_LOGD(TAG, "JSON data saved to preferences (%d bytes)", this->json_data_.size());
    return true;
  }

  ESP_LOGE(TAG, "Failed to save JSON data to preferences");
  return false;
}

TriggerSource JsonAutomationComponent::parse_trigger_source(const std::string &source) {
  std::string lower = source;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "input")
    return TriggerSource::INPUT;
  return TriggerSource::UNKNOWN;
}

TriggerType JsonAutomationComponent::parse_trigger_type(const std::string &type) {
  std::string lower = type;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "press")
    return TriggerType::PRESS;
  if (lower == "release")
    return TriggerType::RELEASE;
  return TriggerType::UNKNOWN;
}

ActionSource JsonAutomationComponent::parse_action_source(const std::string &source) {
  std::string lower = source;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "switch")
    return ActionSource::SWITCH;
  if (lower == "delay")
    return ActionSource::DELAY;
  if (lower == "light")
    return ActionSource::LIGHT;
  return ActionSource::UNKNOWN;
}

ActionType JsonAutomationComponent::parse_action_type(const std::string &type) {
  std::string lower = type;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "turn_on")
    return ActionType::TURN_ON;
  if (lower == "turn_off")
    return ActionType::TURN_OFF;
  if (lower == "toggle")
    return ActionType::TOGGLE;
  return ActionType::UNKNOWN;
}

bool JsonAutomationComponent::parse_json_automations(const std::string &json_data) {
  ESP_LOGD(TAG, "Parsing JSON automations...");

  if (json_data.size() > MAX_JSON_SIZE) {
    ESP_LOGE(TAG, "JSON data too large: %d bytes (max: %d)", json_data.size(), MAX_JSON_SIZE);
    this->trigger_json_error("JSON data exceeds maximum size");
    return false;
  }

  this->automations_.clear();

  bool parse_success = json::parse_json(json_data, [this](JsonObject root) -> bool {
    JsonArray automations_array = root.as<JsonArray>();

    if (!automations_array.isNull()) {
      for (JsonVariant automation_var : automations_array) {
        JsonObject automation_obj = automation_var.as<JsonObject>();

        if (!automation_obj.containsKey("id") || !automation_obj.containsKey("trigger") ||
            !automation_obj.containsKey("actions")) {
          ESP_LOGW(TAG, "Skipping invalid automation: missing required fields");
          continue;
        }

        AutomationRule rule;
        rule.id = automation_obj["id"].as<std::string>();
        rule.name = automation_obj.containsKey("name") ? automation_obj["name"].as<std::string>() : rule.id;
        rule.enabled = automation_obj.containsKey("enabled") ? automation_obj["enabled"].as<bool>() : true;

        JsonObject trigger_obj = automation_obj["trigger"];
        if (trigger_obj.containsKey("source")) {
          rule.trigger.source = this->parse_trigger_source(trigger_obj["source"].as<std::string>());
        }
        if (trigger_obj.containsKey("type")) {
          rule.trigger.type = this->parse_trigger_type(trigger_obj["type"].as<std::string>());
        }
        if (trigger_obj.containsKey("input_id")) {
          rule.trigger.input_id = trigger_obj["input_id"].as<std::string>();
        }

        // Validate trigger fields
        if (rule.trigger.source == TriggerSource::UNKNOWN || rule.trigger.type == TriggerType::UNKNOWN ||
            rule.trigger.input_id.empty()) {
          ESP_LOGW(TAG, "Skipping automation %s: invalid or missing trigger fields", rule.id.c_str());
          continue;
        }

        JsonArray actions = automation_obj["actions"];
        int valid_action_count = 0;
        for (JsonVariant action_var : actions) {
          if (action_var.is<JsonObject>()) {
            JsonObject action_obj = action_var.as<JsonObject>();

            Action action;
            if (action_obj.containsKey("source")) {
              action.source = this->parse_action_source(action_obj["source"].as<std::string>());
            }
            if (action_obj.containsKey("type")) {
              action.type = this->parse_action_type(action_obj["type"].as<std::string>());
            }
            if (action_obj.containsKey("switch_id")) {
              action.switch_id = action_obj["switch_id"].as<std::string>();
            }
            if (action_obj.containsKey("delay_s")) {
              action.delay_s = action_obj["delay_s"].as<uint32_t>();
            }

            // Validate action fields
            bool valid_action = false;
            if (action.source == ActionSource::DELAY && action.delay_s > 0) {
              valid_action = true;
            } else if ((action.source == ActionSource::SWITCH || action.source == ActionSource::LIGHT) &&
                       action.type != ActionType::UNKNOWN && !action.switch_id.empty()) {
              valid_action = true;
            }

            if (valid_action) {
              rule.actions.push_back(action);
              valid_action_count++;
            } else {
              ESP_LOGW(TAG, "Skipping invalid action in automation %s", rule.id.c_str());
            }
          }
        }

        // Only add automation if it has at least one valid action
        if (valid_action_count > 0) {
          this->automations_.push_back(rule);
          ESP_LOGD(TAG, "Loaded automation: %s (%s) with %d valid actions", rule.id.c_str(), rule.name.c_str(),
                   valid_action_count);
        } else {
          ESP_LOGW(TAG, "Skipping automation %s: no valid actions", rule.id.c_str());
        }
      }

      return true;
    }

    ESP_LOGE(TAG, "JSON is not an array");
    this->trigger_json_error("JSON must be an array of automations");
    return false;
  });

  if (parse_success) {
    ESP_LOGI(TAG, "Successfully parsed %d automations", this->automations_.size());
    this->trigger_automation_loaded(json_data);
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to parse JSON automations");
    this->trigger_json_error("JSON parsing failed");
    return false;
  }
}

binary_sensor::BinarySensor *JsonAutomationComponent::resolve_binary_sensor(const std::string &object_id) {
  uint32_t key = esphome::fnv1_hash(object_id);
  auto *sensor = App.get_binary_sensor_by_key(key);
  if (!sensor) {
    ESP_LOGW(TAG, "Binary sensor not found: %s (hash: %u)", object_id.c_str(), key);
  } else {
    ESP_LOGD(TAG, "Resolved binary_sensor: %s (hash: %u)", object_id.c_str(), key);
  }
  return sensor;
}

switch_::Switch *JsonAutomationComponent::resolve_switch(const std::string &object_id) {
  uint32_t key = esphome::fnv1_hash(object_id);
  auto *sw = App.get_switch_by_key(key);
  if (!sw) {
    ESP_LOGW(TAG, "Switch not found: %s (hash: %u)", object_id.c_str(), key);
  } else {
    ESP_LOGD(TAG, "Resolved switch: %s (hash: %u)", object_id.c_str(), key);
  }
  return sw;
}

light::LightState *JsonAutomationComponent::resolve_light(const std::string &object_id) {
  uint32_t key = esphome::fnv1_hash(object_id);
  auto *light = App.get_light_by_key(key);
  if (!light) {
    ESP_LOGW(TAG, "Light not found: %s (hash: %u)", object_id.c_str(), key);
  } else {
    ESP_LOGD(TAG, "Resolved light: %s (hash: %u)", object_id.c_str(), key);
  }
  return light;
}

Trigger<> *JsonAutomationComponent::create_trigger(const AutomationRule &rule) {
  if (rule.trigger.source == TriggerSource::INPUT) {
    if (rule.trigger.input_id.empty()) {
      ESP_LOGE(TAG, "Missing input_id for Input trigger");
      return nullptr;
    }

    auto *sensor = this->resolve_binary_sensor(rule.trigger.input_id);
    if (!sensor)
      return nullptr;

    if (rule.trigger.type == TriggerType::PRESS) {
      auto *trigger = new binary_sensor::PressTrigger(sensor);
      return trigger;
    } else if (rule.trigger.type == TriggerType::RELEASE) {
      auto *trigger = new binary_sensor::ReleaseTrigger(sensor);
      return trigger;
    }
  }

  ESP_LOGW(TAG, "Unsupported trigger configuration");
  ESP_LOGW(TAG, "Note: Only Input triggers with press/release are currently supported");
  return nullptr;
}

esphome::Action<> *JsonAutomationComponent::create_action(const Action &action) {
  if (action.source == ActionSource::SWITCH) {
    if (action.switch_id.empty()) {
      ESP_LOGW(TAG, "Missing switch_id for switch action");
      return nullptr;
    }

    auto *sw = this->resolve_switch(action.switch_id);
    if (!sw)
      return nullptr;

    if (action.type == ActionType::TURN_ON) {
      return new switch_::TurnOnAction<>(sw);
    } else if (action.type == ActionType::TURN_OFF) {
      return new switch_::TurnOffAction<>(sw);
    } else if (action.type == ActionType::TOGGLE) {
      return new switch_::ToggleAction<>(sw);
    }
  } else if (action.source == ActionSource::LIGHT) {
    if (action.switch_id.empty()) {
      ESP_LOGW(TAG, "Missing switch_id for light action");
      return nullptr;
    }

    auto *light = this->resolve_light(action.switch_id);
    if (!light)
      return nullptr;

    if (action.type == ActionType::TURN_ON) {
      auto *light_action = new light::LightControlAction<>(light);
      light_action->set_state(true);
      return light_action;
    } else if (action.type == ActionType::TURN_OFF) {
      auto *light_action = new light::LightControlAction<>(light);
      light_action->set_state(false);
      return light_action;
    } else if (action.type == ActionType::TOGGLE) {
      return new light::ToggleAction<>(light);
    }
  } else if (action.source == ActionSource::DELAY) {
    auto *delay_action = new esphome::DelayAction<>();
    delay_action->set_delay(action.delay_s * 1000);
    return delay_action;
  }

  ESP_LOGW(TAG, "Unsupported action configuration");
  return nullptr;
}

void JsonAutomationComponent::clear_automations() {
  ESP_LOGD(TAG, "Clearing %d existing automation objects", this->automation_objects_.size());
  this->automation_objects_.clear();
}

void JsonAutomationComponent::create_all_automations() {
  for (const auto &rule : this->automations_) {
    if (!this->create_automation_from_rule(rule)) {
      ESP_LOGW(TAG, "Failed to create automation: %s", rule.id.c_str());
    }
  }
}

bool JsonAutomationComponent::create_automation_from_rule(const AutomationRule &rule) {
  ESP_LOGD(TAG, "Creating automation: %s (%s)", rule.id.c_str(), rule.name.c_str());

  if (!rule.enabled) {
    ESP_LOGD(TAG, "Automation %s is disabled, skipping", rule.id.c_str());
    return true;
  }

  Trigger<> *trigger = this->create_trigger(rule);
  if (!trigger) {
    ESP_LOGE(TAG, "Failed to create trigger for automation: %s", rule.id.c_str());
    return false;
  }

  Automation<> *automation = new Automation<>(trigger);

  int action_count = 0;
  for (const auto &action : rule.actions) {
    esphome::Action<> *action_obj = this->create_action(action);
    if (action_obj) {
      automation->add_action(action_obj);
      action_count++;
    }
  }

  this->automation_objects_.push_back(std::unique_ptr<Automation<>>(automation));

  ESP_LOGI(TAG, "Successfully created automation: %s with %d actions", rule.id.c_str(), action_count);

  return true;
}

void JsonAutomationComponent::execute_automation(const std::string &automation_id) {
  ESP_LOGD(TAG, "Looking up automation: %s", automation_id.c_str());

  for (const auto &automation : this->automations_) {
    if (automation.id == automation_id) {
      ESP_LOGI(TAG, "Found automation %s (%s) with %d actions", automation_id.c_str(), automation.name.c_str(),
               automation.actions.size());

      ESP_LOGI(TAG, "Automation is active and will execute when triggered");
      ESP_LOGI(TAG, "Enabled: %s", automation.enabled ? "YES" : "NO");

      return;
    }
  }

  ESP_LOGW(TAG, "Automation not found: %s", automation_id.c_str());
}

void JsonAutomationComponent::add_on_automation_loaded_callback(std::function<void(std::string)> callback) {
  this->automation_loaded_callback_.add(std::move(callback));
}

void JsonAutomationComponent::add_on_json_error_callback(std::function<void(std::string)> callback) {
  this->json_error_callback_.add(std::move(callback));
}

void JsonAutomationComponent::trigger_automation_loaded(const std::string &data) {
  this->automation_loaded_callback_.call(data);
}

void JsonAutomationComponent::trigger_json_error(const std::string &error) { this->json_error_callback_.call(error); }

}  // namespace json_automation
}  // namespace esphome
