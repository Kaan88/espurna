import { sendAction } from './connection.mjs';

import {
    onPanelTargetClick,
    showPanelByName,
    styleInject,
    styleVisible,
} from './core.mjs';

import {
    initSelect,
    setChangedElement,
    setOriginalsFromValues,
    setSelectValue,
    variableListeners,
} from './settings.mjs';

import {
    fromSchema,
    loadConfigTemplate,
    loadTemplate,
    mergeTemplate,
    NumberInput,
} from './template.mjs';

/** @typedef {{name: string, units: number, type: number, index_global: number, description: string}} Magnitude */

/** @typedef {[number, string]} SupportedUnits */

const Magnitudes = {
    /** @type {Map<number, Magnitude>} */
    properties: new Map(),

    /** @type {Map<number, string>} */
    errors: new Map(),

    /** @type {Map<number, string>} */
    types: new Map(),

    units: {
        /** @type {Map<number, SupportedUnits[]>} */
        supported: new Map(),

        /** @type {Map<number, string>} */
        names: new Map(),
    },

    /** @type {Map<number, string>} */
    typePrefix: new Map(),

    /** @type {Map<string, number>} */
    prefixType: new Map(),
};

/**
 * @param {Magnitude} magnitude
 * @param {string} name
 */
function magnitudeTypedKey(magnitude, name) {
    const prefix = Magnitudes.typePrefix
        .get(magnitude.type);
    const index = magnitude.index_global;
    return `${prefix}${name}${index}`;
}

/**
 * @param {string} prefix
 * @param {any[][]} values
 * @param {string[]} schema
 */
function initModuleMagnitudes(prefix, values, schema) {
    const container = document.getElementById(`${prefix}-magnitudes`);
    if (!container || container.childElementCount > 0) {
        return;
    }

    values.forEach((value) => {
        const magnitude = fromSchema(value, schema);

        const type = /** @type {!number} */
            (magnitude.type);
        const index_global = /** @type {!number} */
            (magnitude.index_global);
        const index_module = /** @type {!number} */
            (magnitude.index_module);

        const line = loadConfigTemplate("module-magnitude");

        const label = /** @type {!HTMLLabelElement} */
            (line.querySelector("label"));
        label.textContent =
            `${Magnitudes.types.get(type) ?? "?"} #${magnitude.index_global}`;

        const span = /** @type {!HTMLSpanElement} */
            (line.querySelector("span"));
        span.textContent =
            Magnitudes.properties.get(index_global)?.description ?? "";

        const input = /** @type {!HTMLInputElement} */
            (line.querySelector("input"));
        input.name = `${prefix}Magnitude`;
        input.value = index_module.toString();
        input.dataset["original"] = input.value;

        mergeTemplate(container, line);
    });
}

/**
 * @param {any} types
 * @param {any} errors
 * @param {any} units
 */
function initMagnitudes(types, errors, units) {
    /** @type {[number, string, string][]} */
    (types.values).forEach((value) => {
        const info = fromSchema(value, types.schema);

        const type = /** @type {number} */(info.type);
        Magnitudes.types.set(type,
            /** @type {string} */(info.name));

        const prefix = /** @type {string} */(info.prefix);
        Magnitudes.typePrefix.set(type, prefix);
        Magnitudes.prefixType.set(prefix, type);
    });

    /** @type {[number, string][]} */
    (errors.values).forEach((value) => {
        const error = fromSchema(value, errors.schema);
        Magnitudes.errors.set(
            /** @type {number} */(error.type),
            /** @type {string} */(error.name));
    });

    /** @type {SupportedUnits[][]} */
    (units).forEach((value, id) => {
        Magnitudes.units.supported.set(id, value);
        value.forEach(([type, name]) => {
            Magnitudes.units.names.set(type, name);
        });
    });
}

/**
 * @typedef {function(number, Magnitude): void} MagnitudeCallback
 * @param {any[]} values
 * @param {string[]} schema
 * @param {MagnitudeCallback[]} callbacks
 */
function initMagnitudesList(values, schema, callbacks) {
    values.forEach((value, id) => {
        const magnitude = fromSchema(value, schema);

        const type = /** @type {number} */(magnitude.type);
        const prettyName =
            `${Magnitudes.types.get(type) ?? "?"} #${magnitude.index_global}`;

        /** @type {Magnitude} */
        const result = {
            name: prettyName,
            units: /** @type {number} */(magnitude.units),
            type: type,
            index_global: /** @type {number} */(magnitude.index_global),
            description: /** @type {string} */(magnitude.description),
        };

        Magnitudes.properties.set(id, result);
        callbacks.forEach((callback) => {
            callback(id, result);
        });
    });
}

/**
 * @param {number} id
 * @param {Magnitude} magnitude
 */
