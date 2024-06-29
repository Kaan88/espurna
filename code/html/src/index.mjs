/// <reference path="index.build.d.mts" />

/**
 * @typedef {function(string, any): void} Listener
 */

/**
 * @typedef {{[k: string]: Listener}} Listeners
 */

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

/** @type {number | null} */
let KeepTime = null;

/** @type {number} */
let Ago = 0;

/**
 * @typedef {{date: Date | null, offset: string}} NowType
 * @type NowType
 */

const Now = {
    date: null,
    offset: "",
};

/**
 * @param {{[k: string]: string}} cache
 * @param {string} key
 * @param {string} value
 */
function documentTitleCache(cache, key, value) {
    cache[key] = value;
    document.title = `${cache.hostname} - ${cache.app_name} ${cache.app_version}`;
}

/**
 * @type {{[k: string]: string}}
 */
const __title_cache = {
    "hostname": "?",
    "app_name": "ESPurna",
    "app_version": "0.0.0",
};

/**
 * @param {string} key
 * @param {string} value
 */
function documentTitle(key, value) {
    documentTitleCache(__title_cache, key, value);
}

/**
 * @param {string} module
 */
function moduleVisible(module) {
    styleInject([`.module-${module} { display: revert; }`]);
}

/**
 * @param {string[]} modules
 */
function modulesVisible(modules) {
    modules.forEach((module) => {
        moduleVisible(module);
    });
}

function modulesVisibleAll() {
    document.querySelectorAll("[class*=module-]")
        .forEach((elem) => {
            /** @type {HTMLElement} */(elem).style.display = "revert";
        });
}

/**
 * @param {string} value
 */
function deviceNow(value) {
    try {
        Now.date = normalizedDate(value);
        Now.offset = timestampOffset(value);
    } catch (e) {
        notifyError(null, null, 0, 0, e);
    }
}

/**
 * @param {string} value
 */
function onAction(value) {
    if ("reload" === value) {
        pageReloadIn(1000);
    }
}

/**
 * @param {string} value
 */
function onMessage(value) {
    window.alert(value);
}

/**
 * @param {number} value
 */
function initWebMode(value) {
    const initial = (1 === value);

    const layout = document.getElementById("layout")
    if (layout) {
        layout.style.display = (initial ? "none" : "inherit");
    }

    const password = document.getElementById("password");
    if (password) {
        password.style.display = initial ? "inherit" : "none";
    }
}

/**
 * @param {string} value
 * @returns {string}
 */
function timestampDatetime(value) {
    return value.slice(0, 19);
}

/**
 * @param {string} value
 * @returns {string}
 */
function timestampOffset(value) {
    if (value.endsWith("Z")) {
        return "Z";
    }

    return value.slice(-6);
}

/**
 * @param {NowType} now
 * @returns {string}
 */
function displayDatetime(now) {
    if (now.date) {
        let datetime = timestampDatetime(now.date.toISOString());
        datetime = datetime.replace("T", " ");
        datetime = `${datetime} ${now.offset}`;
        return datetime;
    }

    return "?";
}

/**
 * @param {string} value
 * @returns {string}
 */
function normalizedTimestamp(value) {
    return `${timestampDatetime(value)}Z`;
}

/**
 * @param {string} value
 * @returns {Date}
 */
function normalizedDate(value) {
    return new Date(normalizedTimestamp(value));
}


function keepTime() {
    document.querySelector("span[data-key='app:ago']").textContent = Ago.toString();
    ++Ago;

    if (null !== Now.date) {
        document.querySelector("span[data-key='app:now']")
            .textContent = displayDatetime(Now);
        Now.date = new Date(Now.date.valueOf() + 1000);
    }
}

/**
 * @returns {Listeners}
 */
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
        "webMode": (_, value) => {
            initWebMode(value);
        },
    };
}

/**
 * @returns {string}
 */
function generatePassword() {
    let password = "";
    do {
        password = randomString(10);
    } while (!validatePassword(password));

    return password;
}

/**
 * @param {HTMLFormElement} form
 */
function generatePasswordsForForm(form) {
    const value = generatePassword();
    for (let elem of [form.elements.adminPass0, form.elements.adminPass1]) {
        setChangedElement(elem);
        elem.type = "text";
        elem.value = value;
    }
}

/**
 * @param {HTMLFormElement} form
 */
function initSetupPassword(form) {
    document.querySelector(".button-setup-password")
        .addEventListener("click", (event) => {
            event.preventDefault();
            const forms = [form];
            if (validateFormsPasswords(forms, true)) {
                applySettings(getData(forms, true));
            }
        });
    document.querySelector(".button-generate-password")
        .addEventListener("click", (event) => {
            event.preventDefault();
            generatePasswordsForForm(form);
        });
}

/**
 * @param {Event} event
 * @returns {any}
 */
function toggleMenu(event) {
    event.preventDefault();
    /** @type {HTMLElement} */(event.target).parentElement?.classList.toggle("active");
}

/**
 * @param {Event} event
 */
function toggleVisiblePassword(event) {
    const target = /** @type {HTMLSpanElement} */(event.target);
    const input = /** @type {HTMLInputElement} */(target.previousElementSibling);

    if (input.type === "password") {
        input.type = "text";
    } else {
        input.type = "password";
    }
}

/**
 * @param {MessageEvent<any>} event
 */
function onJsonPayload(event) {
    Ago = 0;

    if (!KeepTime) {
        KeepTime = window.setInterval(keepTime, 1000);
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
        KeepTime = window.setInterval(keepTime, 1000);
        modulesVisibleAll();
        return;
    }

    connect(onJsonPayload);
}

document.addEventListener("DOMContentLoaded", init);
