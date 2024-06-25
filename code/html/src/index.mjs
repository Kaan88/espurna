import { notifyError } from './errors.mjs';
window.onerror = notifyError;

import {
    pageReloadIn,
    randomString,
    showPanel,
    styleInject,
} from './core.mjs';

import { validatePassword, validateFormsPasswords } from './validate.mjs';

import {
    askAndCallAction,
    askAndCallReboot,
    askAndCallReconnect,
} from './question.mjs';

import {
    init as initSettings,
    applySettings,
    getData,
    setChangedElement,
    updateVariables,
    variableListeners,
} from './settings.mjs';

import { init as initWiFi } from './wifi.mjs';
import { init as initGpio } from './gpio.mjs';
import { init as initConnection, connect } from './connection.mjs';

import { init as initApi } from './api.mjs';
import { init as initCurtain } from './curtain.mjs';
import { init as initDebug } from './debug.mjs';
import { init as initDomoticz } from './domoticz.mjs';
import { init as initGarland } from './garland.mjs';
import { init as initHa } from './ha.mjs';
import { init as initLed } from './led.mjs';
import { init as initLight } from './light.mjs';
import { init as initLightfox } from './lightfox.mjs';
import { init as initOta } from './ota.mjs';
import { init as initRelay } from './relay.mjs';
import { init as initRfm69 } from './rfm69.mjs';
import { init as initRfbridge } from './rfbridge.mjs';
import { init as initRules } from './rules.mjs';
import { init as initSchedule } from './schedule.mjs';
import { init as initSensor } from './sensor.mjs';
import { init as initThermostat } from './thermostat.mjs';
import { init as initThingspeak } from './thingspeak.mjs';

let KeepTime = null;

let Ago = 0;
let Now = {
    date: null,
    offset: "",
};

const __title_cache = {
    "hostname": "?",
    "app_name": "ESPurna",
    "app_version": "0.0.0",
};

function documentTitle(key, value) {
    __title_cache[key] = value;
    document.title = `${__title_cache.hostname} - ${__title_cache.app_name} ${__title_cache.app_version}`;
}

function moduleVisible(module) {
    styleInject([`.module-${module} { display: revert; }`]);
}

function modulesVisible(modules) {
    modules.forEach((module) => {
        moduleVisible(module);
    });
}

function modulesVisibleAll() {
    document.querySelectorAll("[class*=module-]")
        .forEach((elem) => {
            elem.style.display = "revert";
        });
}

function deviceNow(value) {
    try {
        Now.date = normalizedDate(value);
        Now.offset = timestampOffset(value);
    } catch (e) {
        notifyError(null, null, 0, 0, e);
    }
}

function onAction(value) {
    if ("reload" === value) {
        pageReloadIn(1000);
    }
}

function onMessage(value) {
    window.alert(value);
}

function initWebMode(value) {
    const initial = (1 === value);

    const layout = document.getElementById("layout");
    layout.style.display = initial ? "none" : "inherit";

    const password = document.getElementById("password");
    password.style.display = initial ? "inherit" : "none";
}

function deviceUptime() {
    Ago = 0;
}

function timestampDatetime(timestamp) {
    return timestamp.slice(0, 19);
}

function timestampOffset(timestamp) {
    if (timestamp.endsWith("Z")) {
        return "Z";
    }

    return timestamp.slice(-6);
}

function displayDatetime(now) {
    let datetime = timestampDatetime(now.date.toISOString());
    datetime = datetime.replace("T", " ");
    datetime = `${datetime} ${now.offset}`;

    return datetime;
}

function normalizedTimestamp(timestamp) {
    return `${timestampDatetime(timestamp)}Z`;
}

function normalizedDate(timestamp) {
    return new Date(normalizedTimestamp(timestamp));
}

function keepTime() {
    document.querySelector("span[data-key='app:ago']").textContent = Ago;
    ++Ago;

    if (null !== Now.date) {
        document.querySelector("span[data-key='app:now']")
            .textContent = displayDatetime(Now);
        Now.date = new Date(Now.date.valueOf() + 1000);
    }
}

function listeners() {
    return {
        "action": (_, value) => {
            onAction(value);
        },
        "app_name": documentTitle,
        "app_version": documentTitle,
        "hostname": documentTitle,
        "message": (_, value) => {
            onMessage(value);
        },
        "modulesVisible": (_, value) => {
            modulesVisible(value);
        },
        "now": (_, value) => {
            deviceNow(value);
        },
        "uptime": deviceUptime,
        "webMode": (_, value) => {
            initWebMode(value);
        },
    };
}

