import { sendAction } from './connection.mjs';

/**
 * @param {Event} event
 * @param {number} state
 */
function publishState(event, state) {
    event.preventDefault();
    sendAction("ha-publish", {state});
}

/** @param {Event} event */
function publishEnabled(event) {
    publishState(event, 1);
}

/** @param {Event} event */
function publishDisabled(event) {
    publishState(event, 0);
}

export function init() {
    document.querySelector(".button-ha-enabled")
        ?.addEventListener("click", publishEnabled);
    document.querySelector(".button-ha-disabled")
        ?.addEventListener("click", publishDisabled);
}
