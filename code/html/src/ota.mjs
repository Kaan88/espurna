import { notifyError } from './errors.mjs';
import { upgradeUrl } from './connection.mjs';
import { variableListeners } from './settings.mjs';

let FreeSize = 0;

/** 
 * @returns {HTMLInputElement} 
 */
function buttonUpgrade() {
    return document.querySelector(".button-upgrade");
}

/** 
 * @param {number} flash_mode
 * @returns string
 */
function describeFlashMode(flash_mode) {
    if ((0 <= flash_mode) && (flash_mode <= 3)) {
        const modes = ['QIO', 'QOUT', 'DIO', 'DOUT'];
        return `${modes[flash_mode]} (${flash_mode})`;
    }

    return `Unknown flash mode ${flash_mode}`;
}

/** 
 * @param {Uint8Array} buffer
 * @returns boolean
 */
function isGzip(buffer) {
    return (0x1f === buffer[0]) && (0x8b === buffer[1]);
}

/** 
 * @param {Uint8Array} buffer
 * @returns number
 */
function flashMode(buffer) {
    return buffer[2];
}

/** 
 * @param {Uint8Array} buffer
 * @returns boolean
 */
function checkMagic(buffer) {
    return 0xe9 === buffer[0];
}

/** 
 * @param {Uint8Array} buffer
 * @returns boolean
 */
function checkFlashMode(buffer) {
    return 0x03 === flashMode(buffer);
}

function notifyValueError(event) {
    notifyError(`ERROR while attempting OTA upgrade - XHR ${event.type}`, null, 0, 0, null);
}

/** 
 * @param {PointerEvent} event
 */
function onButtonClick(event) {
    event.preventDefault();

    const elem = document.querySelector("input[name='upgrade']");
    const file = elem.files[0];

    const data = new FormData();
    data.append("upgrade", file, file.name);

    const xhr = new XMLHttpRequest();

    xhr.addEventListener("error", notifyValueError, false);
    xhr.addEventListener("abort", notifyValueError, false);

    const progress = document.getElementById("upgrade-progress");
    xhr.addEventListener("load",
        () => {
            progress.style.display = "none";
            if ("OK" === xhr.responseText) {
                alert("Firmware image uploaded, board rebooting. This page will be refreshed in 5 seconds");
            } else {
                alert(`ERROR while attempting OTA upgrade - ${xhr.status.toString()} ${xhr.statusText}, ${xhr.responseText}`);
            }
        }, false);

    xhr.upload.addEventListener("progress",
        (event) => {
            progress.style.display = "inherit";
            if (event.lengthComputable) {
                progress.value = event.loaded;
                progress.max = event.total;
            }
        }, false);

    xhr.open("POST", upgradeUrl().href);
    xhr.send(data);
}

/** 
 * @param {number} size
 * @returns {number}
 */
function roundedSize(size) {
    return (size - (size % 4096)) + 4096;
}

/** 
 * @param {InputEvent} event
 */
async function onFileChanged(event) {
    event.preventDefault();

    const file = event.target.files[0];
    document.querySelector("input[name='filename']").value = file.name;

    const need = roundedSize(file.size);
    if ((FreeSize !== 0) && (need > FreeSize)) {
        alert(`OTA .bin cannot be uploaded. Need at least ${need}bytes of free space, ${FreeSize}bytes available.`);
        return;
    }

    const buffer = await file.slice(0, 3).arrayBuffer();
    const header = new Uint8Array(buffer);

    if (!isGzip(header)) {
        if (!checkMagic(header)) {
            alert("Invalid binary header, does not look like firmware .bin");
            return;
        }

        if (!checkFlashMode(header)) {
            alert(describeFlashMode(flashMode(header)));
        }
    }

    const button = buttonUpgrade();
    button.disabled = false;
}

function listeners() {
    return {
        "free_size": (_, value) => {
            FreeSize = parseInt(value, 10);
        },
    };
}

export function init() {
    variableListeners(listeners());

    const upgrade = document.querySelector("input[name='upgrade']");
    document.querySelector(".button-upgrade-browse")
        .addEventListener("click", () => {
            upgrade.click();
        });

    upgrade.addEventListener("change", onFileChanged);

    const button = buttonUpgrade();
    button.addEventListener("click", onButtonClick);
    button.disabled = true;
}
