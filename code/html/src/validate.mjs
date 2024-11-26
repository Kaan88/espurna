import { isChangedElement, isIgnoredElement, getElements } from './settings.mjs';
import { showPanel } from './core.mjs';
import {
    formPassPair,
    filterForm as filterPasswordForm,
} from './password.mjs';

const DIFFERENT_PASSWORD = "Passwords are different!";
const EMPTY_PASSWORD = "Password cannot be empty!";
const INVALID_PASSWORD = "Invalid password!";

const CUSTOM_VALIDITY = "customValidity";

/**
 * @param {string} value
 * @returns {boolean}
 */
export function validatePassword(value) {
    // http://www.the-art-of-web.com/javascript/validate-password/
    // at least one lowercase and one uppercase letter or number
    // at least eight characters (letters, numbers or special characters)

    // MUST be 8..63 printable ASCII characters. See:
    // https://en.wikipedia.org/wiki/Wi-Fi_Protected_Access#Target_users_(authentication_key_distribution)
    // https://github.com/xoseperez/espurna/issues/1151

    const Pattern = /^(?=.*[A-Z\d])(?=.*[a-z])[\w~!@#$%^&*()<>,.?;:{}[\]\\|]{8,63}/;
    return ((typeof value === "string")
        && (value.length >= 8)
        && Pattern.test(value));
}

/**
 * @typedef {{strict?: boolean, assumeChanged?: boolean}} ValidationOptions
 */

/**
 * @typedef {[HTMLInputElement, HTMLInputElement]} PasswordInputPair
 */

/**
 * @param {import('./settings.mjs').InputOrSelect} elem
 * @param {function(HTMLElement): void} callback
 */
function findPanel(elem, callback) {
    const panel = elem.closest(".panel");
    if (!(panel instanceof HTMLElement)) {
        return;
    }

    callback(panel);
}

/**
 * @param {import('./settings.mjs').InputOrSelect} elem
 * @param {string} message
 */
export function reportValidityForInputOrSelect(elem, message = "") {
    findPanel(elem, (panel) => {
        showPanel(panel);

        if (message.length != 0) {
            elem.setCustomValidity(message);
            elem.dataset[CUSTOM_VALIDITY] = message;
        }

        elem.focus();
        elem.reportValidity();
    });
}

/**
 * @param {import('./settings.mjs').InputOrSelect} elem
 */
export function resetCustomValidity(elem) {
    delete elem.dataset[CUSTOM_VALIDITY];
    elem.setCustomValidity("");
}

/**
 * @param {import('./settings.mjs').InputOrSelect} elem
 * @returns {boolean}
 */
function validateInputOrSelect(elem) {
    if (elem.checkValidity()) {
        return true;
    }

    reportValidityForInputOrSelect(elem);
    return false;
}

/**
 * Try to validate password pair in the given list of forms. Alerts when validation fails.
 * With initial setup, this usually happens to be the only validation func w/ optional strict mode.
 * With normal panel, strict is expected to be false.
 * Only 'changed' elements affect validation, password fields can remain empty and still pass validation.
 * @param {HTMLFormElement[]} forms
 * @param {ValidationOptions} options
 * @returns {boolean}
 */
export function validateFormsPasswords(forms, {strict = true, assumeChanged = false} = {}) {
    const [form] = filterPasswordForm(forms);
    if (!form) {
        return true;
    }

    let err = "";

    const inputs = formPassPair(form);
    if (!inputs || inputs.length !== 2) {
        err = EMPTY_PASSWORD;
    } else if (assumeChanged || inputs.some(isChangedElement)) {
        if (!inputs[0].value.length || !inputs[1].value.length) {
            err = EMPTY_PASSWORD;
        } else if (inputs[0].value !== inputs[1].value) {
            err = DIFFERENT_PASSWORD;
        } else if (strict && !validatePassword(inputs[0].value)) {
            err = INVALID_PASSWORD;
        }
    }

    if (!err) {
        return true;
    }

    if (inputs.length === 2) {
        const first = inputs[0];
        findPanel(first, (panel) => {
            showPanel(panel);
            first.focus();
        });
    }

    alert(err);
    return false;
}

/**
 * @param {HTMLFormElement[]} forms
 * @returns {boolean}
 */
export function validateFormsReportValidity(forms) {
    const elems = forms
        .flatMap((form) => getElements(form))
        .filter((x) => isChangedElement(x) && !isIgnoredElement(x))

    if (!elems.length) {
        return false;
    }

    return elems.every(validateInputOrSelect);
}

/**
 * @param {HTMLFormElement[]} forms
 * @returns {boolean}
 */
export function validateForms(forms) {
    return validateFormsReportValidity(forms)
        && validateFormsPasswords(forms, {strict: false});
}
