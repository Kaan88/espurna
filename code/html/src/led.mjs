import {
    addSimpleEnumerables,
    fromSchema,
    groupSettingsOnAdd,
    variableListeners,
} from './settings.mjs';

import { addFromTemplate } from './template.mjs';

function addNode(cfg) {
    addFromTemplate(document.getElementById("leds"), "led-config", cfg);
}

function listeners() {
    return {
        "ledConfig": (_, value) => {
            let container = document.getElementById("leds");
            if (container.childElementCount > 0) {
                return;
            }

            value.leds.forEach((entries) => {
                addNode(fromSchema(entries, value.schema));
            });

            addSimpleEnumerables("led", "LED", value.leds.length);
        },
    };
};

export function init() {
    variableListeners(listeners());

    groupSettingsOnAdd("leds", () => {
        addNode();
    });
}
