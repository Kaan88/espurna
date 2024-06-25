import { default as iro } from '@jaames/iro';
import { styleInject, styleVisible } from './core.mjs';
import { sendAction } from './connection.mjs';
import { mergeTemplate, loadTemplate } from './template.mjs';
import { addEnumerables, variableListeners } from './settings.mjs';

let ColorPicker = null;

function colorToHsvString(color) {
    var h = String(Math.round(color.hsv.h));
    var s = String(Math.round(color.hsv.s));
    var v = String(Math.round(color.hsv.v));
    return h + "," + s + "," + v;
}

function hsvStringToColor(hsv) {
    var parts = hsv.split(",");
    return {
        h: parseInt(parts[0]),
        s: parseInt(parts[1]),
        v: parseInt(parts[2])
    };
}

function colorSlider(type) {
    return {component: iro.ui.Slider, options: {sliderType: type}};
}

function colorWheel() {
    return {component: iro.ui.Wheel, options: {}};
}

function colorBox() {
    return {component: iro.ui.Box, options: {}};
}

function colorUpdate(mode, value) {
    if ("rgb" === mode) {
        ColorPicker.color.hexString = value;
    } else if ("hsv" === mode) {
        ColorPicker.color.hsv = hsvStringToColor(value);
    }
}

function lightStateHideRelay(id) {
    styleInject([
        styleVisible(`.relay-control-${id}`, false)
    ]);
}

function initLightState() {
    const toggle = document.getElementById("light-state-value");
    toggle.addEventListener("change", (event) => {
        event.preventDefault();
        sendAction("light", {state: event.target.checked});
    });
}

function updateLightState(value) {
    const state = document.getElementById("light-state-value");
    state.checked = value;
    colorPickerState(value);
}

function colorPickerState(value) {
    const light = document.getElementById("light");
    if (value) {
        light.classList.add("light-on");
    } else {
        light.classList.remove("light-on");
    }
}

function colorEnabled(value) {
    if (value) {
        lightAddClass("light-color");
    }
}

function colorInit(value) {
    // TODO: ref. #2451, input:change causes pretty fast updates.
    // either make sure we don't cause any issue on the esp, or switch to
    // color:change instead (which applies after input ends)
    let change = () => {
    };

    const rules = [];
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

    ColorPicker = new iro.ColorPicker("#light-picker", {layout});
    ColorPicker.on("input:change", change);
}

function updateMireds(value) {
    const mireds = document.getElementById("mireds-value");
    if (mireds !== null) {
        mireds.value = value;
        mireds.nextElementSibling.textContent = value;
    }
}

function lightAddClass(className) {
    const light = document.getElementById("light");
    light.classList.add(className);
}

// White implies we should hide one or both white channels
function whiteEnabled(value) {
    if (value) {
        lightAddClass("light-white");
    }
}

// When there are CCT controls, no need for raw white channel sliders
function cctEnabled(value) {
    if (value) {
        lightAddClass("light-cct");
    }
}

function cctInit(value) {
    const control = loadTemplate("mireds-control");

    const slider = control.getElementById("mireds-value");
    slider.setAttribute("min", value.cold);
    slider.setAttribute("max", value.warm);
    slider.addEventListener("change", (event) => {
        event.target.nextElementSibling.textContent = event.target.value;
        sendAction("light", {mireds: event.target.value});
    });

    const datalist = control.querySelector("datalist");
    datalist.innerHTML = `
    <option value="${value.cold}">Cold</option>
    <option value="${value.warm}">Warm</option>
    `;

    mergeTemplate(document.getElementById("light-cct"), control);
}

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

function onChannelSliderChange(event) {
    event.target.nextElementSibling.textContent = event.target.value;

    let channel = {}
    channel[event.target.dataset["id"]] = event.target.value;

    sendAction("light", {channel});
}

function onBrightnessSliderChange(event) {
    event.target.nextElementSibling.textContent = event.target.value;
    sendAction("light", {brightness: event.target.value});
}

function initBrightness() {
    const template = loadTemplate("brightness-control");

    const slider = template.getElementById("brightness-value");
    slider.addEventListener("change", onBrightnessSliderChange);

    mergeTemplate(document.getElementById("light-brightness"), template);
}

function updateBrightness(value) {
    const brightness = document.getElementById("brightness-value");
    if (brightness !== null) {
        brightness.value = value;
        brightness.nextElementSibling.textContent = value;
    }
}

function initChannels(channels) {
    const container = document.getElementById("light-channels");
    const enumerables = [];

    channels.forEach((tag, channel) => {
        const line = loadTemplate("channel-control");
        line.querySelector("span.slider").dataset["id"] = channel;
        line.querySelector("div").setAttribute("id", `light-channel-${tag.toLowerCase()}`);

        const slider = line.querySelector("input.slider");
        slider.dataset["id"] = channel;
        slider.addEventListener("change", onChannelSliderChange);

        const label = `Channel #${channel} (${tag.toUpperCase()})`;
        line.querySelector("label").textContent = label;
        mergeTemplate(container, line);

        enumerables.push({"id": channel, "name": label});
    });

    addEnumerables("Channels", enumerables);
}

function updateChannels(values) {
    const container = document.getElementById("light");
    if (!container) {
        return;
    }

    values.forEach((value, channel) => {
        const slider = container.querySelector(`input.slider[data-id='${channel}']`);
        if (!slider) {
            return;
        }

        slider.value = value;
        slider.nextElementSibling.textContent = value;
    });
}

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
