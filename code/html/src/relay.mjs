import {
    addEnumerables,
    fromSchema,
    getEnumerables,
    variableListeners,
} from './settings.mjs';

import { sendAction } from './connection.mjs';

import {
    loadConfigTemplate,
    mergeTemplate,
    fillTemplateLineFromCfg,
} from './template.mjs';

function onToggle(event) {
    event.preventDefault();
    sendAction("relay", {
        id: parseInt(event.target.dataset["id"], 10),
        status: event.target.checked ? "1" : "0"});
}

function initToggle(id, cfg) {
    let line = loadConfigTemplate("relay-control");

    let root = line.querySelector("div");
    root.classList.add(`relay-control-${id}`);

    let name = line.querySelector("span[data-key='relayName']");
    name.textContent = cfg.relayName;
    name.dataset["id"] = id;
    name.setAttribute("title", cfg.relayProv);

    let realId = "relay".concat(id);

    let toggle = line.querySelector("input[type='checkbox']");
    toggle.checked = false;
    toggle.disabled = true;
    toggle.dataset["id"] = id;
    toggle.addEventListener("change", onToggle);

    toggle.setAttribute("id", realId);
    toggle.previousElementSibling.setAttribute("for", realId);

    mergeTemplate(document.getElementById("relays"), line);
}

function updateState(data) {
    data.states.forEach((state, id) => {
        const relay = fromSchema(state, data.schema);

        let elem = document.querySelector(`input[name='relay'][data-id='${id}']`);
        elem.checked = relay.status;
        elem.disabled = ({
            0: false,
            1: !relay.status,
            2: relay.status
        })[relay.lock]; // TODO: specify lock statuses earlier?
    });
}

function initConfig(id, cfg) {
    let line = loadConfigTemplate("relay-config");
    fillTemplateLineFromCfg(line, id, cfg);
    mergeTemplate(document.getElementById("relayConfig"), line);
}

function listeners() {
    return {
        "relayConfig": (_, value) => {
            let container = document.getElementById("relays");
            if (container.childElementCount > 0) {
                return;
            }

            let relays = [];
            value.relays.forEach((entries, id) => {
                let cfg = fromSchema(entries, value.schema);
                if (!cfg.relayName || !cfg.relayName.length) {
                    cfg.relayName = `Switch #${id}`;
                }

                relays.push({
                    "id": id,
                    "name": `${cfg.relayName} (${cfg.relayProv})`
                });

                initToggle(id, cfg);
                initConfig(id, cfg);
            });

            addEnumerables("relay", relays);
        },
        "relayState": (_, value) => {
            updateState(value);
        },
    };
}

export function createNodeList(containerId, values, keyPrefix) {
    const target = document.getElementById(containerId);
    if (target.childElementCount > 0) {
        return;
    }

    // TODO: let schema set the settings key
    const template = loadConfigTemplate("number-input");
    values.forEach((value, index) => {
        const line = template.cloneNode(true);

        const enumerables = getEnumerables("relay");
        line.querySelector("label").textContent = (enumerables)
            ? enumerables[index].name : `Switch #${index}`;

        const input = line.querySelector("input");
        input.name = keyPrefix;
        input.value = value;
        input.dataset["original"] = value;

        mergeTemplate(target, line);
    });
}

export function init() {
    variableListeners(listeners());
}
