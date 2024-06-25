import { pendingChanges } from './settings.mjs';
import { sendAction } from './connection.mjs';

export function askSaveSettings(ask) {
    if (pendingChanges()) {
        return ask("There are pending changes to the settings, continue the operation without saving?");
    }

    return true;
}

export function askDisconnect(ask) {
    return ask("Are you sure you want to disconnect from the current WiFi network?");
}

export function askReboot(ask) {
    return ask("Are you sure you want to reboot the device?");
}

export function askAndCall(questions, callback) {
    for (let question of questions) {
        if (!question(window.confirm)) {
            return;
        }
    }

    callback();
}

export function askAndCallReconnect() {
    askAndCall([askSaveSettings, askDisconnect], () => {
        sendAction("reconnect");
    });
}

export function askAndCallReboot() {
    askAndCall([askSaveSettings, askReboot], () => {
        sendAction("reboot");
    });
}

export function askAndCallAction(event) {
    askAndCall([(ask) => ask(`Confirm the action: "${event.target.textContent}"`)], () => {
        sendAction(event.target.name);
    });
}

