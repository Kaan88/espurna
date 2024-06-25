import { sendAction } from './connection.mjs';

import {
    setInputValue,
    setOriginalsFromValues,
    variableListeners,
} from './settings.mjs';

import {
    mergeTemplate,
    loadConfigTemplate,
} from './template.mjs';

function onButtonPress(event) {
    const prefix = "button-rfb-";
    const [buttonRfbClass] = Array.from(event.target.classList)
        .filter(x => x.startsWith(prefix));

    if (buttonRfbClass) {
        const container = event.target.parentElement.parentElement;
        const input = container.querySelector("input");

        sendAction(`rfb${buttonRfbClass.replace(prefix, "")}`, {
            id: parseInt(input.dataset["id"], 10),
            status: input.name === "rfbON"
        });
    }
}

function addNode() {
    let container = document.getElementById("rfbNodes");

    const id = container.childElementCount;
    const line = loadConfigTemplate("rfb-node");
    line.querySelector("span").textContent = id;

    for (let input of line.querySelectorAll("input")) {
        input.dataset["id"] = id;
        input.setAttribute("id", `${input.name}${id}`);
    }

    for (let action of ["learn", "forget"]) {
        for (let button of line.querySelectorAll(`.button-rfb-${action}`)) {
            button.addEventListener("click", onButtonPress);
        }
    }

    mergeTemplate(container, line);
}

function handleCodes(value) {
    value.codes.forEach((codes, id) => {
        const realId = id + value.start;
        const [off, on] = codes;

        const rfbOn = document.getElementById(`rfbON${realId}`);
        setInputValue(rfbOn, on);

        const rfbOff = document.getElementById(`rfbOFF${realId}`);
        setInputValue(rfbOff, off);

        setOriginalsFromValues([rfbOn, rfbOff]);
    });
}

function listeners() {
    return {
        "rfbCount": (_, value) => {
            for (let i = 0; i < value; ++i) {
                addNode();
            }
        },
        "rfb": (_, value) => {
            handleCodes(value);
        },
    };
}

export function init() {
    variableListeners(listeners());
}
