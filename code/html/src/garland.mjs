import { sendAction } from './connection.mjs';

/**
 * @param {string} selector
 * @param {function(HTMLInputElement): void} callback
 */
function withInputChange(selector, callback) {
    const elem = document.querySelector(selector);
    if (!(elem instanceof HTMLInputElement)) {
        return;
    }

    elem.addEventListener("change", () => {
        callback(elem);
    });
}

export function init() {
    withInputChange(".checkbox-garland-enable",
        (elem) => {
            sendAction("garland_switch", {status: elem.checked ? 1 : 0});
        });

    withInputChange(".slider-garland-brightness",
        (elem) => {
            sendAction("garland_set_brightness", {brightness: elem.value});
        });

    withInputChange(".slider-garland-speed",
        (elem) => {
            sendAction("garland_set_speed", {speed: elem.value});
        });

    document.querySelector(".button-garland-set-default")
        ?.addEventListener("click", () => {
            sendAction("garland_set_default", {});
        });
}
