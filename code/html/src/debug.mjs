import { send, sendAction } from './connection.mjs';
import { variableListeners } from './settings.mjs';

class CmdOutputBase {
    constructor(elem) {
        this.elem = elem;
        this.lastScrollHeight = elem.scrollHeight;
        this.lastScrollTop = elem.scrollTop;
        this.followScroll = true;

        elem.addEventListener("scroll", () => {
            // in case we adjust the scroll manually
            const current = this.elem.scrollHeight - this.elem.scrollTop;
            const last = this.lastScrollHeight - this.lastScrollTop;
            if ((current - last) > 16) {
                this.followScroll = false;
            }

            // ...and, in case we return to the bottom row
            const offset = current - this.elem.offsetHeight;
            if (offset < 16) {
                this.followScroll = true;
            }

            this.lastScrollHeight = this.elem.scrollHeight;
            this.lastScrollTop = this.elem.scrollTop;
        });
    }

    follow() {
        if (this.followScroll) {
            this.elem.scrollTop = this.elem.scrollHeight;
            this.lastScrollHeight = this.elem.scrollHeight;
            this.lastScrollTop = this.elem.scrollTop;
        }
    }

    clear() {
        this.elem.textContent = "";
        this.followScroll = true;
    }

    push(line) {
        this.elem.appendChild(new Text(line));
    }

    pushAndFollow(line) {
        this.elem.appendChild(new Text(`${line}\n`));
        this.followScroll = true
    }
}

let CmdOutput = null;

function listeners() {
    return {
        "log": (_, value) => {
            send("{}");

            const messages = value["msg"];
            if (messages === undefined) {
                return;
            }

            for (let msg of messages) {
                CmdOutput.push(msg);
            }

            CmdOutput.follow();
        },
    };
}

function onFormSubmit(event) {
    event.preventDefault();

    const line = event.target.elements.cmd.value;
    event.target.elements.cmd.value = "";

    CmdOutput.pushAndFollow(line);
    sendAction("cmd", {"line": `${line}\n`});
}

// While the settings are grouped using forms, actual submit is useless here
// b/c the data is intended to be sent with the websocket connection and never through some http endpoint
// *NOTICE* that manual event cancellation should happen asap, any exceptions will stop the specific
// handler function, but will not stop anything else left in the chain.
function disableFormSubmit(event) {
    event.preventDefault();
}

export function init() {
    variableListeners(listeners());

    CmdOutput = new CmdOutputBase(document.getElementById("cmd-output"));

    document.forms["form-debug"].addEventListener("submit", onFormSubmit);
    document.querySelectorAll("form:not([name='form-debug'])")
        .forEach((form) => {
            form.addEventListener("submit", disableFormSubmit);
        });

    document.querySelector(".button-dbg-clear")
        .addEventListener("click", (event) => {
            event.preventDefault();
            CmdOutput.clear();
        });
}
