import { addFromTemplate } from './template.mjs';
import { groupSettingsOnAdd, variableListeners, fromSchema } from './settings.mjs';

function addNode(cfg) {
    addFromTemplate(document.getElementById("schedules"), "schedule-config", cfg);
}

function listeners() {
    return {
        "schConfig": (_, value) => {
            let container = document.getElementById("schedules");
            container.dataset["settingsMax"] = value.max;

            value.schedules.forEach((entries) => {
                addNode(fromSchema(entries, value.schema));
            });
        },
    };
}

export function init() {
    variableListeners(listeners());
    groupSettingsOnAdd("schedules", () => {
        addNode();
    });
}
