#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/automation.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/switch/automation.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/automation.h"
#include <map>
#include <vector>
#include <memory>

namespace esphome {
namespace json_automation {

static const size_t MAX_JSON_SIZE = 4096;

enum class TriggerSource { INPUT, UNKNOWN };

enum class TriggerType { PRESS, RELEASE, UNKNOWN };

enum class ActionSource { SWITCH, DELAY, LIGHT, UNKNOWN };

enum class ActionType { TURN_ON, TURN_OFF, TOGGLE, UNKNOWN };

struct Trigger {
  TriggerSource source;
  TriggerType type;
  std::string input_id;

  Trigger() : source(TriggerSource::UNKNOWN), type(TriggerType::UNKNOWN), input_id("") {}
};

struct Action {
  ActionSource source;
  ActionType type;
  std::string switch_id;
  uint32_t delay_s;

  Action() : source(ActionSource::UNKNOWN), type(ActionType::UNKNOWN), delay_s(0) {}
};

struct AutomationRule {
  std::string id;
  std::string name;
  bool enabled;
  Trigger trigger;
  std::vector<Action> actions;

  AutomationRule() : enabled(true) {}
};

class JsonAutomationComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_json_data(const std::string &json_data);
  bool load_json_from_preferences();
  bool save_json_to_preferences();
  bool parse_json_automations(const std::string &json_data);

  void execute_automation(const std::string &automation_id);

  void add_on_automation_loaded_callback(std::function<void(std::string)> callback);
  void add_on_json_error_callback(std::function<void(std::string)> callback);

  const std::vector<AutomationRule> &get_automations() const { return automations_; }

 protected:
  std::string json_data_;
  std::vector<AutomationRule> automations_;
  ESPPreferenceObject pref_;

  CallbackManager<void(std::string)> automation_loaded_callback_;
  CallbackManager<void(std::string)> json_error_callback_;

  std::vector<std::unique_ptr<Automation<>>> automation_objects_;

  void trigger_automation_loaded(const std::string &data);
  void trigger_json_error(const std::string &error);

  bool create_automation_from_rule(const AutomationRule &rule);
  void clear_automations();
  void create_all_automations();

  binary_sensor::BinarySensor *resolve_binary_sensor(const std::string &object_id);
  switch_::Switch *resolve_switch(const std::string &object_id);
  light::LightState *resolve_light(const std::string &object_id);

  Trigger<> *create_trigger(const AutomationRule &rule);
  esphome::Action<> *create_action(const Action &action);

  TriggerSource parse_trigger_source(const std::string &source);
  TriggerType parse_trigger_type(const std::string &type);
  ActionSource parse_action_source(const std::string &source);
  ActionType parse_action_type(const std::string &type);
};

class AutomationLoadedTrigger : public Trigger<std::string> {
 public:
  explicit AutomationLoadedTrigger(JsonAutomationComponent *parent) {
    parent->add_on_automation_loaded_callback([this](std::string data) { this->trigger(data); });
  }
};

class JsonErrorTrigger : public Trigger<std::string> {
 public:
  explicit JsonErrorTrigger(JsonAutomationComponent *parent) {
    parent->add_on_json_error_callback([this](std::string error) { this->trigger(error); });
  }
};

template<typename... Ts> class LoadJsonAction : public Action<Ts...> {
 public:
  LoadJsonAction(JsonAutomationComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, json_data)

  void play(Ts... x) override {
    auto json_data = this->json_data_.value(x...);
    this->parent_->clear_automations();
    this->parent_->set_json_data(json_data);
    if (this->parent_->parse_json_automations(json_data)) {
      auto automations = this->parent_->get_automations();
      for (const auto &rule : automations) {
        this->parent_->create_automation_from_rule(rule);
      }
    }
  }

 protected:
  JsonAutomationComponent *parent_;
};

template<typename... Ts> class SaveJsonAction : public Action<Ts...> {
 public:
  SaveJsonAction(JsonAutomationComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->save_json_to_preferences(); }

 protected:
  JsonAutomationComponent *parent_;
};

template<typename... Ts> class ExecuteAutomationAction : public Action<Ts...> {
 public:
  ExecuteAutomationAction(JsonAutomationComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, automation_id)

  void play(Ts... x) override {
    auto automation_id = this->automation_id_.value(x...);
    this->parent_->execute_automation(automation_id);
  }

 protected:
  JsonAutomationComponent *parent_;
};

}  // namespace json_automation
}  // namespace esphome
