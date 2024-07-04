import { default as iro } from '@jaames/iro';
import { styleInject, styleVisible } from './core.mjs';
import { sendAction } from './connection.mjs';
import { mergeTemplate, loadTemplate } from './template.mjs';
import { addEnumerables, variableListeners } from './settings.mjs';

/** @param {function(HTMLElement): void} callback */
function withPicker(callback) {
    const elem = document.getElementById("light-picker");
    if (elem) {
        callback(elem);
    }
}

/**
 * @param {iro.Color} color
 * @returns {string}
 */
function colorToHsvString(color) {
    const hsv =
        [color.hsv.h, color.hsv.s, color.hsv.v]
        .filter((value) => value !== undefined)
        .map(Math.round);

    if (hsv.length !== 3) {
        return "0,0,0";
    }

    return hsv.join(",");
}

/**
 * @param {string} string
 */
function hsvStringToColor(string) {
    const parts = string.split(",")
        .map(parseInt);

    return {
        h: parts[0] ?? 0,
        s: parts[1] ?? 0,
        v: parts[2] ?? 0,
    };
}

/** @param {string} type */
function colorSlider(type) {
    return {component: iro.ui.Slider, options: {sliderType: type}};
}

function colorWheel() {
    return {component: iro.ui.Wheel, options: {}};
}

function colorBox() {
    return {component: iro.ui.Box, options: {}};
}

/**
 * @param {"rgb" | "hsv"} mode
 * @param {string} value
 */
function colorUpdate(mode, value) {
    withPicker((elem) => {
        elem.dispatchEvent(
            new CustomEvent("color:string", {
                detail: {
                    mode: mode,
                    value: value,
                }}));
    });
}

/** @param {number} id */
function lightStateHideRelay(id) {
    styleInject([
        styleVisible(`.relay-control-${id}`, false)
    ]);
}

/** @param {function(HTMLInputElement): void} callback */
function withState(callback) {
    const elem = document.getElementById("light-state-value");
    if (elem instanceof HTMLInputElement) {
        callback(elem);
    }
}

function initLightState() {
    withState((elem) => {
        elem.addEventListener("change", (event) => {
            event.preventDefault();
            sendAction("light", {state: /** @type {!HTMLInputElement} */(event.target).checked});
        });

    });
}

/** @param {boolean} value */
function colorPickerState(value) {
    const light = /** @type {!HTMLElement} */
        (document.getElementById("light"));
    if (value) {
        light.classList.add("light-on");
    } else {
        light.classList.remove("light-on");
    }
}

/** @param {boolean} value */
function updateLightState(value) {
    withState((elem) => {
        elem.checked = value;
        colorPickerState(value);
    });
}

/** @param {boolean} value */
function colorEnabled(value) {
    if (value) {
        lightAddClass("light-color");
    }
}

/** @param {boolean} value */
function colorInit(value) {
    // TODO: ref. #2451, input:change causes pretty fast updates.
    // either make sure we don't cause any issue on the esp, or switch to
    // color:change instead (which applies after input ends)

    /** @type {function(iro.Color): void} */
    let change = () => {
    };

    /** @type {string[]} */
    const rules = [];

    /** @type {{"component": any, "options": any}[]} */
    const layout = [];

    // RGB
    if (value) {
        layout.push(colorWheel());
        change = (color) => {
            sendAction("light", {
                rgb: color.hexString
            });
        };
    // HSV
    } else {
        layout.push(colorBox());
        layout.push(colorSlider("hue"));
        layout.push(colorSlider("saturation"));
        change = (color) => {
            sendAction("light", {
                hsv: colorToHsvString(color)
            });
        };
    }

    layout.push(colorSlider("value"));
    styleInject(rules);

    withPicker((elem) => {
        // TODO tsserver does not like the resulting type
        const picker = new /** @type {any} */
            (iro.ColorPicker(elem, {layout}));
        picker.on("input:change", change);

        elem.addEventListener("color:string", (event) => {
            const color = /** @type {CustomEvent<{mode: string, value: string}>} */
                (event).detail;
            switch (color.mode) {
            case "rgb":
                picker.color.hexString = color.value;
                break;
            case "hsv":
                picker.color.hsv = hsvStringToColor(color.value);
                break;
            }
        });
    });
}

/** @param {function(HTMLInputElement): void} callback */
function withMiredsValue(callback) {
    const elem = document.getElementById("mireds-value");
    if (elem instanceof HTMLInputElement) {
        callback(elem);
    }
}

/**
 * @param {HTMLElement} elem
 * @param {string} text
 */
function textForNextSibling(elem, text) {
    const next = elem.nextElementSibling;
    if (!next) {
        return;
    }

    next.textContent = text;
}

/**
 * @param {HTMLInputElement} elem
 * @param {number} value
 */
function setMiredsValue(elem, value) {
    elem.value = value.toString();
    textForNextSibling(elem, elem.value);
}

