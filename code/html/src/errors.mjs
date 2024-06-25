export function showErrorNotification(message) {
    let container = document.getElementById("error-notification");
    if (container.childElementCount > 0) {
        return false;
    }

    container.style.display = "inherit";
    container.style.whiteSpace = "pre-wrap";

    let notification = document.createElement("div");
    notification.classList.add("pure-u-1");
    notification.classList.add("pure-u-lg-1");
    notification.textContent = message;

    container.appendChild(notification);

    return false;
}

export function notifyError(message, source, lineno, colno, error) {
    if (!source && error) {
        source = error.fileName;
    }

    if (!lineno && error) {
        lineno = error.lineNumber;
    }

    if (!colno && error) {
        colno = error.columnNumber;
    }

    let text = '';
    if (message) {
        text += message;
    }

    if (source && lineno && colno) {
        text += ` ${source}:${lineno}:${colno}:`;
    }

    if (error) {
        text += error.stack;
    }

    text += "\n\nFor more info see the Debug Log and / or Developer Tools console.";

    return showErrorNotification(text);
}
