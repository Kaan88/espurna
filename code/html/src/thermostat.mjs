import { askAndCall, askSaveSettings } from './question.mjs';
import { sendAction } from './connection.mjs';

function checkTempRange(event) {
    const min = document.getElementById("tempRangeMinInput");
    const max = document.getElementById("tempRangeMaxInput");

    if (event.target.id === max.id) {
        const maxValue = parseInt(max.value, 10) - 1;
        if (parseInt(min.value, 10) > maxValue) {
            min.value = maxValue;
        }
    } else {
        const minValue = parseInt(min.value, 10) + 1;
        if (parseInt(max.value, 10) < minValue) {
            max.value = minValue;
        }
    }
}

function onResetCounters() {
    const questions = [
        askSaveSettings,
        (ask) => ask("Are you sure you want to reset burning counters?")
    ];

    askAndCall(questions, () => {
        sendAction("thermostat_reset_counters");
    });
}

export function init() {
    document.querySelector(".button-thermostat-reset-counters")
        .addEventListener("click", onResetCounters);
    document.getElementById("tempRangeMaxInput")
        .addEventListener("change", checkTempRange);
    document.getElementById("tempRangeMinInput")
        .addEventListener("change", checkTempRange);
}