function generatePassword() {
    let password = "";
    do {
        password = randomString(10);
    } while (!validatePassword(password));

    return password;
}

function generatePasswordsForForm(form) {
    const value = generatePassword();
    for (let elem of [form.elements.adminPass0, form.elements.adminPass1]) {
        setChangedElement(elem);
        elem.type = "text";
        elem.value = value;
    }
}

function initSetupPassword(form) {
    document.querySelector(".button-setup-password")
        .addEventListener("click", (event) => {
            event.preventDefault();
            const forms = [form];
            if (validateFormsPasswords(forms, true)) {
                applySettings(getData(forms, true, false));
            }
        });
    document.querySelector(".button-generate-password")
        .addEventListener("click", (event) => {
            event.preventDefault();
            generatePasswordsForForm(form);
        });
}

function toggleMenu(event) {
    event.preventDefault();
    event.target.parentElement.classList.toggle("active");
}

function toggleVisiblePassword(event) {
    let elem = event.target.previousElementSibling;
    if (elem.type === "password") {
        elem.type = "text";
    } else {
        elem.type = "password";
    }
}

function onJsonPayload(event) {
    if (!KeepTime) {
        KeepTime = setInterval(keepTime, 1000);
    }

    try {
        const parsed = JSON.parse(
            event.data
                .replace(/\n/g, "\\n")
                .replace(/\r/g, "\\r")
                .replace(/\t/g, "\\t"));
        updateVariables(parsed);
    } catch (e) {
        notifyError(null, null, 0, 0, e);
    }
}

function init() {
    // Initial page, when webMode only allows to change the password
    initSetupPassword(document.forms["form-setup-password"]);
    document.querySelectorAll(".password-reveal")
        .forEach((elem) => {
            elem.addEventListener("click", toggleVisiblePassword);
        });

    // Sidebar menu & buttons
    document.querySelector(".menu-link")
        .addEventListener("click", toggleMenu);
    document.querySelectorAll(".pure-menu-link")
        .forEach((elem) => {
            elem.addEventListener("click", showPanel);
        });

    document.querySelector(".button-reconnect")
        .addEventListener("click", askAndCallReconnect);
    document.querySelectorAll(".button-reboot")
        .forEach((elem) => {
            elem.addEventListener("click", askAndCallReboot);
        });

    // Generic action sender
    document.querySelectorAll(".button-simple-action")
        .forEach((elem) => {
            elem.addEventListener("click", askAndCallAction);
        });

    variableListeners(listeners());

    initConnection();
    initSettings();
    initWiFi();
    initGpio();

    if (MODULE_OTA) {
        initOta();
    }

    if (MODULE_HA) {
        initHa();
    }

    if (MODULE_SNS) {
        initSensor();
    }

    if (MODULE_GARLAND) {
        initGarland();
    }

    if (MODULE_THERMOSTAT) {
        initThermostat();
    }

    if (MODULE_LIGHTFOX) {
        initLightfox();
    }

    if (MODULE_RELAY) {
        initRelay();
    }

    if (MODULE_RFM69) {
        initRfm69();
    }

    if (MODULE_RFB) {
        initRfbridge();
    }

    if (MODULE_CMD || MODULE_DBG) {
        initDebug();
    }

    if (MODULE_API) {
        initApi();
    }

    if (MODULE_LED) {
        initLed();
    }

    if (MODULE_LIGHT) {
        initLight();
    }

    if (MODULE_SCH) {
        initSchedule();
    }

    if (MODULE_RPN) {
        initRules();
    }

    if (MODULE_RELAY && MODULE_DCZ) {
        initDomoticz();
    }

    if (MODULE_RELAY && MODULE_TSPK) {
        initThingspeak();
    }

    if (MODULE_CURTAIN) {
        initCurtain();
    }

    // don't autoconnect w/ localhost or file://
    if (MODULE_LOCAL) {
        updateVariables({
            webMode: 0,
            now: "2024-01-01T00:00:00+01:00",
        });
        KeepTime = setInterval(keepTime, 1000);
        modulesVisibleAll();
        return;
    }

    connect(onJsonPayload);
}

document.addEventListener("DOMContentLoaded", init);
