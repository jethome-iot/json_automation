#include "json_automation.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace json_automation {

static const char *const TAG = "json_automation";

void JsonAutomationComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up JSON Automation Component...");
  
  this->pref_ = global_preferences->make_preference<std::string>(this->get_object_id_hash());
  
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

void JsonAutomationComponent::loop() {
}

void JsonAutomationComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "JSON Automation Component:");
  ESP_LOGCONFIG(TAG, "  Number of parsed automations: %d", this->automations_.size());
  ESP_LOGCONFIG(TAG, "  Active automation objects: %d", this->automation_objects_.size());
  
  for (const auto &automation : this->automations_) {
    ESP_LOGCONFIG(TAG, "  Automation ID: %s", automation.id.c_str());
    ESP_LOGCONFIG(TAG, "    Trigger: %s (%s)", automation.trigger_type.c_str(), 
                  automation.trigger_condition.c_str());
    ESP_LOGCONFIG(TAG, "    Actions: %d", automation.actions.size());
  }
}

void JsonAutomationComponent::set_json_data(const std::string &json_data) {
  this->json_data_ = json_data;
}

bool JsonAutomationComponent::load_json_from_preferences() {
  std::string stored_json;
  if (this->pref_.load(&stored_json)) {
    ESP_LOGD(TAG, "Loaded JSON from preferences");
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
  
  if (this->json_data_.size() > MAX_JSON_SIZE) {
    ESP_LOGE(TAG, "Cannot save: JSON data too large: %d bytes (max: %d)", 
             this->json_data_.size(), MAX_JSON_SIZE);
    this->trigger_json_error("JSON data exceeds maximum size for saving");
    return false;
  }
  
  if (this->pref_.save(&this->json_data_)) {
    ESP_LOGD(TAG, "JSON data saved to preferences (%d bytes)", this->json_data_.size());
    return true;
  }
  
  ESP_LOGE(TAG, "Failed to save JSON data to preferences");
  return false;
}

bool JsonAutomationComponent::validate_json_structure(JsonObject root) {
  if (!root.containsKey("automations")) {
    ESP_LOGE(TAG, "JSON missing 'automations' array");
    this->trigger_json_error("Missing 'automations' array in JSON");
    return false;
  }
  
  if (!root["automations"].is<JsonArray>()) {
    ESP_LOGE(TAG, "'automations' is not an array");
    this->trigger_json_error("'automations' must be an array");
    return false;
  }
  
  return true;
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
    if (!this->validate_json_structure(root)) {
      return false;
    }
    
    JsonArray automations = root["automations"];
    
    for (JsonVariant automation_var : automations) {
      JsonObject automation_obj = automation_var.as<JsonObject>();
      
      if (!automation_obj.containsKey("id") || 
          !automation_obj.containsKey("trigger") || 
          !automation_obj.containsKey("actions")) {
        ESP_LOGW(TAG, "Skipping invalid automation: missing required fields");
        continue;
      }
      
      AutomationRule rule;
      rule.id = automation_obj["id"].as<std::string>();
      
      JsonObject trigger = automation_obj["trigger"];
      rule.trigger_type = trigger["type"].as<std::string>();
      rule.trigger_condition = trigger.containsKey("condition") ? 
                              trigger["condition"].as<std::string>() : "";
      
      if (trigger.containsKey("parameters")) {
        JsonObject params = trigger["parameters"];
        for (JsonPair kv : params) {
          rule.parameters[kv.key().c_str()] = kv.value().as<std::string>();
        }
      }
      
      JsonArray actions = automation_obj["actions"];
      for (JsonVariant action_var : actions) {
        if (action_var.is<const char*>()) {
          rule.actions.push_back(action_var.as<std::string>());
        } else if (action_var.is<JsonObject>()) {
          JsonObject action_obj = action_var.as<JsonObject>();
          std::string action_str = json::build_json([&action_obj](JsonObject root) {
            for (JsonPair kv : action_obj) {
              root[kv.key()] = kv.value();
            }
          });
          rule.actions.push_back(action_str);
        }
      }
      
      this->automations_.push_back(rule);
      ESP_LOGD(TAG, "Loaded automation: %s", rule.id.c_str());
    }
    
    return true;
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
  if (rule.trigger_type == "binary_sensor") {
    if (rule.parameters.count("object_id") == 0) {
      ESP_LOGE(TAG, "Missing object_id parameter for binary_sensor trigger");
      return nullptr;
    }
    
    auto object_id = rule.parameters.at("object_id");
    auto *sensor = this->resolve_binary_sensor(object_id);
    if (!sensor) return nullptr;
    
    if (rule.trigger_condition == "on_press") {
      auto *trigger = new binary_sensor::PressTrigger(sensor);
      return trigger;
    } else if (rule.trigger_condition == "on_release") {
      auto *trigger = new binary_sensor::ReleaseTrigger(sensor);
      return trigger;
    }
  }
  
  ESP_LOGW(TAG, "Unsupported trigger type: %s/%s", rule.trigger_type.c_str(), rule.trigger_condition.c_str());
  ESP_LOGW(TAG, "Note: Only binary_sensor triggers are currently supported");
  return nullptr;
}

Action<> *JsonAutomationComponent::create_action(const std::string &action_str, const AutomationRule &rule) {
  size_t colon_pos = action_str.find(':');
  if (colon_pos == std::string::npos) {
    ESP_LOGW(TAG, "Invalid action format: %s", action_str.c_str());
    return nullptr;
  }
  
  std::string action_type = action_str.substr(0, colon_pos);
  std::string object_id = action_str.substr(colon_pos + 1);
  
  object_id.erase(0, object_id.find_first_not_of(" \t"));
  object_id.erase(object_id.find_last_not_of(" \t") + 1);
  
  if (action_type == "switch.turn_on") {
    auto *sw = this->resolve_switch(object_id);
    if (!sw) return nullptr;
    auto *action = new switch_::TurnOnAction<>(sw);
    return action;
  } else if (action_type == "switch.turn_off") {
    auto *sw = this->resolve_switch(object_id);
    if (!sw) return nullptr;
    auto *action = new switch_::TurnOffAction<>(sw);
    return action;
  } else if (action_type == "switch.toggle") {
    auto *sw = this->resolve_switch(object_id);
    if (!sw) return nullptr;
    auto *action = new switch_::ToggleAction<>(sw);
    return action;
  } else if (action_type == "light.turn_on") {
    auto *light = this->resolve_light(object_id);
    if (!light) return nullptr;
    auto *action = new light::TurnOnAction<>(light);
    return action;
  } else if (action_type == "light.turn_off") {
    auto *light = this->resolve_light(object_id);
    if (!light) return nullptr;
    auto *action = new light::TurnOffAction<>(light);
    return action;
  } else if (action_type == "light.toggle") {
    auto *light = this->resolve_light(object_id);
    if (!light) return nullptr;
    auto *action = new light::ToggleAction<>(light);
    return action;
  }
  
  ESP_LOGW(TAG, "Unsupported action type: %s", action_type.c_str());
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
  ESP_LOGD(TAG, "Creating automation: %s", rule.id.c_str());
  
  Trigger<> *trigger = this->create_trigger(rule);
  if (!trigger) {
    ESP_LOGE(TAG, "Failed to create trigger for automation: %s", rule.id.c_str());
    return false;
  }
  
  Automation<> *automation = new Automation<>(trigger);
  
  int action_count = 0;
  for (const auto &action_str : rule.actions) {
    Action<> *action = this->create_action(action_str, rule);
    if (action) {
      automation->add_action(action);
      action_count++;
    }
  }
  
  this->automation_objects_.push_back(std::unique_ptr<Automation<>>(automation));
  
  ESP_LOGI(TAG, "Successfully created automation: %s with %d actions", 
           rule.id.c_str(), action_count);
  
  return true;
}

void JsonAutomationComponent::execute_automation(const std::string &automation_id) {
  ESP_LOGD(TAG, "Looking up automation: %s", automation_id.c_str());
  
  for (const auto &automation : this->automations_) {
    if (automation.id == automation_id) {
      ESP_LOGI(TAG, "Found automation %s with %d actions", 
               automation_id.c_str(), automation.actions.size());
      
      ESP_LOGI(TAG, "Automation is active and will execute when triggered");
      
      for (const auto &action : automation.actions) {
        ESP_LOGD(TAG, "Action in automation: %s", action.c_str());
      }
      
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

void JsonAutomationComponent::trigger_json_error(const std::string &error) {
  this->json_error_callback_.call(error);
}

}  // namespace json_automation
}  // namespace esphome
