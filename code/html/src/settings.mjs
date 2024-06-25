import { notifyError } from './errors.mjs';
import { pageReloadIn } from './core.mjs';
import { send, sendAction, configUrl } from './connection.mjs';
import { validateForms } from './validate.mjs';

// <select data-original="..."> is read / saved as:
// - multiple=false -> value string of the selected option
// - multiple=true -> comma-separated values of all selected options
//
// If selectedIndex is -1, it means we never selected anything
// (TODO: could this actually happen with anything other than empty <select>?)

function stringifySelectedValues(select) {
    if (select.multiple) {
        return Array.from(select.selectedOptions)
            .map(option => option.value)
            .join(",");
    } else if (select.selectedIndex >= 0) {
        return select.options[select.selectedIndex].value;
    }

    return select.dataset["original"];
}

export function isChangedElement(elem) {
    return "true" === elem.dataset["changed"];
}

export function setChangedElement(elem) {
    elem.dataset["changed"] = "true";
}

export function resetChangedElement(elem) {
    elem.dataset["changed"] = "false";
}

function resetGroupPending(elem) {
    elem.dataset["settingsGroupPending"] = "";
}

// TODO: note that we also include kv schema as 'data-settings-schema' on the container.
// produce a 'set' and compare instead of just matching length?
export function fromSchema(source, schema) {
    if (schema.length !== source.length) {
        throw `Schema mismatch! Expected length ${schema.length} vs. ${source.length}`;
    }

    var target = {};
    schema.forEach((key, index) => {
        target[key] = source[index];
    });

    return target;
}

// Right now, group additions happen from:
// - WebSocket, likely to happen exactly once per connection through processData handler(s). Specific keys trigger functions that append into the container element.
// - User input. Same functions are triggered, but with an additional event for the container element that causes most recent element to be marked as changed.
// Removal only happens from user input by triggering 'settings-group-del' from the target element.
//
// TODO: distinguish 'current' state to avoid sending keys when adding and immediatly removing the latest node?
// TODO: previous implementation relied on defaultValue and / or jquery $(...).val(), but this does not really work where 'line' only has <select>

function groupElementInfo(target) {
    const out = [];

    const inputs = target.querySelectorAll("input,select");
    inputs.forEach((elem) => {
        const name = elem.dataset.settingsRealName || elem.name;
        if (name === undefined) {
            return;
        }

        out.push({
            element: elem,
            key: name,
            value: elem.dataset["original"] || getDataForElement(elem)
        });
    });

    return out;
}

function getGroupPending(elem) {
    const raw = elem.dataset["settingsGroupPending"] || "";
    if (!raw.length) {
        return [];
    }

    return raw.split(",");
}

function addGroupPending(elem, index) {
    const pending = getGroupPending(elem);
    pending.push(`set:${index}`);
    elem.dataset["settingsGroupPending"] = pending.join(",");
}

function popGroupPending(elem, index) {
    const pending = getGroupPending(elem);

    const added = pending.indexOf(`set:${index}`);
    if (added >= 0) {
        pending.splice(added, 1);
    } else {
        pending.push(`del:${index}`);
    }

    elem.dataset["settingsGroupPending"] = pending.join(",");
}

function isGroupElement(elem) {
    return elem.dataset["settingsGroupElement"] !== undefined;
}

function isIgnoredElement(elem) {
    return elem.dataset["settingsIngore"] !== undefined;
}

// to 'instantiate' a new element, we must explicitly set 'target' keys in kvs
// notice that the 'row' creation *should* be handled by the group-specific
// event listener, we already expect the dom element to exist at this point
function onGroupSettingsEventAdd(event) {
    const group = event.target;
    const index = group.children.length - 1;
    const last = group.children[index];
    addGroupPending(group, index);

    for (const target of settingsTargets(group)) {
        const elem = last.querySelector(`[name='${target}']`);
        if (elem) {
            setChangedElement(elem);
        }
    }
}

// removing the element means we need to notify the kvs about the updated keys
// in case it's the last row, just remove those keys from the store
// in case we are in the middle, make sure to handle difference update
// in case change was 'ephemeral' (i.e. from the previous add that was not saved), do nothing
function onGroupSettingsEventDel(event) {
    const group = event.currentTarget;

    const elems = Array.from(group.children);
    const shiftFrom = elems.indexOf(group);

    const info = elems.map(groupElementInfo);
    for (let index = -1; index < info.length; ++index) {
        const prev = (index > 0)
            ? info[index - 1]
            : null;
        const current = info[index];

        if ((index > shiftFrom) && prev && (prev.length === current.length)) {
            for (let inner = 0; inner < prev.length; ++inner) {
                const [lhs, rhs] = [prev[inner], current[inner]];
                if (lhs.value !== rhs.value) {
                    setChangedElement(rhs.element);
                }
            }
        }
    }

    if (elems.length) {
        popGroupPending(group, elems.length - 1);
    }

    event.preventDefault();
    event.stopImmediatePropagation();
    event.target.remove();
}

