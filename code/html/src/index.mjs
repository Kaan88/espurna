/// <reference path="index.build.d.mts" />

import { notifyError, notifyErrorEvent } from './errors.mjs';
window.addEventListener("error", (event) => {
    notifyErrorEvent(event);
    console.error(event.error);
});

import {
    pageReloadIn,
    randomString,
    onPanelTargetClick,
    styleInject,
} from './core.mjs';

import { validatePassword, validateFormsPasswords } from './validate.mjs';

import { askAndCall } from './question.mjs';

import {
    init as initSettings,
    applySettings,
    askSaveSettings,
    getData,
    setChangedElement,
    updateVariables,
    variableListeners,
} from './settings.mjs';

import { init as initWiFi } from './wifi.mjs';
import { init as initGpio } from './gpio.mjs';
import {
    init as initConnection,
    connect,
    sendAction,
} from './connection.mjs';

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
import { init as initLocal } from './local.mjs';

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
 * @type {{[k: string]: string}}
 */
const __title_cache = {
    hostname: "?",
    app_name: "ESPurna",
    app_version: "0.0.0",
};

/**
 * @param {string} key
 * @param {string} value
 */
function documentTitle(key, value) {
    __title_cache[key] = value;
    document.title = `${__title_cache.hostname} - ${__title_cache.app_name} ${__title_cache.app_version}`;
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
        notifyError(/** @type {Error} */(e));
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
    const ago = document.querySelector("span[data-key='app:ago']");
    if (ago) {
        ago.textContent = Ago.toString();
    }

    ++Ago;

    if (null !== Now.date) {
        const now = document.querySelector("span[data-key='app:now']");
        if (now) {
            now.textContent = displayDatetime(Now);
        }

        Now.date = new Date(Now.date.valueOf() + 1000);
    }
}

/** @import { QuestionWrapper } from './question.mjs' */

/** @type {QuestionWrapper} */
function askDisconnect(ask) {
    return ask("Are you sure you want to disconnect from the current WiFi network?");
}

/** @type {QuestionWrapper} */
function askReboot(ask) {
    return ask("Are you sure you want to reboot the device?");
}

/** @param {CloseEvent} event */
function askReload(event) {
    /** @type {QuestionWrapper} */
    return function(ask) {
        return ask(`Connection lost with the device - ${event.reason}. Click OK to refresh the page.`);
    };
}

function askAndCallReconnect() {
    askAndCall([askSaveSettings, askDisconnect], () => {
        sendAction("reconnect");
    });
}

function askAndCallReboot() {
    askAndCall([askSaveSettings, askReboot], () => {
        sendAction("reboot");
    });
}

/** @param {Event} event */
function askAndCallSimpleAction(event) {
    const target = event.target;
    if (!(target instanceof HTMLButtonElement)) {
        return;
    }

    /** @type {QuestionWrapper} */
    const wrapper =
        (ask) => ask(`Confirm the action: "${target.textContent}"`);

    askAndCall([wrapper], () => {
        sendAction(target.name);
    });
}

// TODO https://github.com/microsoft/TypeScript/issues/58969
// at-import'ed type becomes 'unused' for some reason
// until fixed, prefer direct import vs. typedef

/**
 * @returns {import('./settings.mjs').KeyValueListeners}
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
        password = randomString(10, {special: true});
    } while (!validatePassword(password));

    return password;
}

/**
 * @param {HTMLFormElement} form
 */
function generatePasswordsForForm(form) {
    const value = generatePassword();

    ["adminPass0", "adminPass1"]
        .map((x) => form.elements.namedItem(x))
        .filter((x) => x instanceof HTMLInputElement)
        .forEach((elem) => {
            setChangedElement(elem);
            elem.type = "text";
            elem.value = value;
        });
}

/**
 * @param {HTMLFormElement} form
 */
function initSetupPassword(form) {
    document.querySelectorAll(".button-setup-password")
        .forEach((elem) => {
            elem.addEventListener("click", (event) => {
                event.preventDefault();

                const target = /** @type {HTMLInputElement} */
                    (event.target);
                const lenient = target.classList
                    .contains("button-setup-lenient");

                const forms = [form];
                if (validateFormsPasswords(forms, {strict: !lenient})) {
                    applySettings(getData(forms, {cleanup: false}));
                }
            });
        });

    document.querySelector(".button-generate-password")
        ?.addEventListener("click", (event) => {
            event.preventDefault();
            generatePasswordsForForm(form);
        });
}

/**
 * @param {Event} event
 * @returns {any}
 */
function onMenuLinkClick(event) {
    event.preventDefault();

    const target = event.target;
    if (!(target instanceof HTMLElement)) {
        return;
    }

    if (target?.parentElement) {
        target.parentElement.classList.toggle("active");
    }
}

/**
 * @param {Event} event
 */
function onPasswordRevealClick(event) {
    const target = event.target;
    if (!(target instanceof HTMLElement)) {
        return;
    }

    const input = target.previousElementSibling;
    if (!(input instanceof HTMLInputElement)) {
        return;
    }

    if (input.type === "password") {
        input.type = "text";
    } else {
        input.type = "password";
    }
}

/**
 * @param {CloseEvent} event
 */
function onConnectionClose(event) {
    askAndCall([askReload(event)], () => {
        pageReloadIn(1000);
    });
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
                .replace(/:Infinity/g, ':"inf"')
                .replace(/:-Infinity/g, ':"-inf"')
                .replace(/:NaN/g, ':"nan"')
                .replace(/\n/g, "\\n")
                .replace(/\r/g, "\\r")
                .replace(/\t/g, "\\t"));
        updateVariables(parsed);
    } catch (e) {
        notifyError(/** @type {Error} */(e));
    }
}

function init() {
    // Initial page, when webMode only allows to change the password
    const passwd = document.forms.namedItem("form-setup-password");
    if (passwd) {
        initSetupPassword(passwd);
    }

    document.querySelectorAll(".password-reveal")
        .forEach((elem) => {
            elem.addEventListener("click", onPasswordRevealClick);
        });

    // Sidebar menu & buttons
    document.querySelector(".menu-link")
        ?.addEventListener("click", onMenuLinkClick);
    document.querySelectorAll(".pure-menu-link")
        .forEach((elem) => {
            elem.addEventListener("click", onPanelTargetClick);
        });

    document.querySelector(".button-reconnect")
        ?.addEventListener("click", askAndCallReconnect);
    document.querySelectorAll(".button-reboot")
        .forEach((elem) => {
            elem.addEventListener("click", askAndCallReboot);
        });

    // Generic action sender
    document.querySelectorAll(".button-simple-action")
        .forEach((elem) => {
            elem.addEventListener("click", askAndCallSimpleAction);
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

    if (MODULE_LOCAL) {
        initLocal();
        KeepTime = window.setInterval(keepTime, 1000);
        modulesVisibleAll();
        return;
    }

    // don't autoconnect w/ localhost or file://
    connect({onclose: onConnectionClose, onmessage: onJsonPayload});
}

document.addEventListener("DOMContentLoaded", init);
