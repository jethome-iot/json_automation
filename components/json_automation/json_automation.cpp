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
    ESP_LOGD(TAG, "Parsing initial JSON data");
    if (this->parse_json_automations(this->json_data_)) {
      this->save_json_to_preferences();
    }
  } else {
    ESP_LOGD(TAG, "Loading JSON data from preferences");
    this->load_json_from_preferences();
  }
}

void JsonAutomationComponent::loop() {
}

void JsonAutomationComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "JSON Automation Component:");
  ESP_LOGCONFIG(TAG, "  Number of automations: %d", this->automations_.size());
  
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
    ESP_LOGD(TAG, "Loaded JSON from preferences: %s", stored_json.c_str());
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

void JsonAutomationComponent::execute_automation(const std::string &automation_id) {
  ESP_LOGD(TAG, "Looking up automation: %s", automation_id.c_str());
  
  for (const auto &automation : this->automations_) {
    if (automation.id == automation_id) {
      ESP_LOGI(TAG, "Found automation %s with %d actions", 
               automation_id.c_str(), automation.actions.size());
      
      ESP_LOGW(TAG, "Note: This component only stores and parses automations.");
      ESP_LOGW(TAG, "Dynamic action execution is not implemented.");
      ESP_LOGW(TAG, "Use the parsed data in on_automation_loaded callbacks for custom logic.");
      
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
