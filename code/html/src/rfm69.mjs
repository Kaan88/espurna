import { addFromTemplate } from './template.mjs';
import { groupSettingsOnAdd, fromSchema, variableListeners } from './settings.mjs';
import { sendAction } from './connection.mjs';

let State = {
    filters: {}
};

function addMapping(cfg) {
    addFromTemplate(document.getElementById("rfm69-mapping"), "rfm69-node", cfg);
}

function messages() {
    let [body] = document.getElementById("rfm69-messages").tBodies;
    return body;
}

function rows() {
    return messages().rows;
}

function addMessage(message) {
    let timestamp = (new Date()).toLocaleTimeString("en-US", {hour12: false});

    let container = messages();
    let row = container.insertRow();
    for (let value of [timestamp, ...message]) {
        let cell = row.insertCell();
        cell.appendChild(document.createTextNode(value));
        filterRow(State.filters, row);
    }
}

function clearCounters() {
    sendAction("rfm69Clear");
    return false;
}

function clearMessages() {
    let container = messages();
    while (container.rows.length) {
        container.deleteRow(0);
    }
    return false;
}

function filterRow(filters, row) {
    row.style.display = "table-row";
    for (const [cell, filter] of Object.entries(filters)) {
        if (row.cells[cell].textContent !== filter) {
            row.style.display = "none";
        }
    }
}

function filterRows(filters, rows) {
    for (let row of rows) {
        filterRow(filters, row);
    }
}

function filterEvent(event) {
    if (event.target.classList.contains("filtered")) {
        delete State.filters[event.target.cellIndex];
    } else {
        State.filters[event.target.cellIndex] = event.target.textContent;
    }
    event.target.classList.toggle("filtered");

    filterRows(State.filters, rows());
}

function clearFilters() {
    let container = messages();
    for (let elem of container.querySelectorAll("filtered")) {
        elem.classList.remove("filtered");
    }

    State.filters = {};
    filterRows(State.filters, container.rows);
}

function listeners() {
    return {
        "rfm69": (_, value) => {
            if (value.message !== undefined) {
                addMessage(value.message);
            }
            if (value.mapping !== undefined) {
                value.mapping.forEach((mapping) => {
                    addMapping(fromSchema(mapping, value.schema));
                });
            }
        },
    };
}

export function init() {
    variableListeners(listeners());

    document.querySelector(".button-clear-counts")
        .addEventListener("click", clearCounters);
    document.querySelector(".button-clear-messages")
        .addEventListener("click", clearMessages);

    document.querySelector(".button-clear-filters")
        .addEventListener("click", clearFilters);
    document.querySelector("#rfm69-messages tbody")
        .addEventListener("click", filterEvent);

    groupSettingsOnAdd("rfm69-mapping", () => {
        addMapping();
    });
}
