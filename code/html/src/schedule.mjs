import { showPanelByName } from './core.mjs';
import { addFromTemplate, addFromTemplateWithSchema } from './template.mjs';
import { addEnumerables, groupSettingsOnAddElem, variableListeners } from './settings.mjs';

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
 * @param {[number, string, string]} value
 */
function onValidate(value) {
    withSchedules((elem) => {
        const [id, key, message] = value;
        const elems = /** @type {NodeListOf<HTMLInputElement>} */
            (elem.querySelectorAll(`input[name=${key}]`));

        if (id < elems.length) {
            showPanelByName("sch");

            const target = elems[id];
            target.focus();
            target.setCustomValidity(message);
            target.reportValidity();
        }
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
        "schValidate": (_, value) => {
            onValidate(value);
        },
        "schTypes": (_, value) => {
            addEnumerables("schType", value);
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
