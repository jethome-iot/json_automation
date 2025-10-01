# ESPHome JSON Automation Component

An ESPHome external component that creates **real, executable ESPHome automations at runtime** by parsing JSON automation definitions stored in flash preferences. The component dynamically instantiates ESPHome trigger and action classes, resolves entities by their object IDs, and wires them together into functioning automations.

## Features

- **Dynamic automation creation**: Creates real ESPHome `Automation<>` objects at runtime from JSON
- **Entity resolution**: Resolves binary sensors, switches, and lights using ESPHome's object ID registry
- **Real trigger classes**: Instantiates `PressTrigger`, `ReleaseTrigger` from JSON definitions
- **Real action classes**: Creates `TurnOnAction`, `TurnOffAction`, `ToggleAction` for switches and lights
- **Persistent storage**: Saves JSON configurations to flash memory (survives reboots, max 4KB)
- **Runtime updates**: Load new JSON and recreate all automations on-the-fly
- **Safe memory management**: Uses `unique_ptr` for proper lifecycle management

## How It Works

Unlike typical ESPHome automations that are defined at compile time, this component:

1. **Parses JSON** automation definitions at runtime
2. **Resolves entities** using `fnv1_hash(object_id)` and `App.get_*_by_key()`
3. **Creates trigger objects** (e.g., `binary_sensor::PressTrigger`)
4. **Creates action objects** (e.g., `light::TurnOnAction`)
5. **Wires them together** into `Automation<>` objects that execute when triggers fire

The automations function exactly like compile-time ESPHome automations, but are created dynamically from JSON stored in flash.

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

binary_sensor:
  - platform: gpio
    id: button
    pin: GPIO0

switch:
  - platform: gpio
    id: fan
    pin: GPIO12

light:
  - platform: binary
    id: bedroom_light
    output: light_output

json_automation:
  id: my_automations
  json_data: |
    {
      "automations": [
        {
          "id": "button_press_light",
          "trigger": {
            "type": "binary_sensor",
            "condition": "on_press",
            "parameters": {
              "object_id": "button"
            }
          },
          "actions": [
            "light.turn_on: bedroom_light"
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
          format: "Created %d automations from JSON"
          args: ['data.c_str()']
  on_json_error:
    then:
      - logger.log:
          format: "JSON Error: %s"
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
        "type": "binary_sensor",
        "condition": "on_press",
        "parameters": {
          "object_id": "button_object_id"
        }
      },
      "actions": [
        "switch.turn_on: fan_object_id",
        "light.toggle: light_object_id"
      ]
    }
  ]
}
```

## Supported Features

### Trigger Types

Currently supported:

- **binary_sensor**
  - `on_press`: Creates `binary_sensor::PressTrigger`
  - `on_release`: Creates `binary_sensor::ReleaseTrigger`

### Action Types

Currently supported:

- **Switch actions**: `switch.turn_on`, `switch.turn_off`, `switch.toggle`
- **Light actions**: `light.turn_on`, `light.turn_off`, `light.toggle`

### Entity Resolution

Entities are resolved by their `object_id` using ESPHome's hash-based registry:

```json
"parameters": {
  "object_id": "my_button"
}
```

The component:
1. Calculates `fnv1_hash("my_button")`
2. Looks up the entity using `App.get_binary_sensor_by_key(hash)`
3. Uses the resolved entity to create the trigger/action

## Actions (YAML)

### Load JSON

Load new JSON and recreate all automations:

```yaml
button:
  - platform: template
    name: "Load Custom Automation"
    on_press:
      - json_automation.load_json:
          id: my_automations
          json_data: |
            {
              "automations": [
                {
                  "id": "new_automation",
                  "trigger": {
                    "type": "binary_sensor",
                    "condition": "on_press",
                    "parameters": {
                      "object_id": "button"
                    }
                  },
                  "actions": [
                    "switch.toggle: fan"
                  ]
                }
              ]
            }
```

This will:
1. Clear all existing automations
2. Parse the new JSON
3. Create new automation objects
4. Wire them to triggers and actions

### Save JSON

Save current configuration to flash preferences:

```yaml
- json_automation.save_json:
    id: my_automations
```

## Complete Examples

### Button Controls Light and Fan

```json
{
  "automations": [
    {
      "id": "button_press_light",
      "trigger": {
        "type": "binary_sensor",
        "condition": "on_press",
        "parameters": {
          "object_id": "button"
        }
      },
      "actions": [
        "light.turn_on: bedroom_light"
      ]
    },
    {
      "id": "button_release_light",
      "trigger": {
        "type": "binary_sensor",
        "condition": "on_release",
        "parameters": {
          "object_id": "button"
        }
      },
      "actions": [
        "light.turn_off: bedroom_light",
        "switch.toggle: fan"
      ]
    }
  ]
}
```

### Required YAML Configuration

```yaml
esphome:
  name: json-automation-example

