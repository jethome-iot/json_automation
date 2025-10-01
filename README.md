# ESPHome JSON Automation Component

An ESPHome external component that stores and parses JSON automation definitions with persistent flash storage. This component provides a structured way to store automation metadata that can be accessed via callbacks for custom logic implementation.

## Features

- **JSON-based automation storage**: Store automation definitions using JSON format
- **Persistent storage**: Save JSON configurations to flash memory (survives reboots, max 4KB)
- **Dynamic JSON loading**: Load and parse JSON at runtime or from preferences
- **Trigger callbacks**: Get notified when automations are loaded or errors occur
- **Automation lookup**: Query parsed automation data by ID
- **Structured metadata**: Store triggers, actions, and parameters in a queryable format

## Important Limitations

**This component does NOT execute ESPHome actions dynamically.** It stores and parses JSON automation definitions and provides callbacks with the parsed data. To use the parsed automations, you must implement custom logic in the `on_automation_loaded` callback.

ESPHome automations are compile-time constructs. This component is designed for storing automation metadata that your custom code can act upon.

## Installation

### Method 1: Local Component

1. Copy the `components` folder to your ESPHome project directory
2. Reference it in your YAML configuration:

```yaml
external_components:
  - source:
      type: local
      path: components
```

### Method 2: GitHub Repository

```yaml
external_components:
  - source: github://yourusername/esphome-json-automation
    components: [ json_automation ]
```

## Configuration

### Basic Setup

```yaml
preferences:
  flash_write_interval: 5min

json_automation:
  id: my_automations
  json_data: |
    {
      "automations": [
        {
          "id": "example_automation",
          "trigger": {
            "type": "binary_sensor",
            "condition": "on_press",
            "parameters": {
              "sensor_id": "my_button"
            }
          },
          "actions": [
            "light.turn_on: my_light"
          ]
        }
      ]
    }
```

### With Callbacks

```yaml
json_automation:
  id: my_automations
  on_automation_loaded:
    then:
      - logger.log: 
          format: "Loaded automations: %s"
          args: ['data.c_str()']
  on_json_error:
    then:
      - logger.log:
          format: "Error: %s"
          args: ['error.c_str()']
          level: ERROR
```

## JSON Structure

### Automation Format

```json
{
  "automations": [
    {
      "id": "unique_automation_id",
      "trigger": {
        "type": "trigger_type",
        "condition": "trigger_condition",
        "parameters": {
          "key": "value"
        }
      },
      "actions": [
        "simple_action_string",
        {
          "type": "action_type",
          "parameter": "value"
        }
      ]
    }
  ]
}
```

### Supported Trigger Types

- **binary_sensor**: For button presses, motion sensors, etc.
  - Conditions: `on_press`, `on_release`, `on_state`
  
- **sensor**: For temperature, humidity, and other numeric sensors
  - Conditions: `on_value`, `on_value_range`
  
- **time**: For scheduled automations
  - Conditions: `on_time`, `cron`

### Action Examples

```json
"actions": [
  "light.turn_on: living_room",
  "switch.toggle: fan",
  {
    "type": "delay",
    "duration": "5min"
  },
  {
    "type": "homeassistant.service",
    "service": "notify.mobile_app",
    "data": {
      "message": "Automation triggered!"
    }
  }
]
```

## Actions (YAML)

### Load JSON

Load new JSON automation configuration:

```yaml
button:
  - platform: template
    name: "Load Custom Automation"
    on_press:
      - json_automation.load_json:
          id: my_automations
          json_data: |
            {
              "automations": [...]
            }
```

### Save JSON

Save current configuration to preferences:

```yaml
- json_automation.save_json:
    id: my_automations
```

### Execute Automation (Lookup Only)

Look up a specific automation by ID (logs automation details, does not execute actions):

```yaml
- json_automation.execute:
    id: my_automations
    automation_id: "morning_routine"
```

**Note**: This action only looks up and logs the automation. It does not execute ESPHome actions. Use `on_automation_loaded` callbacks to implement custom logic based on the parsed data.

## Complete Examples

### Storing Automation Metadata

