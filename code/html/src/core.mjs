/**
 * @param {string[]} rules
 */
export function styleInject(rules) {
    if (!rules.length) {
        return;
    }

    const style = document.createElement("style");
    style.setAttribute("type", "text/css");
    document.head.appendChild(style);

    const sheet = style.sheet;
    if (!sheet) {
        return;
    }

    let pos = sheet.cssRules.length;
    for (let rule of rules) {
        style.sheet.insertRule(rule, pos++);
    }
}

/**
 * @param {string} selector
 * @param {boolean} value
 */
export function styleVisible(selector, value) {
    return `${selector} { content-visibility: ${value ? "visible": "hidden"}; }`
}

/**
 * @param {number} timeout
 */
export function pageReloadIn(timeout) {
    setTimeout(() => window.location.reload(), timeout);
}

/**
 * @param {HTMLElement} container
 */
export function moreElem(container) {
    container.querySelectorAll(".more")
        .forEach((elem) => {
            if (!(elem instanceof HTMLElement)) {
                return;
            }

            elem.style.display = (elem.style.display === "")
                ? "inherit" : "";
        });
}

/**
 * @param {HTMLElement} elem
 */
export function lastMoreElem(elem) {
    if (elem.lastChild instanceof HTMLElement) {
        moreElem(elem.lastChild)
    }
}

/**
 * @param {string} name
 */
export function showPanelByName(name) {
    // only a single panel is shown on the 'layout'
    const target = document.getElementById(`panel-${name}`);
    if (!target) {
        return;
    }

    for (const panel of document.getElementsByClassName("panel")) {
        if (!(panel instanceof HTMLElement)) {
            continue;
        }

        panel.style.display = "none";
    }

    target.style.display = "revert";

    const layout = document.getElementById("layout");
    if (layout) {
        layout.classList.remove("active");
    }

    // TODO: sometimes, switching view causes us to scroll past
    // the header (e.g. emon ratios panel on small screen)
    // layout itself stays put, but the root element seems to scroll,
    // at least can be reproduced with Chrome
    if (document.documentElement) {
        document.documentElement.scrollTop = 0;
    }
}

/**
 * @param {Event} event
 */
export function onPanelTargetClick(event) {
    event.preventDefault();

    const target = event.target;
    if (!(target instanceof HTMLElement)) {
        return;
    }

    const name = target.dataset["panel"];
    if (name) {
        showPanelByName(name);
    }
}

/**
 * @typedef {{hex?: boolean, lowercase?: boolean, numbers?: boolean, special?: boolean, uppercase?: boolean}} RandomStringOptions
 *
 * @param {number} length
 * @param {RandomStringOptions} options
 */
export function randomString(length, {hex = false, lowercase = true, numbers = true, special = false, uppercase = true} = {}) {
    let mask = "";
    if (lowercase || hex) { mask += "abcdef"; }
    if (lowercase) { mask += "ghijklmnopqrstuvwxyz"; }
    if (uppercase || hex) { mask += "ABCDEF"; }
    if (uppercase) { mask += "GHIJKLMNOPQRSTUVWXYZ"; }
    if (numbers || hex) { mask += "0123456789"; }
    if (special) { mask += "~`!@#$%^&*()_+-={}[]:\";'<>?,./|\\"; }

    const source = new Uint32Array(length);
    const result = new Array(length);

    window.crypto
        .getRandomValues(source)
        .forEach((value, i) => {
            result[i] = mask[value % mask.length];
        });

    return result.join("");
}
