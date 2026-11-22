module.exports = [
  {
    "type": "heading",
    "defaultValue": "Named Timer Settings"
  },
  {
    "type": "text",
    "defaultValue": "Customize the quick preset names shown on your watch, or send a new named timer straight to your Pebble."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Quick Preset Names"
      },
      {
        "type": "text",
        "defaultValue": "These names appear under Edit -> Preset Name on your watch. Leave any slot blank to hide it."
      },
      {
        "type": "input",
        "messageKey": "PRESET_0",
        "label": "Preset 1",
        "defaultValue": "Coffee",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Coffee" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_1",
        "label": "Preset 2",
        "defaultValue": "Laundry",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Laundry" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_2",
        "label": "Preset 3",
        "defaultValue": "Tea",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Tea" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_3",
        "label": "Preset 4",
        "defaultValue": "Cooking",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Cooking" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_4",
        "label": "Preset 5",
        "defaultValue": "Workout",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Workout" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_5",
        "label": "Preset 6",
        "defaultValue": "Focus",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Focus" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_6",
        "label": "Preset 7",
        "defaultValue": "Eggs",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Eggs" }
      },
      {
        "type": "input",
        "messageKey": "PRESET_7",
        "label": "Preset 8",
        "defaultValue": "Nap",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Nap" }
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Add Named Timer (Optional)"
      },
      {
        "type": "text",
        "defaultValue": "Set a duration above 0 to add a new named timer when you tap Save."
      },
      {
        "type": "input",
        "messageKey": "QUICK_ADD_NAME",
        "label": "Timer Name",
        "defaultValue": "",
        "attributes": { "maxlength": 23, "placeholder": "e.g. Pour Over" }
      },
      {
        "type": "slider",
        "messageKey": "QUICK_ADD_MINUTES",
        "label": "Minutes",
        "defaultValue": 0,
        "min": 0,
        "max": 120,
        "step": 1
      },
      {
        "type": "slider",
        "messageKey": "QUICK_ADD_SECONDS",
        "label": "Seconds",
        "defaultValue": 0,
        "min": 0,
        "max": 55,
        "step": 5
      },
      {
        "type": "toggle",
        "messageKey": "QUICK_ADD_START",
        "label": "Start Immediately",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
