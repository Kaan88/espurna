import { sendAction } from './connection.mjs';

function learn() {
    sendAction("lightfoxLearn");
}

function clear() {
    sendAction("lightfoxClear");
}

export function init() {
    document.querySelector(".button-lightfox-learn")
        ?.addEventListener("click", learn);
    document.querySelector(".button-lightfox-clear")
        ?.addEventListener("click", clear);
}
