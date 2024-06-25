import { sendAction } from './connection.mjs';
import { loadConfigTemplate, mergeTemplate } from './template.mjs';
import { addSimpleEnumerables, variableListeners } from './settings.mjs';

function listeners() {
    return {
        "curtainState": (_, value) => {
            initCurtain();
            updateCurtain(value);
        },
    };
}

function buttonHandler(event) {
    if (event.type !== "click") {
        return;
    }

    event.preventDefault();

    let code = -1;

    const list = event.target.classList;
    if (list.contains("button-curtain-pause")) {
        code = 0;
    } else if (list.contains("button-curtain-open")) {
        code = 1;
    } else if (list.contains("button-curtain-close")) {
        code = 2;
    }

    if (code >= 0) {
        sendAction("curtainAction", {button: code});
        event.target.style.background = "red";
    }
}

function positionHandler(event) {
    sendAction("curtainAction", {position: event.target.value});
}

//Create the controls for one curtain. It is called when curtain is updated (so created the first time)
//Let this there as we plan to have more than one curtain per switch
function initCurtain() {
    let container = document.getElementById("curtains");
    if (container.childElementCount > 0) {
        return;
    }

    // add and init curtain template, prepare multi switches
    let line = loadConfigTemplate("curtain-control");
    line.querySelector(".button-curtain-open")
        .addEventListener("click", buttonHandler);
    line.querySelector(".button-curtain-pause")
        .addEventListener("click", buttonHandler);
    line.querySelector(".button-curtain-close")
        .addEventListener("click", buttonHandler);
    mergeTemplate(container, line);

    // simple position slider
    document.getElementById("curtainSet")
        .addEventListener("change", positionHandler);

    addSimpleEnumerables("curtain", "Curtain", 1);
}

function setBackground(a, b) {
    let elem = document.getElementById("curtainGetPicture");
    elem.style.background = `linear-gradient(${a}, black ${b}%, #a0d6ff ${b}%)`;
}

function setBackgroundTwoSides(a, b) {
    let elem = document.getElementById("curtainGetPicture");
    elem.style.background = `linear-gradient(90deg, black ${a}%, #a0d6ff ${a}% ${b}%, black ${b}%)`;
}

function updateCurtain(value) {
    switch(value.type) {
    case '1': //One side left to right
        setBackground('90deg', value.get);
        break;
    case '2': //One side right to left
        setBackground('270deg', value.get);
        break;
    case '3': //Two sides
        setBackgroundTwoSides(value.get / 2, (100 - value.get/2));
        break;
    case '0': //Roller
    default:
        setBackground('180deg', value.get);
        break;
    }

    let set = document.getElementById("curtainSet");
    set.value = value.set;

    const backgroundMoving = 'rgb(192, 0, 0)';
    const backgroundStopped = 'rgb(64, 184, 221)';

    if (!value.moving) {
        let button = document.querySelector("button.curtain-button");
        button.style.background = backgroundStopped;
    } else if (!value.button) {
        let pause = document.querySelector("button.curtain-pause");
        pause.style.background = backgroundMoving;
    } else {
        let open = document.querySelector("button.button-curtain-open");
        let close = document.querySelector("button.button-curtain-close");
        if (value.button === 1) {
            open.style.background = backgroundMoving;
            close.style.background = backgroundStopped;
        } else if (value.button === 2) {
            open.style.background = backgroundStopped;
            close.style.background = backgroundMoving;
        }
    }
}

export function init() {
    variableListeners(listeners());
}