// 'settings-group' contain elements that represent kv list that is suffixed with an index in raw kvs
// 'button-add-settings-group' will trigger update on the specified 'data-settings-group' element id, which
// needs to have 'settings-group-add' event handler attached to it.

function groupSettingsCheckMax(event) {
    const max = event.target.dataset["settingsMax"];
    const val = 1 + event.target.children.length;

    if ((max !== undefined) && (parseInt(max, 10) < val)) {
        alert(`Max number of ${event.target.id} has been reached (${val} out of ${max})`);
        return false;
    }

    return true;
}

export function groupSettingsOnAdd(elementId, listener) {
    document.getElementById(elementId)
        .addEventListener("settings-group-add", (event) => {
            event.stopPropagation();
            if (!groupSettingsCheckMax(event)) {
                return;
            }

            listener(event);
            onGroupSettingsEventAdd(event);
        });
}

export function onGroupSettingsDel(event) {
    let target = event.target;
    let parent = target.parentElement;

    while (!parent.classList.contains("settings-group")) {
        target = parent;
        parent = target.parentElement;
    }

    target.dispatchEvent(
        new CustomEvent("settings-group-del", {bubbles: true}));
}

// handle addition to the group via the button
// (notice that since we still use the dataset for the elements, hyphens are just capitalized)
function groupSettingsAdd(event) {
    const prefix = "settingsGroupDetail";
    const elem = event.target;

    let eventInit = {detail: null};
    for (let key of Object.keys(elem.dataset)) {
        if (!key.startsWith(prefix)) {
            continue
        }
        if (eventInit.detail === null) {
            eventInit.detail = {};
        }

        let eventKey = key.replace(prefix, "");
        eventKey = eventKey[0].toLowerCase() + eventKey.slice(1);
        eventInit.detail[eventKey] = elem.dataset[key];
    }

    const group = document.getElementById(elem.dataset["settingsGroup"]);
    group.dispatchEvent(new CustomEvent("settings-group-add", eventInit));
}

export function getData(forms, options) {
    // Populate two sets of data, ones that had been changed and ones that stayed the same
    if (options === undefined) {
        options = {};
    }

    const data = {};
    const changed_data = [];
    if (options.cleanup === undefined) {
        options.cleanup = true;
    }

    if (options.changed === undefined) {
        options.changed = true;
    }

    const group_counter = {};

    // TODO: <input type="radio"> can be found as both individual elements and as a `RadioNodeList` view.
    // matching will extract the specific radio element, but will ignore the list b/c it has no tagName
    // TODO: actually use type="radio" in the WebUI to check whether this works
    for (let form of forms) {
        for (let elem of form.elements) {
            if ((elem.tagName !== "SELECT") && (elem.tagName !== "INPUT")) {
                continue;
            }

            if (isIgnoredElement(elem)) {
                continue;
            }

            const name = elem.dataset.settingsRealName || elem.name;
            if (name === undefined) {
                continue;
            }

            const group_element = isGroupElement(elem);
            const group_index = group_counter[name] || 0;
            const group_name = `${name}${group_index}`;
            if (group_element) {
                group_counter[name] = group_index + 1;
            }

            const value = getDataForElement(elem);
            if (null !== value) {
                const elem_indexed = changed_data.indexOf(name) >= 0;
                if ((isChangedElement(elem) || !options.changed) && !elem_indexed) {
                    changed_data.push(group_element ? group_name : name);
                }

                data[group_element ? group_name : name] = value;
            }
        }
    }

    // Finally, filter out only fields that *must* be assigned.
    const resulting_data = {
        set: {
        },
        del: [
        ]
    };

    for (const name in data) {
        if (!options.changed || (changed_data.indexOf(name) >= 0)) {
            resulting_data.set[name] = data[name];
        }
    }

    // Make sure to remove dynamic group entries from the kvs
    // Only group keys can be removed atm, so only process .settings-group
    if (options.cleanup) {
        for (let elem of document.getElementsByClassName("settings-group")) {
            for (let pair of getGroupPending(elem)) {
                const [action, index] = pair.split(":");
                if (action === "del") {
                    const keysRaw = elem.dataset["settingsSchema"]
                        || elem.dataset["settingsTarget"];
                    const keys = !keysRaw ? [] : keysRaw.split(" ");
                    keys.forEach((key) => {
                        resulting_data.del.push(`${key}${index}`);
                    });
                }
            }
        }
    }

    return resulting_data;
}

