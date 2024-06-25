import { sendAction } from './connection.mjs';

function publishState(event, state) {
    event.preventDefault();
    sendAction("ha-publish", {state});
}

function publishEnabled(event) {
    publishState(event, 1);
}

function publishDisabled(event) {
    publishState(event, 0);
}

export function init() {
    document.querySelector(".button-ha-enabled")
        .addEventListener("click", publishEnabled);
    document.querySelector(".button-ha-disabled")
        .addEventListener("click", publishDisabled);
}
