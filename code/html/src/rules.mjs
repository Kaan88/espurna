import { groupSettingsOnAddElem, variableListeners } from './settings.mjs';
import { addFromTemplateWithSchema, addFromTemplate } from './template.mjs';

/** @param {function(HTMLElement): void} callback */
function withRules(callback) {
    callback(/** @type {!HTMLElement} */
        (document.getElementById("rpn-rules")));
}

/**
 * @param {HTMLElement} elem
 * @param {string} rule
 */
function addRule(elem, rule = "") {
    addFromTemplate(elem, "rpn-rule", {rpnRule: rule});
}

/** @param {function(HTMLElement): void} callback */
function withTopics(callback) {
    callback(/** @type {!HTMLElement} */
        (document.getElementById("rpn-topics")));
}

/** @param {HTMLElement} elem */
function addTopic(elem) {
    addFromTemplate(elem, "rpn-topic", {});
}

/**
 * @param {HTMLElement} elem
 * @param {any} value
 */
function addTopicWithSchema(elem, value) {
    addFromTemplateWithSchema(
        elem, "rpn-topic",
        value.topics, value.schema,
        value.max ?? 0);
}

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
 */
function listeners() {
    return {
        "rpnRules": (_, value) => {
            withRules((elem) => {
                for (let rule of value) {
                    addRule(elem, rule);
                }
            });
        },
        "rpnTopics": (_, value) => {
            withTopics((elem) => {
                addTopicWithSchema(elem, value);
            });
        },
    };
}

export function init() {
    variableListeners(listeners());
    withRules((elem) => {
        groupSettingsOnAddElem(elem, () => {
            addRule(elem);
        });
    });
    withTopics((elem) => {
        groupSettingsOnAddElem(elem, () => {
            addTopic(elem);
        });
    });
}
