import { randomString } from './core.mjs';
import { setChangedElement } from './settings.mjs';

function randomApiKey() {
    const elem = document.forms["form-admin"].elements.apiKey;
    elem.value = randomString(16, {hex: true});
    setChangedElement(elem);
}

export function init() {
    document.querySelector(".button-apikey")
        .addEventListener("click", randomApiKey);
}
