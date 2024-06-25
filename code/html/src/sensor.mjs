import { sendAction } from './connection.mjs';
import { variableListeners } from './settings.mjs';

import {
    loadConfigTemplate,
    loadTemplate,
    mergeTemplate,
} from './template.mjs';

import {
    showPanel,
    showPanelByName,
    styleInject,
    styleVisible,
} from './core.mjs';

import {
    fromSchema,
    initSelect,
    setChangedElement,
    setOriginalsFromValuesForNode,
    setSelectValue,
} from './settings.mjs';

const Magnitudes = {
    properties: {},
    errors: {},
    types: {},
    units: {
        names: {},
        supported: {}
    },
    typePrefix: {},
    prefixType: {}
};

function magnitudeTypedKey(magnitude, name) {
    const prefix = Magnitudes.typePrefix[magnitude.type];
    const index = magnitude.index_global;
    return `${prefix}${name}${index}`;
}

function initModuleMagnitudes(data) {
    const targetId = `${data.prefix}-magnitudes`;

    let target = document.getElementById(targetId);
    if (target.childElementCount > 0) { return; }

    data.values.forEach((values) => {
        const entry = fromSchema(values, data.schema);

        let line = loadConfigTemplate("module-magnitude");
        line.querySelector("label").textContent =
            `${Magnitudes.types[entry.type]} #${entry.index_global}`;
        line.querySelector("span").textContent =
            Magnitudes.properties[entry.index_global].description;

        let input = line.querySelector("input");
        input.name = `${data.prefix}Magnitude`;
        input.value = entry.index_module;
        input.dataset["original"] = input.value;

        mergeTemplate(target, line);
    });
}

function initMagnitudes(data) {
    data.types.values.forEach((cfg) => {
        const info = fromSchema(cfg, data.types.schema);
        Magnitudes.types[info.type] = info.name;
        Magnitudes.typePrefix[info.type] = info.prefix;
        Magnitudes.prefixType[info.prefix] = info.type;
    });

    data.errors.values.forEach((cfg) => {
        const error = fromSchema(cfg, data.errors.schema);
        Magnitudes.errors[error.type] = error.name;
    });

    data.units.values.forEach((cfg, id) => {
        const values = fromSchema(cfg, data.units.schema);
        values.supported.forEach(([type, name]) => {
            Magnitudes.units.names[type] = name;
        });

        Magnitudes.units.supported[id] = values.supported;
    });
}

function initMagnitudesList(data, callbacks) {
    data.values.forEach((cfg, id) => {
        const magnitude = fromSchema(cfg, data.schema);
        const prettyName = Magnitudes.types[magnitude.type]
            .concat(" #").concat(parseInt(magnitude.index_global, 10));

        const result = {
            name: prettyName,
            units: magnitude.units,
            type: magnitude.type,
            index_global: magnitude.index_global,
            description: magnitude.description
        };

        Magnitudes.properties[id] = result;
        callbacks.forEach((callback) => {
            callback(id, result);
        });
    });
}

function createMagnitudeInfo(id, magnitude) {
    const container = document.getElementById("magnitudes");

    const info = loadTemplate("magnitude-info");
    const label = info.querySelector("label");
    label.textContent = magnitude.name;

    const input = info.querySelector("input");
    input.dataset["id"] = id;
    input.dataset["type"] = magnitude.type;

    const description = info.querySelector(".magnitude-description");
    description.textContent = magnitude.description;

    const extra = info.querySelector(".magnitude-info");
    extra.style.display = "none";

    mergeTemplate(container, info);
}

function createMagnitudeUnitSelector(id, magnitude) {
    // but, no need for the element when there's no choice
    const supported = Magnitudes.units.supported[id];
    if ((supported !== undefined) && (supported.length > 1)) {
        const line = loadTemplate("magnitude-units");
        line.querySelector("label").textContent =
            `${Magnitudes.types[magnitude.type]} #${magnitude.index_global}`;

        const select = line.querySelector("select");
        select.setAttribute("name", magnitudeTypedKey(magnitude, "Units"));

        const options = [];
        supported.forEach(([id, name]) => {
            options.push({id, name});
        });

        initSelect(select, options);
        setSelectValue(select, magnitude.units);
        setOriginalsFromValuesForNode(line, [select]);

        const container = document.getElementById("magnitude-units");
        container.parentElement.classList.remove("maybe-hidden");
        mergeTemplate(container, line);
    }
}

function magnitudeSettingInfo(id, key) {
    const out = {
        id: id,
        name: Magnitudes.properties[id].name,
        prefix: `${Magnitudes.typePrefix[Magnitudes.properties[id].type]}`,
        index_global: `${Magnitudes.properties[id].index_global}`
    };

    out.key = `${out.prefix}${key}${out.index_global}`;
    return out;
}

function emonRatioInfo(id) {
    return magnitudeSettingInfo(id, "Ratio");
}