This example shows how to store automation definitions. The component parses and stores this data, which you can then use in callbacks:

```json
{
  "automations": [
    {
      "id": "morning_routine",
      "trigger": {
        "type": "time",
        "condition": "on_time",
        "parameters": {
          "hour": "7",
          "minute": "0"
        }
      },
      "actions": [
        "light.turn_on: bedroom_light",
        "switch.turn_on: coffee_maker"
      ]
    }
  ]
}
```

### Using Parsed Data in Callbacks

To actually use the parsed automation data, implement logic in callbacks:

```yaml
json_automation:
  id: my_automations
  json_data: '{"automations": [...]}'
  on_automation_loaded:
    then:
      - lambda: |-
          // Access parsed automations
          auto automations = id(my_automations).get_automations();
          for (const auto &automation : automations) {
            ESP_LOGI("main", "Loaded: %s", automation.id.c_str());
            ESP_LOGI("main", "  Trigger: %s", automation.trigger_type.c_str());
            
            // Implement your custom logic here based on the automation data
            if (automation.id == "morning_routine") {
              // Do something with this automation's data
            }
          }
```

### Metadata for External System Integration

Store automation configurations that external systems (like Home Assistant) can read:

```json
{
  "automations": [
    {
      "id": "security_motion",
      "trigger": {
        "type": "binary_sensor",
        "condition": "on_press",
        "parameters": {
          "sensor_id": "outdoor_motion"
        }
      },
      "actions": [
        "notify: Motion detected outdoors"
      ]
    }
  ]
}
```

## Component API

### C++ Methods

```cpp
void set_json_data(const std::string &json_data);
bool load_json_from_preferences();
bool save_json_to_preferences();
bool parse_json_automations(const std::string &json_data);
void execute_automation(const std::string &automation_id);  // Lookup only, does not execute
const std::vector<AutomationRule> &get_automations() const;  // Access parsed data
```

### Callbacks

- `on_automation_loaded`: Triggered when JSON is successfully parsed (provides parsed data)
- `on_json_error`: Triggered when JSON parsing fails (provides error message)

### Use Case: Custom Logic Implementation

Access the parsed automation data in your lambdas:

```yaml
lambda: |-
  auto automations = id(my_automations).get_automations();
  // Process automation metadata as needed
```

## Flash Memory Considerations

The component uses ESPHome's preferences system to store JSON in flash memory. Consider:

- **Write cycles**: Flash memory has limited write cycles (~100,000)
- **Write interval**: Configure `flash_write_interval` appropriately (default: 1min)
- **Data size**: JSON is limited to 4KB maximum. Both parsing and saving enforce this limit

```yaml
preferences:
  flash_write_interval: 5min  # Reduce flash wear
```

## Validation

Run the included validation script to check component structure:

```bash
python validate_component.py
```

This validates:
- Python configuration structure
- C++ header and implementation
- Example YAML configuration
- Example JSON structure

## Troubleshooting

### JSON Not Loading

- Check JSON syntax with a validator
- Enable debug logging: `logger: level: DEBUG`
- Verify the `automations` array exists in JSON

### Preferences Not Saving

- Ensure `preferences:` component is configured
- Check flash_write_interval setting
- Monitor flash writes to avoid wear

### Component Not Found

- Verify `external_components` path is correct
- Ensure `components` folder contains `json_automation` directory
- Check ESPHome compilation logs

## Development

### Project Structure

```
components/json_automation/
├── __init__.py              # Python config validation
├── json_automation.h        # C++ header
└── json_automation.cpp      # C++ implementation

example.yaml                 # Example ESPHome config
example_automation.json      # Example JSON automations
validate_component.py        # Component validator
```

### Building

This component is built using ESPHome's external component system:

1. Include in your ESPHome YAML configuration
2. Run `esphome compile your_config.yaml`
3. Upload to your ESP device

## License

This component is provided as-is for use with ESPHome projects.

## Contributing

Contributions are welcome! Please ensure:

- Code follows ESPHome coding standards
- Validation script passes
- Examples are updated for new features
- Documentation is clear and complete