/** @param {number} value */
function updateMireds(value) {
    withMiredsValue((elem) => {
        setMiredsValue(elem, value);
    });
}

/** @param {string} className */
function lightAddClass(className) {
    const light = document.getElementById("light");
    light?.classList?.add(className);
}

/**
 * implies we should hide one or both white channels
 * @param {boolean} value
 */
function whiteEnabled(value) {
    if (value) {
        lightAddClass("light-white");
    }
}

/**
 * no need for raw white channel sliders with cct
 * @param {boolean} value
 */
function cctEnabled(value) {
    if (value) {
        lightAddClass("light-cct");
    }
}

/** @param {{cold: number, warm: number}} value */
function cctInit(value) {
    const control = loadTemplate("mireds-control");

    withMiredsValue((elem) => {
        elem.setAttribute("min", value.cold.toString());
        elem.setAttribute("max", value.warm.toString());
        elem.addEventListener("change", () => {
            textForNextSibling(elem, elem.value);
            sendAction("light", {mireds: elem.value});
        });

    });

    const [datalist] = control.querySelectorAll("datalist");
    datalist.innerHTML = `
        <option value="${value.cold}">Cold</option>
        <option value="${value.warm}">Warm</option>`;

    mergeTemplate(
        /** @type {HTMLElement} */
        (document.getElementById("light-cct")), control);
}

/** @param {{[k: string]: any}} data */
function updateLight(data) {
    for (const [key, value] of Object.entries(data)) {
        switch (key) {
        case "state":
            updateLightState(value);
            break;

        case "state_relay_id":
            lightStateHideRelay(value);
            break;

        case "channels":
            initLightState();
            initBrightness();
            initChannels(value);
            break;

        case "cct":
            cctInit(value);
            break;

        case "brightness":
            updateBrightness(value);
            break;

        case "values":
            updateChannels(value);
            break;

        case "rgb":
        case "hsv":
            colorUpdate(key, value);
            break;

        case "mireds":
            updateMireds(value);
            break;
        }
    }
}

/** @param {Event} event */
function onChannelSliderChange(event) {
    if (!(event.target instanceof HTMLInputElement)) {
        return;
    }

    const target = event.target;
    textForNextSibling(target, target.value);
    
    const id = target.dataset["id"];
    if (!id) {
        return;
    }

    sendAction("light", {
        channel: {
            [id]: target.value
        }
    });
}

/** @param {Event} event */
function onBrightnessSliderChange(event) {
    if (!(event.target instanceof HTMLInputElement)) {
        return;
    }

    textForNextSibling(event.target, event.target.value);
    sendAction("light", {brightness: event.target.value});
}

function initBrightness() {
    const template = loadTemplate("brightness-control");

    const elem = template.getElementById("brightness-value");
    elem?.addEventListener("change", onBrightnessSliderChange);

    mergeTemplate(
        /** @type {!HTMLElement} */
        (document.getElementById("light-brightness")), template);
}

/** @param {number} value */
function updateBrightness(value) {
    const elem = document.getElementById("brightness-value");
    if (elem instanceof HTMLInputElement) {
        elem.value = value.toString();
        textForNextSibling(elem, elem.value);
    }
}

/** @param {string[]} channels */
function initChannels(channels) {
    const container = document.getElementById("light-channels");
    if (!container) {
        return;
    }

    /** @type {import('./settings.mjs').EnumerableEntry[]} */
    const enumerables = [];

    channels.forEach((tag, channel) => {
        const line = loadTemplate("channel-control");

        const [root] = line.querySelectorAll("div");
        root.setAttribute("id", `light-channel-${tag.toLowerCase()}`);

        const name =
            `Channel #${channel} (${tag.toUpperCase()})`;

        const [label] = line.querySelectorAll("label");
        label.textContent = name;

        enumerables.push({"id": channel, "name": name});

        const [span] = line.querySelectorAll("span");
        span.dataset["id"] = channel.toString();

        const [slider] = line.querySelectorAll("input");
        slider.dataset["id"] = channel.toString();
        slider.addEventListener("change", onChannelSliderChange);

        mergeTemplate(container, line);
    });

    addEnumerables("Channels", enumerables);
}

/** @param {number[]} values */
function updateChannels(values) {
    const container = document.getElementById("light");
    if (!container) {
        return;
    }

    values.forEach((value, channel) => {
        const slider = container.querySelector(`input.slider[data-id='${channel}']`);
        if (!(slider instanceof HTMLInputElement)) {
            return;
        }

        slider.value = value.toString();
        textForNextSibling(slider, slider.value);
    });
}

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
 */
function listeners() {
    return {
        "light": (_, value) => {
            updateLight(value);
        },
        "useWhite": (_, value) => {
            whiteEnabled(value);
        },
        "useCCT": (_, value) => {
            cctEnabled(value);
        },
        "useColor": (_, value) => {
            colorEnabled(value);
        },
        "useRGB": (_, value) => {
            colorInit(value);
        },
    };
}

export function init() {
    variableListeners(listeners());
}
