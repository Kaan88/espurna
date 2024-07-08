import { addEnumerables, variableListeners } from './settings.mjs';
import { notifyMessage } from './errors.mjs';

/**
 * @param {any} config
 */
function updateEnumerables(config) {
    /** @type {import('./settings.mjs').EnumerableEntry[]} */
    const types = [];

    for (const [type, id] of /** @type {[string, number][]} */(config.types)) {
        types.push({
            "id": id,
            "name": type
        });

        /** @type {import('./settings.mjs').EnumerableEntry[]} */
        const gpios = [{"id": 153, "name": "NONE"}];

        /** @type {number[]} */
        (config[type]).forEach((pin) => {
            gpios.push({"id": pin, "name": `GPIO${pin}`});
        });

        addEnumerables(`gpio-${type}`, gpios);
    }

    addEnumerables("gpio-types", types);
}

/** @param {[pin: number, file: string, func: string, line: number]} failed */
function reportFailed(failed) {
    if (!failed.length) {
        return;
    }

    let report = [];
    for (const [pin, file, func, line] of failed) {
        report.push(`GPIO${pin} @ ${file}:${func}:${line}`);
    }

    notifyMessage(`
        Could not acquire locks on the following pins, check configuration
        \n\n${report.join("\n")}`);
}

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
 */
function listeners() {
    return {
        "gpioConfig": (_, value) => {
            updateEnumerables(value);
        },
        "gpioInfo": (_, value) => {
            reportFailed(value["failed-locks"]);
        },
    };
}

export function init() {
    variableListeners(listeners());
}
