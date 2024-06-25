export function configUrl(): any;
export function upgradeUrl(): any;
export function connectToURL(root: any, onmessage: any): Promise<void>;
export function connectToHost(host: any, onmessage: any): Promise<void>;
export function connectToCurrentURL(onmessage: any): Promise<void>;
export function send(payload: any): void;
export function sendAction(action: any, data: any): void;