function createMagnitudeInfo(id, magnitude) {
    const container = document.getElementById("magnitudes");
    if (!container) {
        return;
    }

    const line = loadTemplate("magnitude-info");

    const label = /** @type {!HTMLLabelElement} */
        (line.querySelector("label"));
    label.textContent = magnitude.name;

    const input = /** @type {!HTMLInputElement} */
        (line.querySelector("input"));
    input.dataset["id"] = id.toString();
    input.dataset["type"] = magnitude.type.toString();

    const info = /** @type {!HTMLSpanElement} */
        (line.querySelector(".magnitude-info"));
    info.style.display = "none";

    const description = /** @type {!HTMLSpanElement} */
        (line.querySelector(".magnitude-description"));
    description.textContent = magnitude.description;

    mergeTemplate(container, line);
}

/**
 * @param {number} id
 * @param {Magnitude} magnitude
 */
function createMagnitudeUnitSelector(id, magnitude) {
    // but, no need for the element when there's no choice
    const supported = Magnitudes.units.supported.get(id);
    if ((supported === undefined) || (!supported.length)) {
        return;
    }

    const container = document.getElementById("magnitude-units");
    if (!container) {
        return;
    }

    const line = loadTemplate("magnitude-units");

    const label = /** @type {!HTMLLabelElement} */
        (line.querySelector("label"));
    label.textContent =
        `${Magnitudes.types.get(magnitude.type) ?? "?"} #${magnitude.index_global}`;

    const select = /** @type {!HTMLSelectElement} */
        (line.querySelector("select"));
    select.setAttribute("name",
        magnitudeTypedKey(magnitude, "Units"));

    /** @type {{id: number, name: string}[]} */
    const options = [];
    supported.forEach(([id, name]) => {
        options.push({id, name});
    });

    initSelect(select, options);
    setSelectValue(select, magnitude.units);
    setOriginalsFromValues([select]);

    container?.parentElement?.classList?.remove("maybe-hidden");
    mergeTemplate(container, line);
}

/**
 * @typedef {{id: number, name: string, key: string, prefix: string, index_global: number}} SettingInfo
 * @param {number} id
 * @param {string} suffix
 * @returns {SettingInfo | null}
 */
function magnitudeSettingInfo(id, suffix) {
    const props = Magnitudes.properties.get(id);
    if (!props) {
        return null;
    }

    const prefix = Magnitudes.typePrefix.get(props.type);
    if (!prefix) {
        return null;
    }

    const out = {
        id: id,
        name: props.name,
        key: `${prefix}${suffix}${props.index_global}`,
        prefix: prefix, 
        index_global: props.index_global,
    };


    return out;
}

/**
 * @param {number} id
 * @returns {SettingInfo | null}
 */
function emonRatioInfo(id) {
    return magnitudeSettingInfo(id, "Ratio");
}

/**
 * @param {string} containerId
 * @param {number} id
 * @param {string} keySuffix
 * @param {number} value
 */
function initMagnitudeNumberSetting(containerId, id, keySuffix, value) {
    const container = document.getElementById(containerId);
    if (!container) {
        return;
    }

    const info = magnitudeSettingInfo(id, keySuffix);
    if (!info) {
        return;
    }

    container?.parentElement?.classList?.remove("maybe-hidden");

    const template = new NumberInput();
    mergeTemplate(container, template.with(
        (label, input) => {
            label.textContent = info.name;
            label.htmlFor = input.id;

            input.id = info.key;
            input.name = input.id;
            input.value = value.toString();

            setOriginalsFromValues([input]);
        }));
}

/**
 * @param {number} id
 */
function initMagnitudesExpected(id) {
    const container = document.getElementById("emon-expected");
    if (!container) {
        return;
    }

    const info = emonRatioInfo(id);
    if (!info) {
        return;
    }

    // TODO: also display currently read value?
    const template = loadTemplate("emon-expected");

    const [expected, result] = template.querySelectorAll("input");
    expected.name += info.key;
    expected.id = expected.name;
    expected.dataset["id"] = info.id.toString();

    result.name += info.key;
    result.id = result.name;

    const [label] = template.querySelectorAll("label");
    label.textContent = info.name;
    label.htmlFor = expected.id;

    styleInject([
        styleVisible(`.emon-expected-${info.prefix}`, true)
    ]);

    mergeTemplate(container, template);
}

function emonCalculateRatios() {
    const expected = document.getElementById("emon-expected")
        ?.querySelectorAll("input.emon-expected-input");
    if (!expected) {
        return;
    }

    /** @type {NodeListOf<HTMLInputElement>} */
    (expected).forEach((input) => {
        if (!input.value || !input.dataset["id"]) {
            return;
        }

        sendAction("emon-expected", {
            id: parseInt(input.dataset["id"], 10),
            expected: parseFloat(input.value) });
    });
}

