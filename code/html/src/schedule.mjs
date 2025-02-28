import { addFromTemplate, addFromTemplateWithSchema } from './template.mjs';
import { addEnumerables, groupSettingsOnAddElem, variableListeners } from './settings.mjs';
import { reportValidityForInputOrSelect } from './validate.mjs';
import { capitalize } from './core.mjs';

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
            reportValidityForInputOrSelect(elems[id], message);
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
            const tuples =
                /** @type {import('./settings.mjs').EnumerableTuple[]} */(value);
            addEnumerables("schType",
                tuples.map((x) => [x[0], capitalize(x[1])]));
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
