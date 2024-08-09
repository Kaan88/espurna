import { loadTemplate, mergeTemplate } from './template.mjs';

/**
 * @param {Error} error
 * @returns {string}
 */
export function formatError(error) {
    if (error.stack) {
        return error.stack;
    }

    return [
        error.name,
        error.message,
        "(no error stack)"
    ].join("\n");
}

/**
 * @param {string} source
 * @param {number} lineno
 * @param {number} colno
 */
export function formatSource(source, lineno, colno) {
    return `${source || "?"}:${lineno ?? "?"}:${colno ?? "?"}:`;
}

/** @type {number} */
let __errors = 0;

/**
 * @param {string} text
 */
export function showNotification(text) {
    const container = document.getElementById("error-notification");
    if (!container) {
        return;
    }

    __errors += 1;
    text += "\n\nFor more info see the Debug Log and / or Developer Tools console.";

    if (0 === container.childElementCount) {
        container.classList.add("show-error");
        mergeTemplate(
            container, loadTemplate("error-notification"));
    }

    container.children[0].textContent = text;
    if (0 !== container.childElementCount) {
        container.children[1].textContent =
            `\n(${__errors} unhandled errors so far)`;
    }
}

/**
 * @param {string} message
 * @param {string} source
 * @param {number} lineno
 * @param {number} colno
 * @param {any} error
 * @return {string}
 */
export function formatErrorEvent(message, source, lineno, colno, error) {
    let text = "";
    if (message) {
        text += message;
    }

    if (source || lineno || colno) {
        text += ` ${source || "?"}:${lineno ?? "?"}:${colno ?? "?"}:`;
    }

    if (error instanceof Error) {
        text += formatError(error);
    }

    return text;
}

/** @param {string} message */
export function notifyMessage(message) {
    showNotification(
        formatErrorEvent(message, "", 0, 0, null));
}

/** @param {Error} error */
export function notifyError(error) {
    showNotification(
        formatErrorEvent("", "", 0, 0, error));
}

/** @param {ErrorEvent} event */
export function notifyErrorEvent(event) {
    showNotification(
        formatErrorEvent(
            event.message,
            event.filename,
            event.lineno,
            event.colno,
            event.error));
}
