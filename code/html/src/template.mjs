// Generic parts of the HTML are placed into <template> container, which requires
// us to 'import' it into the currently loaded page to actually use it.
// (and notice that document.querySelector(...) won't be able to read inside of these)

import {
    setInputValue,
    setOriginalsFromValuesForNode,
    setSelectValue,
    setSpanValue,
    initSelect,
    initEnumerableSelect,
    onGroupSettingsDel,
} from './settings.mjs';

import { moreElem } from './core.mjs';

function moreParent(event) {
    moreElem(event.target.parentElement.parentElement);
}

/**
 * @returns {HTMLElement}
 */
export function loadTemplate(name) {
    let template = document.getElementById(`template-${name}`);
    return document.importNode(template.content, true);
}

export function loadConfigTemplate(id) {
    let template = loadTemplate(id);
    for (let elem of template.querySelectorAll("input,select")) {
        elem.dataset["settingsGroupElement"] = "true";
    }

    for (let elem of template.querySelectorAll("button.button-del-settings-group")) {
        elem.addEventListener("click", onGroupSettingsDel);
    }

    for (let elem of template.querySelectorAll("button.button-more-parent")) {
        elem.addEventListener("click", moreParent);
    }

    for (let elem of template.querySelectorAll("select.enumerable")) {
        initEnumerableSelect(elem, initSelect);
    }

    return template;
}

/**
 * @param {HTMLElement} line
 * @param {number} id
 * @param {any} cfg
 */
export function fillTemplateLineFromCfg(line, id, cfg) {
    let local = {"template-id": id};
    if (cfg === undefined) {
        cfg = {};
    }

    Object.assign(local, cfg);
    cfg = local;

    for (let elem of line.querySelectorAll("input,select,span")) {
        let key = elem.name || elem.dataset.key;
        if (key && (key in cfg)) {
            switch (elem.tagName) {
            case "INPUT":
                setInputValue(elem, cfg[key]);
                break;
            case "SELECT":
                setSelectValue(elem, cfg[key]);
                break;
            case "SPAN":
                setSpanValue(elem, cfg[key]);
                break;
            }
        }
    }

    setOriginalsFromValuesForNode(line);
}

export function mergeTemplate(target, template) {
    for (let child of Array.from(template.children)) {
        target.appendChild(child);
    }
}

export function addFromTemplate(container, template, cfg) {
    const line = loadConfigTemplate(template);
    fillTemplateLineFromCfg(line, container.childElementCount, cfg);
    mergeTemplate(container, line);
}
