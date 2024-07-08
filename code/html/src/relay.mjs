import { sendAction } from './connection.mjs';

import {
    addFromTemplate,
    fromSchema,
    loadTemplate,
    mergeTemplate,
    NumberInput,
} from './template.mjs';

import {
    addEnumerables,
    getEnumerables,
    setOriginalsFromValues,
    variableListeners,
} from './settings.mjs';

/** @param {Event} event */
function onToggle(event) {
    event.preventDefault();

    const target = /** @type {!HTMLInputElement} */(event.target);
    const id = target.dataset["id"];
    if (!id) {
        return;
    }

    sendAction("relay", {
        id: parseInt(id, 10),
        status: target.checked ? "1" : "0"});
}

/**
 * @param {number} id
 * @param {any} cfg
 */
function initToggle(id, cfg) {
    const container = document.getElementById("relays");
    if (!container) {
        return;
    }

    const line = loadTemplate("relay-control");

    const root = /** @type {!HTMLDivElement} */
        (line.querySelector("div"));
    root.classList.add(`relay-control-${id}`);

    const name = /** @type {!HTMLSpanElement} */
        (line.querySelector("span[data-key='relayName']"));
    name.textContent = cfg.relayName;
    name.dataset["id"] = id.toString();
    name.setAttribute("title", cfg.relayProv);

    const toggle = /** @type {!HTMLInputElement} */
        (line.querySelector("input[type='checkbox']"));
    toggle.checked = false;
    toggle.disabled = true;
    toggle.dataset["id"] = id.toString();
    toggle.addEventListener("change", onToggle);

    const realId = `relay${id}`;
    toggle.setAttribute("id", realId);
    toggle.previousElementSibling?.setAttribute("for", realId);
    
    mergeTemplate(container, line);
}

/**
 * @param {any[]} states
 * @param {string[]} schema
 */
function updateFromState(states, schema) {
    states.forEach((state, id) => {
        const elem = /** @type {!HTMLInputElement} */
            (document.querySelector(`input[name='relay'][data-id='${id}']`));

        const relay = fromSchema(state, schema);

        const status = /** @type {boolean} */(relay.status);
        elem.checked = status;

        // TODO: publish possible enum values in ws init
        const as_lock = new Map();
        as_lock.set(0, false);
        as_lock.set(1, !status);
        as_lock.set(2, status);

        const lock = /** @type {number} */(relay.lock);
        if (as_lock.has(lock)) {
            elem.disabled = as_lock.get(lock);
        }
    });
}

/** @param {any} cfg */
function addConfigNode(cfg) {
    const container = /** @type {!HTMLElement} */
        (document.getElementById("relayConfig"));
    addFromTemplate(container, "relay-config", cfg);
}

/**
 * @param {any[]} configs
 * @param {string[]} schema
 */
function updateFromConfig(configs, schema) {
    const container = document.getElementById("relays");
    if (!container || container.childElementCount > 0) {
        return;
    }

    /** @type {import('./settings.mjs').EnumerableEntry[]} */
    const relays = [];

    configs.forEach((config, id) => {
        const relay = fromSchema(config, schema);
        if (!relay.relayName) {
            relay.relayName = `Switch #${id}`;
        }

        relays.push({
            "id": id,
            "name": `${relay.relayName} (${relay.relayProv})`
        });

        initToggle(id, relay);
        addConfigNode(relay);
    });

    addEnumerables("relay", relays);
}

/**
 * @param {string} id
 * @param {any[]} values
 * @param {string} keyPrefix
 */
export function createNodeList(id, values, keyPrefix) {
    const container = document.getElementById(id);
    if (!container || container.childElementCount > 0) {
        return;
    }

    // TODO generic template to automatically match elems to (limited) schema?
    const template = new NumberInput();
    values.forEach((value, index) => {
        mergeTemplate(container, template.with(
            (label, input) => {
                const enumerables = getEnumerables("relay");
                label.textContent =
                    (enumerables[index])
                        ? enumerables[index].name
                        : `Switch #${index}`;

                input.name = keyPrefix;
                input.value = value;
                setOriginalsFromValues([input]);
            }));
        });
}

/** @returns {import('./settings.mjs').KeyValueListeners} */
function listeners() {
    return {
        "relayConfig": (_, value) => {
            updateFromConfig(value.values, value.schema);
        },
        "relayState": (_, value) => {
            updateFromState(value.values, value.schema);
        },
    };
}

export function init() {
    variableListeners(listeners());
}