esp32:
  board: esp32dev

logger:
  level: DEBUG

api:
  encryption:
    key: "your-encryption-key"

ota:
  - platform: esphome
    password: "your-ota-password"

wifi:
  ssid: "your-ssid"
  password: "your-password"

preferences:
  flash_write_interval: 5min

external_components:
  - source:
      type: local
      path: components

binary_sensor:
  - platform: gpio
    id: button
    pin: 
      number: GPIO0
      inverted: true
      mode: INPUT_PULLUP

switch:
  - platform: gpio
    id: fan
    pin: GPIO12

output:
  - platform: gpio
    id: light_output
    pin: GPIO13

light:
  - platform: binary
    id: bedroom_light
    output: light_output

json_automation:
  id: my_automations
  json_data: |
    {
      "automations": [...]
    }
```

## Flash Memory Considerations

The component uses ESPHome's preferences system to store JSON in flash memory:

- **Write cycles**: Flash memory has limited write cycles (~100,000)
- **Write interval**: Configure `flash_write_interval` appropriately (default: 1min)
- **Data size**: JSON is limited to 4KB maximum to protect flash memory

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

## Technical Details

### Runtime Automation Creation

The component creates real ESPHome automation objects at runtime:

```cpp
// Simplified pseudo-code of what happens internally
auto *trigger = new binary_sensor::PressTrigger(sensor);
auto *action = new light::TurnOnAction<>(light);
auto *automation = new Automation<>(trigger);
automation->add_action(action);
```

### Entity Resolution

Entities are resolved using ESPHome's object ID hashing:

```cpp
uint32_t hash = esphome::fnv1_hash(object_id);
auto *sensor = App.get_binary_sensor_by_key(hash);
```

### Memory Management

The component uses `unique_ptr` for safe memory management:

```cpp
std::vector<std::unique_ptr<Automation<>>> automation_objects_;
```

When `clear_automations()` is called, the vector is cleared and all automations are properly destroyed.

### Setup Priority

The component uses `LATE` setup priority to ensure all entities are registered before attempting resolution:

```cpp
float get_setup_priority() const override { 
  return setup_priority::LATE; 
}
```

## Limitations

### Current Restrictions

- **Triggers**: Only `binary_sensor` triggers (`on_press`, `on_release`)
- **Actions**: Only string-based actions (switch/light control)
- **No object actions**: Delay, lambdas, and complex actions not supported
- **No parameters**: Actions don't support additional parameters (brightness, color, etc.)

These limitations keep the implementation simple and avoid template complexity. Future versions may expand support.

### Why These Limitations?

ESPHome uses heavy template metaprogramming. Creating templated objects at runtime requires knowing types at compile time. The current implementation focuses on:

1. **Non-templated triggers**: `Trigger<>` (binary sensor state changes)
2. **Non-templated actions**: `Action<>` (simple on/off/toggle)
3. **Simple actions**: Avoiding lambdas and complex parameter passing

This provides a working dynamic automation system while keeping the code maintainable.

## Troubleshooting

### Automation Not Executing

1. **Check entity resolution**: Ensure `object_id` matches exactly (case-sensitive)
2. **Enable debug logging**: Set `logger: level: DEBUG`
3. **Check logs for errors**: Look for "Failed to resolve" messages
4. **Verify entities exist**: Ensure binary sensors/switches/lights are defined

### JSON Not Loading

- Check JSON syntax with a validator
- Verify the `automations` array exists in JSON
- Check for size limit (4KB maximum)
- Look for parsing errors in logs

### Compile Errors

- Ensure ESPHome version is recent (tested with 2024.x)
- Check that all referenced entities are defined
- Verify external component path is correct

## Development

### Project Structure

```
components/json_automation/
├── __init__.py              # Python config validation & code generation
├── json_automation.h        # C++ header with class definition
└── json_automation.cpp      # C++ implementation with factories

example.yaml                 # Example ESPHome config
example_automation.json      # Example JSON automations
validate_component.py        # Component validator
```

### How It Works Internally

1. **Compile time** (`__init__.py`): 
   - Validates YAML configuration
   - Generates C++ code registration

2. **Boot time** (`setup()`):
   - Loads JSON from preferences or uses provided json_data
   - Parses JSON to extract automation definitions
   - Calls `create_all_automations()` to build runtime objects

3. **Runtime** (`create_all_automations()`):
   - Iterates through parsed automation rules
   - Creates trigger objects via `create_trigger_from_rule()`
   - Creates action objects via `create_action_from_string()`
   - Wires triggers and actions into `Automation<>` objects
   - Stores automations in `automation_objects_` vector

4. **Update time** (`LoadJsonAction`):
   - Calls `clear_automations()` to destroy old objects
   - Parses new JSON
   - Creates new automation objects

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
