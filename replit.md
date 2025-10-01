# ESPHome JSON Automation Component

## Overview

This is an ESPHome external component that enables storing and parsing JSON-based automation definitions with persistent flash storage. The component acts as a metadata storage layer for automation configurations, allowing runtime loading and parsing of automation definitions stored in JSON format. 

**Critical Design Note:** This component does NOT execute ESPHome actions dynamically. It only stores, parses, and provides callbacks with automation metadata. ESPHome automations are compile-time constructs, so this component is designed for storing automation metadata that custom code can interpret and act upon.

## User Preferences

Preferred communication style: Simple, everyday language.

## System Architecture

### Component Structure

**ESPHome External Component Pattern**: The project follows ESPHome's external component architecture with a dual Python/C++ implementation:

- **Python Configuration Layer** (`__init__.py`): Handles YAML schema validation and code generation during ESPHome compilation
- **C++ Runtime Layer** (`json_automation.h` and `json_automation.cpp`): Implements the actual component logic that runs on the ESP device

This separation allows ESPHome to validate configurations at compile time while keeping the runtime footprint minimal on resource-constrained ESP devices.

### Data Storage Strategy

**Persistent Flash Storage**: Automation definitions are stored in ESP flash memory (4KB maximum enforced) using ESPHome's preferences system, allowing configurations to survive device reboots. The storage mechanism uses:

- `flash_write_interval`: Configurable write batching to minimize flash wear
- JSON format for human-readable and parseable storage
- Runtime JSON parsing capability for dynamic configuration loading
- Size validation on both parse and save operations to prevent flash issues

### Event-Driven Architecture

**Trigger-Based Notification System**: The component uses ESPHome's automation/trigger framework to notify other components when events occur:

- `AutomationLoadedTrigger`: Fires when automation JSON is successfully parsed, passing the automation data as a string
- `JsonErrorTrigger`: Fires when JSON parsing or loading fails, passing error messages

This allows other custom components or ESPHome automations to react to configuration changes or errors without tight coupling.

### Action Framework

**Three Core Actions**:
- `LoadJsonAction`: Loads JSON from a source (runtime or flash) and triggers parsing
- `SaveJsonAction`: Persists JSON automation definitions to flash storage
- `ExecuteAutomationAction`: Provides a hook for custom code to interpret and act on parsed automation metadata

These actions integrate with ESPHome's automation system, enabling YAML-defined workflows to manage JSON configurations.

### Automation Metadata Model

**Structured JSON Schema**: Automations are defined using a structured format containing:

- **id**: Unique identifier for automation lookup
- **trigger**: Defines when the automation should conceptually activate (type, condition, parameters)
- **actions**: Array of action definitions with types and parameters

Example structure supports time-based triggers, sensor triggers, and various action types including delays, service calls, and device control commands. The component parses this structure but delegates execution logic to implementing code.

### Component Registration

**ESPHome Component Lifecycle**: Uses standard ESPHome component registration (`cg.register_component`) to integrate with the device's setup/loop cycle, allowing the component to initialize during device boot and participate in the standard ESPHome component lifecycle.

## External Dependencies

### ESPHome Framework

- **esphome.codegen**: Code generation utilities for C++ code emission during compilation
- **esphome.config_validation**: YAML schema validation framework
- **esphome.automation**: Trigger and action base classes for event handling
- **esphome.const**: Standard constant definitions (CONF_ID, CONF_TRIGGER_ID)

### ESP Platform

- **Flash Storage**: Utilizes ESP device flash memory via ESPHome preferences API
- **JSON Parsing**: Uses ArduinoJson library via ESPHome's json::parse_json utility

### Build System

- **Python 3**: Validation script and component configuration written in Python 3
- **ESPHome Compiler**: Required to compile YAML configurations into C++ firmware

No external web services, databases, or third-party APIs are required. The component operates entirely on-device.

## Recent Changes

### October 2025 - Component Implementation Complete

- **Fixed compile-time issues**: Corrected Python-to-C++ action code generation to match constructor signatures
- **Added size validation**: Enforced 4KB maximum JSON size on both parse and save operations to prevent flash issues
- **Clarified execution model**: Updated documentation and code to clearly indicate component stores/parses metadata only, does not execute actions dynamically
- **Validation tooling**: Created `validate_component.py` script to verify component structure and syntax
- **Example configurations**: Added `example.yaml` and `example_automation.json` to demonstrate usage patterns
- **Complete documentation**: Created comprehensive README.md with accurate capability descriptions and usage examples

The component is now production-ready for its stated purpose: storing and parsing JSON automation metadata with persistent flash storage and callback-based custom logic integration.