import { addEnumerables, variableListeners } from './settings.mjs';
import { showErrorNotification } from './errors.mjs';

function makeConfig(value) {
    let types = [];

    for (const [type, id] of value.types) {
        types.push({
            "id": id,
            "name": type
        });

        let gpios = [{"id": 153, "name": "NONE"}];
        value[type].forEach((pin) => {
            gpios.push({"id": pin, "name": `GPIO${pin}`});
        });

        addEnumerables(`gpio-${type}`, gpios);
    }

    addEnumerables("gpio-types", types);
}

function reportFailed(value) {
    let failed = "";
    for (const [pin, file, func, line] of value["failed-locks"]) {
        failed += `GPIO${pin} @ ${file}:${func}:${line}\n`;
    }

    if (failed.length > 0) {
        showErrorNotification("Could not acquire locks on the following pins, check configuration\n\n" + failed);
    }
}

function listeners() {
    return {
        "gpioConfig": (_, value) => {
            makeConfig(value);
        },
        "gpioInfo": (_, value) => {
            reportFailed(value);
        },
    };
}

export function init() {
    variableListeners(listeners());
}
