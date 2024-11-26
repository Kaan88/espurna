import {
    addSimpleEnumerables,
    groupSettingsOnAddElem,
    variableListeners,
    addEnumerables,
} from './settings.mjs';

import { addFromTemplate, addFromTemplateWithSchema } from './template.mjs';

/** @param {function(HTMLElement): void} callback */
function withLeds(callback) {
    callback(/** @type {!HTMLElement} */
        (document.getElementById("leds")));
}

/**
 * @param {HTMLElement} elem
 */
function addLed(elem) {
    addFromTemplate(elem, "led-config", {});
}

/**
 * @param {any} value
 */
function onConfig(value) {
    withLeds((elem) => {
        addFromTemplateWithSchema(
            elem, "led-config",
            value.leds, value.schema,
            value.max ?? 0);
    });
    addSimpleEnumerables("led", "LED", value.leds.length);
}

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
 */
function listeners() {
    return {
        "ledConfig": (_, value) => {
            onConfig(value);
        },
        "ledModes": (_, value) => {
            addEnumerables("ledMode", value);
        },
    };
};

export function init() {
    withLeds((elem) => {
        variableListeners(listeners());
        groupSettingsOnAddElem(elem, () => {
            addLed(elem);
        });
    });
}
