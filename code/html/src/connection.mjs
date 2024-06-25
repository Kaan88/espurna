import { notifyError } from './errors.mjs';
import { pageReloadIn } from './core.mjs';

function makeWebSocketUrl(root) {
    let out = new URL("ws", root);
    out.protocol =
        (root.protocol === "https:")
            ? "wss:"
            : "ws:";

    return out;
}

function makeUrl(path, root) {
    let out = new URL(path, root);
    out.protocol = root.protocol;
    return out;
}

class UrlsBase {
    constructor() {
        this.auth = null;
        this.config = null;
        this.upgrade = null;
        this.ws = null;
    }

    update(root) {
        this.auth = makeUrl("auth", root);
        this.config = makeUrl("config", root);
        this.upgrade = makeUrl("upgrade", root);
        this.ws = makeWebSocketUrl(root);
    }
};

const Urls = new UrlsBase();

class ConnectionBase {
    constructor() {
        this.socket = null;
        this.ping_pong = null;
    }
};

ConnectionBase.prototype.open = function(href, onmessage) {
    this.socket = new WebSocket(href);
    this.socket.onopen = () => {
        this.ping_pong = setInterval(
            () => { sendAction("ping"); }, 5000);
    };
    this.socket.onclose = () => {
        clearInterval(this.ping_pong);
    };
    this.socket.onmessage = onmessage;
}

ConnectionBase.prototype.send = function(payload) {
    this.socket.send(payload);
}

const Connection = new ConnectionBase();

function onConnected(href, onmessage) {
    Connection.open(href, onmessage);
}

function onFetchError(error) {
    notifyError(null, null, error.lineNumber, error.columnNumber, error);
    pageReloadIn(5000);
}

function onError(response) {
    notifyError(`${response.url} responded with status code ${response.status}, reloading the page`, null, 0, 0, null);
    pageReloadIn(5000);
}

async function connectToURL(root, onmessage) {
    Urls.update(root);

    const opts = {
        'method': 'GET',
        'cors': true,
        'credentials': 'same-origin',
    };

    try {
        const response = await fetch(Urls.auth.href, opts);
        // Set up socket connection handlers
        if (response.status === 200) {
            onConnected(Urls.ws.href, onmessage);
        // Nothing to do, reload page and retry on errors
        } else {
            onError(response);
        }
    } catch (e) {
        onFetchError(e);
    }
}

async function onConnectEvent(event) {
    await connectToURL(event.detail.url, event.detail.onmessage);
}

function onSendEvent(event) {
    Connection.send(event.detail.data);
}

export function configUrl() {
    return Urls.config;
}

export function upgradeUrl() {
    return Urls.upgrade;
}

export function send(data) {
    if (data === undefined) {
        data = {};
    }
    window.dispatchEvent(
        new CustomEvent("app-send", {detail: {data}}));
}

export function sendAction(action, data) {
    send(JSON.stringify({action, data}));
}

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