// TODO: <input type="radio"> is a special beast, since the actual value is one of 'checked' elements with the same name=... attribute.
// Right now, WebUI does not use this kind of input, but in case it does this needs a once-over that the actual input value is picked up correctly through all of changed / original comparisons.

// Not all of available forms are used for settings:
// - terminal input, which is implemented with an input field. it is attributed with `action="none"`, so settings handler never treats it as 'changed'
// - initial setup. it is shown programatically, but is still available from the global list of forms

export function getDataForElement(element) {
    switch (element.tagName) {
    case "INPUT":
        switch (element.type) {
        case "radio":
            if (element.checked) {
                return element.value;
            }
            return null;
        case "checkbox":
            return element.checked ? 1 : 0;
        case "number":
        case "text":
        case "password":
        case "hidden":
        case "range":
            // notice that we set directly to storage, thus strings are just fine
            return element.value;
        }
        break;
    case "SELECT":
        if (element.multiple) {
            return bitsetFromSelectedValues(element);
        } else if (element.selectedIndex >= 0) {
            return element.options[element.selectedIndex].value;
        }
    }

    return null;
}


function resetSettingsGroup() {
    const elems = document.getElementsByClassName("settings-group");
    for (let elem of elems) {
        resetChangedElement(elem);
        resetGroupPending(elem);
    }
}

function settingsTargets(elem) {
    let targets = elem.dataset["settingsTarget"];
    if (!targets) {
        return [];
    }

    return targets.split(" ");
}
function stringToBoolean(value) {
    return (value === "1")
        || (value === "y")
        || (value === "yes")
        || (value === "true")
        || (value === "on");
}

function booleanToString(value) {
    return value ? "true" : "false";
}

// When receiving / returning data, <select multiple=true> <option> values are treated as bitset (u32) indexes (i.e. individual bits that are set)
// For example 0b101 is translated to ["0", "2"], or 0b1111 is translated to ["0", "1", "2", "3"]
// Right now only `hbReport` uses such format, but it is not yet clear how such select element should behave when value is not an integer

function bitsetToSelectedValues(bitset) {
    let values = [];
    for (let index = 0; index < 31; ++index) {
        if (bitset & (1 << index)) {
            values.push(index.toString());
        }
    }

    return values;
}

function bitsetFromSelectedValues(select) {
    let result = 0;
    for (let option of select.selectedOptions) {
        result |= 1 << parseInt(option.value);
    }

    return result;
}

export function setInputValue(input, value) {
    switch (input.type) {
    case "radio":
        input.checked = (value === input.value);
        break;
    case "checkbox":
        input.checked =
            (typeof(value) === "boolean") ? value :
            (typeof(value) === "string") ? stringToBoolean(value) :
            (typeof(value) === "number") ? (value !== 0) : false;
        break;
    case "text":
    case "password":
    case "number":
        input.value = value;
        break;
    }
}

export function setSpanValue(span, value) {
    if (Array.isArray(value)) {
        value.forEach((text) => {
            setSpanValue(span, text);
            span.appendChild(document.createElement("br"));
        });
    } else {
        if (typeof(value) === "string") {
            value = span.dataset[`value${value.toUpperCase()}`] || value;
        }
        let content = "";
        if (span.attributes.pre) {
            content += span.attributes.pre.value;
        }
        content += value;
        if (span.attributes.post) {
            content += span.attributes.post.value;
        }
        span.textContent = content;
    }
}

export function setSelectValue(select, value) {
    const values = select.multiple
        ? bitsetToSelectedValues(value)
        : [value.toString()];

    Array.from(select.options)
        .filter((option) => values.includes(option.value))
        .forEach((option) => {
            option.selected = true;
        });

    select.dataset["original"] = values.join(",");
}

export function setOriginalsFromValuesForNode(node, elems) {
    if (elems === undefined) {
        elems = [...node.querySelectorAll("input,select")];
    }

    for (let elem of elems) {
        switch (elem.tagName) {
        case "INPUT":
            if (elem.type === "checkbox") {
                elem.dataset["original"] = booleanToString(elem.checked);
            } else {
                elem.dataset["original"] = elem.value;
            }
            break;
        case "SELECT":
            elem.dataset["original"] = stringifySelectedValues(elem);
            break;
        }
        resetChangedElement(elem);
    }
}

