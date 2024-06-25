import {
    fromSchema,
    groupSettingsOnAdd,
    variableListeners,
} from './settings.mjs';

import { addFromTemplate } from './template.mjs';
import { moreElem } from './core.mjs';
import { sendAction } from './connection.mjs';

function addNode(cfg, showMore) {
    const container = document.getElementById("networks");
    addFromTemplate(container, "network-config", cfg);

    if ((showMore === undefined) || showMore) {
        moreElem(container.lastChild)
    }
}

function scanResult(values) {
    let loading = document.querySelector("div.scan.loading");
    loading.style.display = "none";

    for (let button of document.querySelectorAll(".button-wifi-scan")) {
        button.disabled = false;
    }

    let table = document.getElementById("scanResult");
    table.style.display = "table";

    let [results] = table.tBodies;
    let row = results.insertRow();
    for (let value of values) {
        let cell = row.insertCell();
        cell.appendChild(document.createTextNode(value));
    }
}

function scanStart(event) {
    event.preventDefault();

    let [results] = document.getElementById("scanResult").tBodies;
    while (results.rows.length) {
        results.deleteRow(0);
    }

    let loading = document.querySelector("div.scan.loading");
    loading.style.display = "inherit";

    for (let button of document.querySelectorAll(".button-wifi-scan")) {
        button.disabled = true;
    }

    sendAction("scan");
}

function listeners() {
    return {
        "wifiConfig": (_, value) => {
            const container = document.getElementById("networks");
            container.dataset["settingsMax"] = value.max;

            value.networks.forEach((entries) => {
                addNode(fromSchema(entries, value.schema), false);
            });
        },
        "scanResult": (_, value) => {
            scanResult(value);
        },
    };
}

export function init() {
    variableListeners(listeners());

    groupSettingsOnAdd("networks", () => {
        addNode();
    });

    document.querySelector(".button-wifi-scan")
        .addEventListener("click", scanStart);
}
