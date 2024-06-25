export function styleInject(rules) {
    if (!rules.length) {
        return;
    }

    const style = document.createElement("style");
    style.setAttribute("type", "text/css");
    document.head.appendChild(style);

    let pos = style.sheet.cssRules.length;
    for (let rule of rules) {
        style.sheet.insertRule(rule, pos++);
    }
}

export function styleVisible(selector, value) {
    return `${selector} { content-visibility: ${value ? "visible": "hidden"}; }`
}

export function pageReloadIn(ms) {
    setTimeout(() => {
        window.location.reload();
    }, parseInt(ms, 10));
}

export function moreElem(container) {
    for (let elem of container.querySelectorAll(".more")) {
        elem.style.display = (elem.style.display === "")
            ? "inherit" : "";
    }
}

export function toggleMenu(event) {
    event.preventDefault();
    event.target.parentElement.classList.toggle("active");
}

export function showPanelByName(name) {
    // only a single panel is shown on the 'layout'
    const target = document.getElementById(`panel-${name}`);
    if (!target) {
        return;
    }

    for (const panel of document.querySelectorAll(".panel")) {
        panel.style.display = "none";
    }
    target.style.display = "revert";

    const layout = document.getElementById("layout");
    layout.classList.remove("active");

    // TODO: sometimes, switching view causes us to scroll past
    // the header (e.g. emon ratios panel on small screen)
    // layout itself stays put, but the root element seems to scroll,
    // at least can be reproduced with Chrome
    if (document.documentElement) {
        document.documentElement.scrollTop = 0;
    }
}

export function showPanel(event) {
    event.preventDefault();
    showPanelByName(event.target.dataset["panel"]);
}

export function randomString(length, args) {
    if (typeof args === "undefined") {
        args = {
            lowercase: true,
            uppercase: true,
            numbers: true,
            special: true
        }
    }

    let mask = "";
    if (args.lowercase) { mask += "abcdefghijklmnopqrstuvwxyz"; }
    if (args.uppercase) { mask += "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; }
    if (args.numbers || args.hex) { mask += "0123456789"; }
    if (args.hex) { mask += "ABCDEF"; }
    if (args.special) { mask += "~`!@#$%^&*()_+-={}[]:\";'<>?,./|\\"; }

    let source = new Uint32Array(length);
    let result = new Array(length);

    window.crypto.getRandomValues(source)
        .forEach((value, i) => {
            result[i] = mask[value % mask.length];
        });

    return result.join("");
}