export function setOriginalsFromValues(elems) {
    setOriginalsFromValuesForNode(document, elems);
}

// automatically generate <select> options for know entities
const Enumerable = {};

// <select> initialization from simple {id: ..., name: ...} that map as <option> value=... and textContent
// To avoid depending on order of incoming messages, always store real value inside of dataset["original"] and provide a way to re-initialize every 'enumerable' <select> element on the page
//
// Notice that <select multiple> input and output format is u32 number, but the 'original' string is comma-separated <option> value=... attributes

export function initSelect(select, values) {
    for (let value of values) {
        let option = document.createElement("option");
        option.setAttribute("value", value.id);
        option.textContent = value.name;
        select.appendChild(option);
    }
}

export function initEnumerableSelect(select, callback) {
    for (let className of select.classList) {
        const prefix = "enumerable-";
        if (className.startsWith(prefix)) {
            const name = className.replace(prefix, "");
            if ((Enumerable[name] !== undefined) && Enumerable[name].length) {
                callback(select, Enumerable[name]);
            }
            break;
        }
    }
}

function refreshEnumerableSelect(name) {
    const selector = (name !== undefined)
        ? `select.enumerable.enumerable-${name}`
        : "select.enumerable";

    for (let select of document.querySelectorAll(selector)) {
        initEnumerableSelect(select, (_, enumerable) => {
            while (select.childElementCount) {
                select.removeChild(select.firstElementChild);
            }

            initSelect(select, enumerable);

            const original = select.dataset["original"];
            if (typeof original === "string" && original.length) {
                setSelectValue(select, original);
            }
        });
    }
}

export function getEnumerables(name) {
    return Enumerable[name];
}

export function addEnumerables(name, enumerables) {
    Enumerable[name] = enumerables;
    refreshEnumerableSelect(name);
}

export function addSimpleEnumerables(name, prettyName, count) {
    if (count) {
        let enumerables = [];
        for (let id = 0; id < count; ++id) {
            enumerables.push({"id": id, "name": `${prettyName} #${id}`});
        }

        addEnumerables(name, enumerables);
    }
}

// track <input> values, count total number of changes and their side-effects / needed actions
class SettingsBase {
    constructor() {
        this.counters = {};
        this.resetCounters();
        this.saved = false;
    }

    resetCounters() {
        this.counters.changed = 0;
        this.counters.reboot = 0;
        this.counters.reconnect = 0;
        this.counters.reload = 0;
    }
}

// Handle plain kv pairs when they are already on the page, and don't need special template handlers
// Notice that <span> uses a custom data attribute data-key=..., instead of name=...
export function initGenericKeyValueElement(key, value) {
    for (const span of document.querySelectorAll(`span[data-key='${key}']`)) {
        setSpanValue(span, value);
    }

    const inputs = [];
    for (const elem of document.querySelectorAll(`[name='${key}'`)) {
        switch (elem.tagName) {
        case "INPUT":
            setInputValue(elem, value);
            inputs.push(elem);
            break;
        case "SELECT":
            setSelectValue(elem, value);
            inputs.push(elem);
            break;
        }
    }

    setOriginalsFromValues(inputs);
}

const Settings = new SettingsBase();

export function onElementChange(event) {
    let action = event.target.dataset["action"];
    if ("none" === action) {
        return;
    }

    let originalValue = event.target.dataset["original"];
    let newValue;

    if ((event.target.tagName === "INPUT") && (event.target.type === "checkbox")) {
        originalValue = stringToBoolean(originalValue);
        newValue = event.target.checked;
    } else if (event.target.tagName === "SELECT") {
        newValue = stringifySelectedValues(event.target);
    } else {
        newValue = event.target.value;
    }

    if (typeof originalValue === "undefined") {
        return;
    }

    let changed = isChangedElement(event.target);
    if (newValue !== originalValue) {
        if (!changed) {
            ++Settings.counters.changed;
            if (action in Settings.counters) {
                ++Settings.counters[action];
            }
        }
        setChangedElement(event.target);
        greenifySave();
    } else {
        if (changed) {
            --Settings.counters.changed;
            if (action in Settings.counters) {
                --Settings.counters[action];
            }
        }
        resetChangedElement(event.target);
        greyoutSave();
    }
}

const __variable_listeners = {};

export function listenVariables(key, func) {
    if (__variable_listeners[key] === undefined) {
        __variable_listeners[key] = [];
    }

    __variable_listeners[key].push(func);
}

export function variableListeners(listeners) {
    for (const [key, listener] of Object.entries(listeners)) {
        listenVariables(key, listener);
    }
}

