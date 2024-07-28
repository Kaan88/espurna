import globals from "globals";
import js from "@eslint/js";

export default [
    {
        files: ["gulpfile.mjs"],
        languageOptions: {
            "globals": {
                ...globals.es2021,
                ...globals.node,
            }
        },
        rules: {
            ...js.configs.recommended.rules,
            "quotes": ["error", "single"],
            "no-throw-literal": "error",
            "no-unused-vars": ["error", {
                "argsIgnorePattern": "^_",
            }],
        }
    },
    {
        files: ["html/src/**/*.mjs"],
        languageOptions: {
            "globals": {
                ...globals.es2021,
                ...globals.browser,
                "MODULE_API": "readonly",
                "MODULE_CMD": "readonly",
                "MODULE_CURTAIN": "readonly",
                "MODULE_DBG": "readonly",
                "MODULE_DCZ": "readonly",
                "MODULE_GARLAND": "readonly",
                "MODULE_HA": "readonly",
                "MODULE_LED": "readonly",
                "MODULE_LIGHT": "readonly",
                "MODULE_LIGHTFOX": "readonly",
                "MODULE_LOCAL": "readonly",
                "MODULE_OTA": "readonly",
                "MODULE_RELAY": "readonly",
                "MODULE_RFB": "readonly",
                "MODULE_RFM69": "readonly",
                "MODULE_RPN": "readonly",
                "MODULE_SCH": "readonly",
                "MODULE_SNS": "readonly",
                "MODULE_THERMOSTAT": "readonly",
                "MODULE_TSPK": "readonly",
            }
        },
        rules: {
            ...js.configs.recommended.rules,
            "no-invalid-this": "error",
            "eqeqeq": "error",
            "prefer-arrow-callback": "error"
        }
    }
];
