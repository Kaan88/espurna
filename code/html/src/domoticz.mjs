import { createNodeList } from './relay.mjs';
import { variableListeners } from './settings.mjs';

function listeners() {
    return {
        "dczRelays": (_, value) => {
            createNodeList("dcz-relays", value, "dczRelayIdx");
        },
    };
}

export function init() {
    variableListeners(listeners());
}
