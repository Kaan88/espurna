import { notifyError, notifyMessage } from './errors.mjs';
import { pageReloadIn } from './core.mjs';

/** @typedef {{auth: URL, config: URL, upgrade: URL, ws: URL}} ConnectionUrls */

/**
 * @param {URL} root
 * @returns {URL}
 */
function makeWebSocketUrl(root) {
    const out = new URL("ws", root);
    out.protocol =
        (root.protocol === "https:")
            ? "wss:"
            : "ws:";

    return out;
}

/**
 * @param {string} path
 * @param {URL} root
 * @returns {URL}
 */
function makeUrl(path, root) {
    let out = new URL(path, root);
    out.protocol = root.protocol;
    return out;
}

/**
 * @param {URL} root
 * @returns ConnectionUrls
 */
function makeConnectionUrls(root) {
    return {
        auth: makeUrl("auth", root),
        config: makeUrl("config", root),
        upgrade: makeUrl("upgrade", root),
        ws: makeWebSocketUrl(root),
    };
}
class ConnectionBase {
    constructor() {
        /** @type {WebSocket | null} */
        this._socket = null;

        /** @type {number | null} */
        this._ping_pong = null;

        /** @type {ConnectionUrls | null} */
        this._urls = null;
    }

    /** @returns {boolean} */
    connected() {
        return this._ping_pong !== null;
    }

    /** @returns {ConnectionUrls | null} */
    urls() {
        return this._urls !== null
            ? Object.assign({}, this._urls)
            : null;
    }
};

/**
 * @typedef {function(MessageEvent): any} OnMessage
 */

/**
 * @param {ConnectionUrls} urls
 * @param {OnMessage} onmessage
 */
ConnectionBase.prototype.open = function(urls, onmessage) {
    this._socket = new WebSocket(urls.ws.href);
    this._socket.onopen = () => {
        this._ping_pong = setInterval(
            () => { sendAction("ping"); }, 5000);
    };
    this._socket.onclose = () => {
        if (this._ping_pong) {
            clearInterval(this._ping_pong);
            this._ping_pong = null;
        }
    };
    this._socket.onmessage = onmessage;
}

/**
 * @param {string} payload
 * @throws {Error}
 */
ConnectionBase.prototype.send = function(payload) {
    if (!this._socket) {
        throw new Error("WebSocket disconnected!");
    }

    this._socket.send(payload);
}

const Connection = new ConnectionBase();

/**
 * @returns {boolean}
 */
export function isConnected() {
    return Connection.connected();
}

/**
 * @returns {ConnectionUrls | null}
 */
export function connectionUrls() {
    return Connection.urls();
}

/**
 * @param {ConnectionUrls} urls
 * @param {OnMessage} onmessage
 */
function onAuthorized(urls, onmessage) {
    Connection.open(urls, onmessage);
}

/**
 * @param {Error} error
 */
function onFetchError(error) {
    notifyError(error);
    pageReloadIn(5000);
}

/**
 * @param {Response} response
 */
function onError(response) {
    notifyMessage(`${response.url} responded with status code ${response.status}, reloading the page`);
    pageReloadIn(5000);
}

/**
 * @param {URL} root
 * @param {OnMessage} onmessage
 */
async function connectToURL(root, onmessage) {
    const urls = makeConnectionUrls(root);

    /** @type {RequestInit} */
    const opts = {
        'method': 'GET',
        'credentials': 'same-origin',
        'mode': 'cors',
    };

    try {
        const response = await fetch(urls.auth.href, opts);
        // Set up socket connection handlers
        if (response.status === 200) {
            onAuthorized(urls, onmessage);
        // Nothing to do, reload page and retry on errors
        } else {
            onError(response);
        }
    } catch (e) {
        onFetchError(/** @type {Error} */(e));
    }
}

/** @param {Event} event */
async function onConnectEvent(event) {
    const detail = /** @type {CustomEvent<{url: URL, onmessage: OnMessage}>} */
        (event).detail;
    await connectToURL(
        detail.url, detail.onmessage);
}

/** @param {Event} event */
function onSendEvent(event) {
    Connection.send(/** @type {CustomEvent<{data: string}>} */
        (event).detail.data);
}

/** @param {string} data */
export function send(data) {
    window.dispatchEvent(
        new CustomEvent("app-send", {detail: {data}}));
}

/**
 * @param {string} action
 * @param {any?} data
 */
export function sendAction(action, data = {}) {
    send(JSON.stringify({action, data}));
}

/** @param {OnMessage} onmessage */
export function connect(onmessage) {
    // Optionally, support host=... param that could redirect to somewhere else
    // Note of the Cross-Origin rules that apply, and require device to handle them
    const search = new URLSearchParams(window.location.search);

    let host = search.get("host");
    if (host && !host.startsWith("http:") && !host.startsWith("https:")) {
        host = `http://${host}`;
    }

    const url = (host) ? new URL(host) : window.location;
    window.dispatchEvent(
        new CustomEvent("app-connect", {detail: {url, onmessage}}));
}

export function init() {
    window.addEventListener("app-connect", onConnectEvent);
    window.addEventListener("app-send", onSendEvent);
}
