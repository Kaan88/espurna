import { isChangedElement } from './settings.mjs';

/**
 * @param {string} password
 * @returns {boolean}
 */
export function validatePassword(password) {
    // http://www.the-art-of-web.com/javascript/validate-password/
    // at least one lowercase and one uppercase letter or number
    // at least eight characters (letters, numbers or special characters)

    // MUST be 8..63 printable ASCII characters. See:
    // https://en.wikipedia.org/wiki/Wi-Fi_Protected_Access#Target_users_(authentication_key_distribution)
    // https://github.com/xoseperez/espurna/issues/1151

    const Pattern = /^(?=.*[A-Z\d])(?=.*[a-z])[\w~!@#$%^&*()<>,.?;:{}[\]\\|]{8,63}/;
    return ((typeof password === "string")
        && (password.length >= 8)
        && Pattern.test(password));
}

/**
 * Try to validate 'adminPass{0,1}', searching the first form containing both.
 * In case it's default webMode, avoid checking things when both fields are empty (`required === false`)
 * @param {HTMLFormElement[]} forms
 * @param {{required?: boolean, strict?: boolean}} options
 * @returns {boolean}
 */
export function validateFormsPasswords(forms, {required = true, strict = true} = {}) {
    const [first, second] = Array.from(forms)
        .flatMap((x) => {
            return [
                x.elements.namedItem("adminPass0"),
                x.elements.namedItem("adminPass1"),
            ];
        })
        .filter((x) => x instanceof HTMLInputElement);

    if (first && second) {
        if (!required && !first.value.length && !second.value.length) {
            return true;
        }

        if (first.value !== second.value) {
            alert("Passwords are different!");
            return false;
        }

        const firstValid = first.checkValidity()
            && (!strict || validatePassword(first.value));
        const secondValid = second.checkValidity()
            && (!strict || validatePassword(second.value));

        if (firstValid && secondValid) {
            return true;
        }
    }

    alert(`Invalid password!`);

    return false;
}

/**
 * Same as above, but only applies to the general settings page.
 * Find the first available form that contains 'hostname' input
 * @param {HTMLFormElement[]} forms
 */
export function validateFormsHostname(forms) {
    // per. [RFC1035](https://datatracker.ietf.org/doc/html/rfc1035)
    // Hostname may contain:
    // - the ASCII letters 'a' through 'z' (case-insensitive),
    // - the digits '0' through '9', and the hyphen.
    // Hostname labels cannot begin or end with a hyphen.
    // No other symbols, punctuation characters, or blank spaces are permitted.
    const [hostname] = Array.from(forms)
        .flatMap(form => form.elements.namedItem("hostname"))
        .filter((x) => x instanceof HTMLInputElement);
    if (!hostname) {
        return true;
    }

    // Validation pattern is attached to the element itself, so just check that.
    // (and, we also re-use the hostname for fallback SSID, thus limited to 1...32 chars instead of 1...63)

    const result = (hostname.value.length > 0)
        && (!isChangedElement(hostname) || hostname.checkValidity());
    if (!result) {
        alert(`Hostname cannot be empty and may only contain the ASCII letters ('A' through 'Z' and 'a' through 'z'),
            the digits '0' through '9', and the hyphen ('-')! They can neither start or end with an hyphen.`);
    }

    return result;
}

/**
 * @param {HTMLFormElement[]} forms
 */
export function validateForms(forms) {
    return validateFormsPasswords(forms, {strict: false})
        && validateFormsHostname(forms);
}
