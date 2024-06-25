import { createNodeList } from './relay.mjs';
import { variableListeners } from './settings.mjs';

function listeners() {
    return {
        "tspkRelays": (_, value) => {
            createNodeList("tspk-relays", value, "tspkRelay");
        },
    };
}

export function init() {
    variableListeners(listeners());
}
