#!/usr/bin/env python3
"""
ESPHome JSON Automation Component Validator

This script validates the structure and syntax of the JSON Automation component.
"""

import os
import sys
import json
import re

def validate_python_file(filepath):
    """Validate Python configuration file exists and has basic structure"""
    print(f"Validating Python file: {filepath}")
    
    if not os.path.exists(filepath):
        print(f"  ❌ File not found: {filepath}")
        return False
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    required_imports = ['esphome.codegen', 'esphome.config_validation', 'from esphome import automation']
    for imp in required_imports:
        if imp not in content:
            print(f"  ❌ Missing import: {imp}")
            return False
    
    required_elements = ['CONFIG_SCHEMA', 'to_code']
    for elem in required_elements:
        if elem not in content:
            print(f"  ❌ Missing required element: {elem}")
            return False
    
    print(f"  ✅ Python file structure is valid")
    return True

def validate_cpp_header(filepath):
    """Validate C++ header file exists and has basic structure"""
    print(f"Validating C++ header: {filepath}")
    
    if not os.path.exists(filepath):
        print(f"  ❌ File not found: {filepath}")
        return False
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    required_includes = ['esphome/core/component.h', 'esphome/core/automation.h']
    for inc in required_includes:
        if inc not in content:
            print(f"  ❌ Missing include: {inc}")
            return False
    
    if 'class JsonAutomationComponent : public Component' not in content:
        print(f"  ❌ Missing main component class")
        return False
    
    print(f"  ✅ C++ header structure is valid")
    return True

def validate_cpp_implementation(filepath):
    """Validate C++ implementation file exists and has basic structure"""
    print(f"Validating C++ implementation: {filepath}")
    
    if not os.path.exists(filepath):
        print(f"  ❌ File not found: {filepath}")
        return False
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    required_methods = ['setup()', 'loop()', 'dump_config()', 'parse_json_automations']
    for method in required_methods:
        if method not in content:
            print(f"  ❌ Missing method: {method}")
            return False
    
    print(f"  ✅ C++ implementation structure is valid")
    return True

def validate_example_yaml(filepath):
    """Validate example YAML configuration"""
    print(f"Validating example YAML: {filepath}")
    
    if not os.path.exists(filepath):
        print(f"  ❌ File not found: {filepath}")
        return False
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    required_sections = ['esphome:', 'json_automation:', 'external_components:']
    for section in required_sections:
        if section not in content:
            print(f"  ❌ Missing section: {section}")
            return False
    
    print(f"  ✅ Example YAML structure is valid")
    return True

def validate_example_json(filepath):
    """Validate example JSON automation file"""
    print(f"Validating example JSON: {filepath}")
    
    if not os.path.exists(filepath):
        print(f"  ❌ File not found: {filepath}")
        return False
    
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        
        if 'automations' not in data:
            print(f"  ❌ Missing 'automations' key")
            return False
        
        if not isinstance(data['automations'], list):
            print(f"  ❌ 'automations' must be an array")
            return False
        
        for i, automation in enumerate(data['automations']):
            required_fields = ['id', 'trigger', 'actions']
            for field in required_fields:
                if field not in automation:
                    print(f"  ❌ Automation {i} missing field: {field}")
                    return False
        
        print(f"  ✅ Example JSON structure is valid ({len(data['automations'])} automations)")
        return True
    
    except json.JSONDecodeError as e:
        print(f"  ❌ Invalid JSON: {e}")
        return False

def main():
    """Main validation function"""
    print("=" * 60)
    print("ESPHome JSON Automation Component Validator")
    print("=" * 60)
    print()
    
    all_valid = True
    
    all_valid &= validate_python_file('components/json_automation/__init__.py')
    print()
    
    all_valid &= validate_cpp_header('components/json_automation/json_automation.h')
    print()
    
    all_valid &= validate_cpp_implementation('components/json_automation/json_automation.cpp')
    print()
    
    all_valid &= validate_example_yaml('example.yaml')
    print()
    
    all_valid &= validate_example_json('example_automation.json')
    print()
    
    print("=" * 60)
    if all_valid:
        print("✅ All validation checks passed!")
        print()
        print("Component is ready to use. To use this component:")
        print("1. Copy the 'components' folder to your ESPHome project")
        print("2. Reference it in your YAML with 'external_components'")
        print("3. Configure the 'json_automation' component")
        print("4. Compile and upload to your ESP device")
        return 0
    else:
        print("❌ Some validation checks failed")
        return 1

if __name__ == '__main__':
    sys.exit(main())
