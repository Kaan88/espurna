import { groupSettingsOnAdd, variableListeners, fromSchema } from './settings.mjs';
import { addFromTemplate } from './template.mjs';

function addRule(cfg) {
    addFromTemplate(document.getElementById("rpn-rules"), "rpn-rule", cfg);
}

function addTopic(cfg) {
    addFromTemplate(document.getElementById("rpn-topics"), "rpn-topic", cfg);
}

function listeners() {
    return {
        "rpnRules": (_, value) => {
            for (let rule of value) {
                addRule({"rpnRule": rule});
            }
        },
        "rpnTopics": (_, value) => {
            value.topics.forEach((topic) => {
                addTopic(fromSchema(topic, value.schema));
            });
        },
    };
}

export function init() {
    variableListeners(listeners());
    groupSettingsOnAdd("rpn-rules", () => {
        addRule();
    });
    groupSettingsOnAdd("rpn-topics", () => {
        addTopic();
    });
}