function emonApplyRatios() {
    const results = document.getElementById("emon-expected")
        ?.querySelectorAll("input.emon-expected-result");

    /** @type {NodeListOf<HTMLInputElement>} */
    (results).forEach((result) => {
        if (!result.value) {
            return;
        }

        let next = result.name
            .replace("result:", "");

        const ratio = document.getElementById(next);
        if (!(ratio instanceof HTMLInputElement)) {
            return;
        }

        ratio.value = result.value;
        setChangedElement(ratio);

        result.value = "";

        next = result.name
            .replace("result:", "expected:");
        const expected = document.getElementById(next);
        if (!(expected instanceof HTMLInputElement)) {
            return;
        }

        expected.value = "";
    });

    showPanelByName("sns");
}

/**
 * @param {any[]} values
 * @param {string[]} schema
 */
function initMagnitudesSettings(values, schema) {
    values.forEach((value, id) => {
        const settings = fromSchema(value, schema);

        if (typeof settings.Ratio === "number") {
            initMagnitudeNumberSetting(
                "emon-ratios", id,
                "Ratio", settings.Ratio);
            initMagnitudesExpected(id);
        }

        if (typeof settings.Correction === "number") {
            initMagnitudeNumberSetting(
                "magnitude-corrections", id,
                "Correction", settings.Correction);
        }

        const threshold =
            (typeof settings.ZeroThreshold === "number")
                ? settings.ZeroThreshold :
            (typeof settings.ZeroThreshold === "string")
                ? NaN : null;

        if (typeof threshold === "number") {
            initMagnitudeNumberSetting(
                "magnitude-zero-thresholds", id,
                "ZeroThreshold", threshold);
        }

        if (typeof settings.MinDelta === "number") {
            initMagnitudeNumberSetting(
                "magnitude-min-deltas", id,
                "MinDelta", settings.MinDelta);
        }

        if (typeof settings.MaxDelta === "number") {
            initMagnitudeNumberSetting(
                "magnitude-max-deltas", id,
                "MaxDelta", settings.MaxDelta);
        }
    });
}

/**
 * @param {number} id
 * @returns {HTMLInputElement | null}
 */
function magnitudeValueContainer(id) {
    return document.querySelector(`input[name='magnitude'][data-id='${id}']`);
}

/**
 * @param {any[]} values
 * @param {string[]} schema
 */
function updateMagnitudes(values, schema) {
    values.forEach((value, id) => {
        const props = Magnitudes.properties.get(id);
        if (!props) {
            return;
        }

        const input = magnitudeValueContainer(id);
        if (!input) {
            return;
        }

        const magnitude = fromSchema(value, schema);
        if (typeof magnitude.units === "number") {
            props.units = magnitude.units;
        }

        const units =
            Magnitudes.units.names.get(props.units) ?? "";

        if (typeof magnitude.error === "number" && 0 !== magnitude.error) {
            input.value =
                Magnitudes.errors.get(magnitude.error) ?? "Unknown error";
        } else if (typeof magnitude.value === "string") {
            input.value = `${magnitude.value}${units}`;
        } else {
            input.value = magnitude.value?.toString() ?? "?";
        }
    });
}

/**
 * @param {any[]} values
 * @param {string[]} schema
 */
function updateEnergy(values, schema) {
    values.forEach((value) => {
        const energy = fromSchema(value, schema);
        if (typeof energy.id !== "number") {
            return;
        }

        const input = magnitudeValueContainer(energy.id);
        if (!input) {
            return;
        }

        const props = Magnitudes.properties.get(energy.id);
        if (!props) {
            return;
        }

        if (typeof energy.saved !== "string" || !energy.saved) {
            return;
        }

        const info = input?.parentElement?.parentElement
            ?.querySelector(".magnitude-info");
        if (!(info instanceof HTMLElement)) {
            return;
        }

        info.style.display = "inherit";
        info.textContent = energy.saved;
    });
}

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
 */
function listeners() {
    return {
        "magnitudes-init": (_, value) => {
            initMagnitudes(
                value.types, value.errors, value.units);
        },
        "magnitudes-module": (_, value) => {
            initModuleMagnitudes(
                value.prefix, value.values, value.schema);
        },
        "magnitudes-list": (_, value) => {
            initMagnitudesList(value.values, value.schema, [
                createMagnitudeUnitSelector, createMagnitudeInfo]);
        },
        "magnitudes-settings": (_, value) => {
            initMagnitudesSettings(value.values, value.schema);
        },
        "magnitudes": (_, value) => {
            updateMagnitudes(value.values, value.schema);
        },
        "energy": (_, value) => {
            updateEnergy(value.values, value.schema);
        },
    };
}

export function init() {
    variableListeners(listeners());

    document.querySelector(".button-emon-expected")
        ?.addEventListener("click", onPanelTargetClick);
    document.querySelector(".button-emon-expected-calculate")
        ?.addEventListener("click", emonCalculateRatios);
    document.querySelector(".button-emon-expected-apply")
        ?.addEventListener("click", emonApplyRatios);
}
