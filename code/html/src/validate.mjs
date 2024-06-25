import { isChangedElement } from './settings.mjs';

export function validatePassword(password) {
    // http://www.the-art-of-web.com/javascript/validate-password/
    // at least one lowercase and one uppercase letter or number
    // at least eight characters (letters, numbers or special characters)

    // MUST be 8..63 printable ASCII characters. See:
    // https://en.wikipedia.org/wiki/Wi-Fi_Protected_Access#Target_users_(authentication_key_distribution)
    // https://github.com/xoseperez/espurna/issues/1151

    const Pattern = /^(?=.*[A-Z\d])(?=.*[a-z])[\w~!@#$%^&*()<>,.?;:{}[\]\\|]{8,63}/;
    return (
        (password !== undefined)
        && (typeof password === "string")
        && (password.length > 0)
        && Pattern.test(password));
}

// Try to validate 'adminPass{0,1}', searching the first form containing both.
// In case it's default webMode, avoid checking things when both fields are empty (`required === false`)
export function validateFormsPasswords(forms, required) {
    let [passwords] = Array.from(forms).filter(
        form => form.elements.adminPass0 && form.elements.adminPass1);

    if (passwords) {
        let first = passwords.elements.adminPass0;
        let second = passwords.elements.adminPass1;

        if (!required && !first.value.length && !second.value.length) {
            return true;
        }

        let firstValid = first.checkValidity() && validatePassword(first.value);
        let secondValid = second.checkValidity() && validatePassword(second.value);
        if (firstValid && secondValid) {
            if (first.value === second.value) {
                return true;
            }

            alert("Passwords are different!");
            return false;
        }

        alert("The password you have entered is not valid, it must be 8..63 characters and have at least 1 lowercase and 1 uppercase / number!");
    }

    return false;
}


// Same as above, but only applies to the general settings page.
// Find the first available form that contains 'hostname' input
export function validateFormsHostname(forms) {
    // per. [RFC1035](https://datatracker.ietf.org/doc/html/rfc1035)
    // Hostname may contain:
    // - the ASCII letters 'a' through 'z' (case-insensitive),
    // - the digits '0' through '9', and the hyphen.
    // Hostname labels cannot begin or end with a hyphen.
    // No other symbols, punctuation characters, or blank spaces are permitted.
    let [hostname] = Array.from(forms).filter(form => form.elements.hostname);
    if (!hostname) {
        return true;
    }

    // Validation pattern is attached to the element itself, so just check that.
    // (and, we also re-use the hostname for fallback SSID, thus limited to 1...32 chars instead of 1...63)

    hostname = hostname.elements.hostname;
    let result = hostname.value.length
        && (!isChangedElement(hostname) || hostname.checkValidity());

    if (!result) {
        alert("Hostname cannot be empty and may only contain the ASCII letters ('A' through 'Z' and 'a' through 'z'), the digits '0' through '9', and the hyphen ('-')! They can neither start or end with an hyphen.");
    }

    return result;
}

export function validateForms(forms) {
    return validateFormsPasswords(forms) && validateFormsHostname(forms);
}
