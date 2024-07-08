import { addFromTemplate, addFromTemplateWithSchema } from './template.mjs';
import { groupSettingsOnAddElem, variableListeners } from './settings.mjs';

/** @param {function(HTMLElement): void} callback */
function withSchedules(callback) {
    callback(/** @type {!HTMLElement} */
        (document.getElementById("schedules")));
}

/**
 * @param {HTMLElement} elem
 */
function scheduleAdd(elem) {
    addFromTemplate(elem, "schedule-config", {});
}

/**
 * @param {any} value
 */
function onConfig(value) {
    withSchedules((elem) => {
        addFromTemplateWithSchema(
            elem, "schedule-config",
            value.schedules, value.schema,
            value.max ?? 0);
    });
}

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
 */
function listeners() {
    return {
        "schConfig": (_, value) => {
            onConfig(value);
        },
    };
}

export function init() {
    withSchedules((elem) => {
        variableListeners(listeners());
        groupSettingsOnAddElem(elem, () => {
            scheduleAdd(elem);
        });
    });
}