export function updateKeyValue(key, value) {
    const listeners = __variable_listeners[key];
    if (listeners !== undefined) {
        for (let listener of listeners) {
            listener(key, value);
        }
    }

    if (typeof(value) !== "object") {
        initGenericKeyValueElement(key, value);
    }
}

function onSaved(value) {
    Settings.saved = value;
}

function greyoutSave() {
    const elems = document.querySelectorAll(".button-save");
    for (let elem of elems) {
        elem.style.removeProperty("--save-background");
    }
}

function greenifySave() {
    const elems = document.querySelectorAll(".button-save");
    for (let elem of elems) {
        elem.style.setProperty("--save-background", "rgb(0, 192, 0)");
    }
}

function resetOriginals() {
    setOriginalsFromValues();
    resetSettingsGroup();

    Settings.resetCounters();
    Settings.saved = false;
}

function afterSaved() {
    var response;

    if (Settings.counters.reboot > 0) {
        response = window.confirm("You have to reboot the board for the changes to take effect, do you want to do it now?");
        if (response) {
            sendAction("reboot");
        }
    } else if (Settings.counters.reconnect > 0) {
        response = window.confirm("You have to reconnect to the WiFi for the changes to take effect, do you want to do it now?");
        if (response) {
            sendAction("reconnect");
        }
    } else if (Settings.counters.reload > 0) {
        response = window.confirm("You have to reload the page to see the latest changes, do you want to do it now?");
        if (response) {
            pageReloadIn(0);
        }
    }

    resetOriginals();
    greyoutSave();
}

function waitForSaved(){
    if (!Settings.saved) {
        setTimeout(waitForSaved, 1000);
    } else {
        afterSaved();
    }
}

export function applySettings(settings) {
    send(JSON.stringify({settings}));
}

export function applySettingsFromAllForms() {
    // Since we have 2-page config, make sure we select the active one
    let forms = document.getElementsByClassName("form-settings");
    if (validateForms(forms)) {
        applySettings(getData(forms));
        Settings.counters.changed = 0;
        waitForSaved();
    }

    return false;
}

function resetToFactoryDefaults(event) {
    event.preventDefault();

    let response = window.confirm("Are you sure you want to erase all settings from the device?");
    if (response) {
        sendAction("factory_reset");
    }
}

function handleSettingsFile(event) {
    event.preventDefault();

    const inputFiles = event.target.files;
    if (typeof inputFiles === "undefined" || inputFiles.length === 0) {
        return false;
    }

    const inputFile = inputFiles[0];
    event.target.value = "";

    if (!window.confirm("Previous settings will be overwritten. Are you sure you want to restore from this file?")) {
        return false;
    }

    const reader = new FileReader();
    reader.onload = function(event) {
        try {
            var data = JSON.parse(event.target.result);
            sendAction("restore", data);
        } catch (e) {
            notifyError(null, null, 0, 0, e);
        }
    };
    reader.readAsText(inputFile);
}

export function pendingChanges() {
    return Settings.counters.changed > 0;
}

function listeners() {
    return {
        "saved": (_, value) => {
            onSaved(value);
        },
    };
}

export function updateVariables(kvs) {
    Object.entries(kvs)
        .forEach(([key, value]) => {
            updateKeyValue(key, value);
        });
}

export function init() {
    variableListeners(listeners());

    document.getElementById("uploader")
        .addEventListener("change", handleSettingsFile);

    document.querySelector(".button-save")
        .addEventListener("click", (event) => {
            event.preventDefault();
            applySettingsFromAllForms();
        });

    document.querySelector(".button-settings-backup")
        .addEventListener("click", (event) => {
            event.preventDefault();
            const elem = document.getElementById("downloader");
            elem.href = configUrl().href;
            elem.click();
        });

    document.querySelector(".button-settings-restore")
        .addEventListener("click", () => {
            document.getElementById("uploader").click();
        });
    document.querySelector(".button-settings-factory")
        .addEventListener("click", resetToFactoryDefaults);

    document.querySelectorAll(".button-add-settings-group")
        .forEach((elem) => {
            elem.addEventListener("click", groupSettingsAdd);
        });

    // No group handler should be registered after this point, since we depend on the order
    // of registration to trigger 'after-add' handler and update group attributes *after*
    // module function finishes modifying the container
    for (const group of document.querySelectorAll(".settings-group")) {
        group.addEventListener("settings-group-del", onGroupSettingsEventDel);
        group.addEventListener("change", onElementChange);
    }

    for (let elem of document.querySelectorAll("input,select")) {
        elem.addEventListener("change", onElementChange);
    }

    resetOriginals();
}
