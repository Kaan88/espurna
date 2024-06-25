import { sendAction } from './connection.mjs';
import { variableListeners } from './settings.mjs';

function listeners() {
    return {
        "garlandBrightness": (_, value) => {
            const brightnessSlider = document.getElementById("garlandBrightness");
            brightnessSlider.value = value;
        },
        "garlandSpeed": (_, value) => {
            const speedSlider = document.getElementById("garlandSpeed");
            speedSlider.value = value;
        },
    };
}

export function init() {
    variableListeners(listeners());

    document.querySelector(".checkbox-garland-enable")
        .addEventListener("change", (event) => {
            sendAction("garland_switch", {status: event.target.checked ? 1 : 0});
        });

    document.querySelector(".slider-garland-brightness")
        .addEventListener("change", (event) => {
            sendAction("garland_set_brightness", {brightness: event.target.value});
        });

    document.querySelector(".slider-garland-speed")
        .addEventListener("change", (event) => {
            sendAction("garland_set_speed", {speed: event.target.value});
        });

    document.querySelector(".button-garland-set-default")
        .addEventListener("click", () => {
            sendAction("garland_set_default", {});
        });
}
