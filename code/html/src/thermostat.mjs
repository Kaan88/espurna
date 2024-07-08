import { askAndCall } from './question.mjs';
import { askSaveSettings } from './settings.mjs';
import { sendAction } from './connection.mjs';

/**
 * @param {HTMLInputElement} min
 * @param {HTMLInputElement} max
 */
function checkTempMax(min, max) {
    return function() {
        const maxValue = parseInt(max.value, 10) - 1;
        if (parseInt(min.value, 10) > maxValue) {
            min.value = maxValue.toString();
        }
    }
}

/**
 * @param {HTMLInputElement} min
 * @param {HTMLInputElement} max
 */
function checkTempMin(min, max) {
    return function() {
        const minValue = parseInt(min.value, 10) + 1;
        if (parseInt(max.value, 10) < minValue) {
            max.value = minValue.toString();
        }
    }
}

/** @type {import("./question.mjs").QuestionWrapper} */
function askResetCounters(ask) {
    return ask("Are you sure you want to reset burning counters?");
}

function onResetCounters() {
    const questions = [
        askSaveSettings,
        askResetCounters,
    ];

    askAndCall(questions, () => {
        sendAction("thermostat_reset_counters");
    });
}

export function init() {
    document.querySelector(".button-thermostat-reset-counters")
        ?.addEventListener("click", onResetCounters);

    const [min, max] =
        ["Min", "Max"]
        .map((x) => `tempRange${x}Input`)
        .map((x) => document.getElementById(x))
        .filter((x) => x instanceof HTMLInputElement);

    if (min && max) {
        min.addEventListener("change", checkTempMin(min, max));
        max.addEventListener("change", checkTempMax(min, max));
    }
}