function initMagnitudeTextSetting(containerId, id, keySuffix, value) {
    const template = loadTemplate("text-input");
    const input = template.querySelector("input");

    const info = magnitudeSettingInfo(id, keySuffix);
    input.id = info.key;
    input.name = input.id;
    input.value = value;
    setOriginalsFromValuesForNode(template, [input]);

    const label = template.querySelector("label");
    label.textContent = info.name;
    label.htmlFor = input.id;

    const container = document.getElementById(containerId);
    container.parentElement.classList.remove("maybe-hidden");
    mergeTemplate(container, template);
}

function initMagnitudesRatio(id, value) {
    initMagnitudeTextSetting("emon-ratios", id, "Ratio", value);
}

function initMagnitudesExpected(id) {
    // TODO: also display currently read value?
    const template = loadTemplate("emon-expected");
    const [expected, result] = template.querySelectorAll("input");

    const info = emonRatioInfo(id);

    expected.name += `${info.key}`;
    expected.id = expected.name;
    expected.dataset["id"] = info.id;

    result.name += `${info.key}`;
    result.id = result.name;

    const label = template.querySelector("label");
    label.textContent = info.name;
    label.htmlFor = expected.id;

    styleInject([
        styleVisible(`.emon-expected-${info.prefix}`, true)
    ]);

    mergeTemplate(document.getElementById("emon-expected"), template);
}

function emonCalculateRatios() {
    const inputs = document.getElementById("emon-expected")
        .querySelectorAll(".emon-expected-input");

    inputs.forEach((input) => {
        if (input.value.length) {
            sendAction("emon-expected", {
                id: parseInt(input.dataset["id"], 10),
                expected: parseFloat(input.value) });
        }
    });
}

function emonApplyRatios() {
    const results = document.getElementById("emon-expected")
        .querySelectorAll(".emon-expected-result");

    results.forEach((result) => {
        if (result.value.length) {
            const ratio = document.getElementById(
                result.name.replace("result:", ""));
            ratio.value = result.value;
            setChangedElement(ratio);

            result.value = "";

            const expected = document.getElementById(
                result.name.replace("result:", "expected:"));
            expected.value = "";
        }
    });

    showPanelByName("sns");
}

function initMagnitudesCorrection(id, value) {
    initMagnitudeTextSetting("magnitude-corrections", id, "Correction", value);
}

function initMagnitudesSettings(data) {
    data.values.forEach((cfg, id) => {
        const settings = fromSchema(cfg, data.schema);

        if (settings.Ratio !== null) {
            initMagnitudesRatio(id, settings.Ratio);
            initMagnitudesExpected(id);
        }

        if (settings.Correction !== null) {
            initMagnitudesCorrection(id, settings.Correction);
        }

        let threshold = settings.ZeroThreshold;
        if (threshold === null) {
            threshold = NaN;
        }

        initMagnitudeTextSetting(
            "magnitude-zero-thresholds", id,
            "ZeroThreshold", threshold);

        initMagnitudeTextSetting(
            "magnitude-min-deltas", id,
            "MinDelta", settings.MinDelta);

        initMagnitudeTextSetting(
            "magnitude-max-deltas", id,
            "MaxDelta", settings.MaxDelta);
    });
}

function magnitudeValueContainer(id) {
    return document.querySelector(`input[name='magnitude'][data-id='${id}']`);
}

function updateMagnitudes(data) {
    data.values.forEach((cfg, id) => {
        if (!Magnitudes.properties[id]) {
            return;
        }

        const magnitude = fromSchema(cfg, data.schema);
        const properties = Magnitudes.properties[id];
        properties.units = magnitude.units;

        const units = Magnitudes.units.names[properties.units] || "";
        const input = magnitudeValueContainer(id);
        input.value = (0 !== magnitude.error)
            ? Magnitudes.errors[magnitude.error]
            : (("nan" === magnitude.value)
                ? ""
                : `${magnitude.value}${units}`);
    });
}

function updateEnergy(data) {
    data.values.forEach((cfg) => {
        const energy = fromSchema(cfg, data.schema);
        if (!Magnitudes.properties[energy.id]) {
            return;
        }

        if (energy.saved.length) {
            const input = magnitudeValueContainer(energy.id);
            const info = input.parentElement.parentElement
                .querySelector(".magnitude-info");
            info.style.display = "inherit";
            info.textContent = energy.saved;
        }
    });
}

function listeners() {
    return {
        "magnitudes-init": (_, value) => {
            initMagnitudes(value);
        },
        "magnitudes-module": (_, value) => {
            initModuleMagnitudes(value);
        },
        "magnitudes-list": (_, value) => {
            initMagnitudesList(value, [
                createMagnitudeUnitSelector, createMagnitudeInfo]);
        },
        "magnitudes-settings": (_, value) => {
            initMagnitudesSettings(value);
        },
        "magnitudes": (_, value) => {
            updateMagnitudes(value);
        },
        "energy": (_, value) => {
            updateEnergy(value);
        },
    };
}

export function init() {
    variableListeners(listeners());

    document.querySelector(".button-emon-expected")
        .addEventListener("click", showPanel);
    document.querySelector(".button-emon-expected-calculate")
        .addEventListener("click", emonCalculateRatios);
    document.querySelector(".button-emon-expected-apply")
        .addEventListener("click", emonApplyRatios);
}
