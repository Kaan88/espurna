/*

ESP8266 file system builder

Copyright (C) 2016-2019 by Xose Pérez <xose dot perez at gmail dot com>
Copyright (C) 2019-2024 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

// -----------------------------------------------------------------------------
// Dependencies
// -----------------------------------------------------------------------------

import {
    dest as destination,
    parallel,
    src as source,
} from 'gulp';

import { inlineSource } from 'inline-source';
import { build as esbuildBuild } from 'esbuild';
import { minify as htmlMinify } from 'html-minifier-terser';
import { JSDOM } from 'jsdom';

import { default as gzip } from 'gulp-gzip';
import { default as rename } from 'gulp-rename';
import { default as replace } from 'gulp-replace';

import * as through from 'through2';

import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

/**
 * through2.obj return value, wrapper for real node type
 * @typedef {import("node:stream").Transform} StreamTransform
 */

/**
 * module names used internally, usually as element class-name
 * @typedef {{[k: string]: boolean}} Modules
 */

/**
 * helper functions that deal with 'module' elements
 * @typedef {function(JSDOM): boolean} HtmlModify
 */

/**
 * declare some modules as optional, only to be included for specific builds
 * @constant
 * @type Modules
 */
const DEFAULT_MODULES = {
    'api': true,
    'cmd': true,
    'curtain': false,
    'dbg': true,
    'dcz': true,
    'garland': false,
    'ha': true,
    'idb': true,
    'led': true,
    'light': false,
    'lightfox': false,
    'local': false,
    'mqtt': true,
    'nofuss': true,
    'ntp': true,
    'ota': true,
    'relay': true,
    'rfb': false,
    'rfm69': false,
    'rpn': true,
    'sch': true,
    'sns': false,
    'thermostat': false,
    'tspk': true,
};

const MODULES_ALL = Object.fromEntries(
    Object.entries(DEFAULT_MODULES).map(
        ([key, _]) => {
            if ('local' === key) {
                return [key, false];
            }

            return [key, true];
        }));

// webui_serve does not also start ws server, but intead runs some local-only code
const MODULES_LOCAL =
    Object.assign(MODULES_ALL, {local: true});

/**
 * generic output, usually this includes a single module
 * @constant
 * @type {Modules}
 */
const NAMED_BUILD = {
    'curtain': 'curtain',
    'garland': 'garland',
    'light': 'light',
    'lightfox': 'lightfox',
    'rfbridge': 'rfb',
    'rfm69': 'rfm69',
    'sensor': 'sns',
    'thermostat': 'thermostat',
};

// vendored sources from node_modules/ need explicit paths
const NODE_DIR = path.join('node_modules');

// importmap manifest for dev server. atm, explicit list
// TODO import.meta.resolve wants umd output for some reason
const IMPORT_MAP = {
    '@jaames/iro': '/@jaames/iro/dist/iro.es.js',
};

// input sources, making sure relative paths start here
const SRC_DIR = path.join('html', 'src');

// output .html.gz, after inlining and compressing everything
const DATA_DIR = path.join('espurna', 'data');

// .h compiled from the .html.gz, can be used as a u8 blob inside of the firmware
const STATIC_DIR = path.join('espurna', 'static');

// main source file used by inline-source
const ENTRYPOINT = path.join(SRC_DIR, 'index.html')

// -----------------------------------------------------------------------------
// Methods
// -----------------------------------------------------------------------------

/**
 * @param {import("html-minifier-terser").Options} options
 * @returns {StreamTransform}
 */
function toMinifiedHtml(options) {
    return through.obj(async function (source, _, callback) {
        if (!source.isNull()) {
            const contents = source.contents.toString();
            source.contents = Buffer.from(
                await htmlMinify(contents, options));
        }

        callback(null, source);
    });
}

/*
 * @param {string} name
 * @returns {string}
 */
function safename(name) {
    return path.basename(name).replaceAll('.', '_');
}

/*
 * @param {string} name
 * @returns {StreamTransform}
 */
function toHeader(name) {
    return through.obj(function (source, _, callback) {
        // generates c++-friendly header output from raw binary data
        let output = `alignas(4) static constexpr uint8_t ${safename(name)}[] PROGMEM = {`;
        for (let i = 0; i < source.contents.length; i++) {
            if (i > 0) { output += ','; }
            if (0 === (i % 20)) { output += '\n'; }
            output += '0x' + ('00' + source.contents[i].toString(16)).slice(-2);
        }
        output += '\n};\n';

        // replace source stream with a different one, also replacing contents
        let dest = source.clone();
        dest.path = `${source.path}.h`;
        dest.contents = Buffer.from(output);

        callback(null, dest);
    });
}

/*
 * @returns {StreamTransform}
 */
function logSource() {
    return through.obj(function (source, _, callback) {
        console.info(`${path.basename(source.path)}\tsize: ${source.contents.length} bytes`);
        callback(null, source);
    });
}

/*
 * by default, destination preserves stat.*time of the source. which is obviosly bogus here as gulp
 * only knows about the entrypoint and not about every include happenning through inline-source
 * @returns {StreamTransform}
 */
function adjustFileStat() {
    return through.obj(function(source, _, callback) {
        const now = new Date();
        source.stat.atime = now;
        source.stat.mtime = now;
        source.stat.ctime = now;
        callback(null, source);
    });
}

/*
 * ref. https://github.com/evanw/esbuild/issues/1895
 * from our side, html/src/*.mjs (with the exception of index.mjs) require 'init()' call to be actually set up and used
 * as the result, no code from the module should be bundled into the output when module was not initialized
 * however, since light module depends on iro.js and does not have `sideEffects: false` in package.json, it would still get bundled because of top-level import
 * (...and since module modifying something in global scope is not unheard of...)
 * @returns {import("esbuild").Plugin}
 */
function forceNoSideEffects() {
    return {
        name: 'no-side-effects',
        setup(build) {
            build.onResolve({filter: /@jaames\/iro/, namespace: 'file'},
                async ({path, ...options}) => {
                    const result = await build.resolve(path, {...options, namespace: 'noRecurse'});
                    return {...result, sideEffects: false};
                });
        },
    };
}

/**
 * ref. html/src/index.mjs
 * TODO exportable values, e.g. in build.mjs? right now, false-positive of undeclared values, plus see 'forceNoSideEffects()'
 * @param {{[k: string]: boolean}} modules
 * @returns {{[k: string]: boolean}}
 */
function makeDefine(modules) {
    return Object.fromEntries(
        Object.entries(modules).map(
            ([key, value]) => {
                return [`MODULE_${key.toUpperCase()}`, value.toString()];
            }));
}

/**
 * @param {string} sourcefile
 * @param {string} contents
 * @param {string} resolveDir
 * @param {{[k: string]: boolean}} modules
 * @param {boolean} minify
 */
async function inlineJavascriptBundle(sourcefile, contents, resolveDir, define, minify) {
    return await esbuildBuild({
        stdin: {
            contents,
            loader: 'js',
            resolveDir,
            sourcefile,
        },
        bundle: true,
        plugins: [
            forceNoSideEffects(),
        ],
        define,
        minify,
        platform: minify
            ? 'browser'
            : 'neutral',
        external: minify
            ? undefined
            : ['./*.mjs'],
        write: false,
    });
}

/**
 * @param {string} srcdir
 * @param {{[k: string]: boolean}} modules
 * @param {boolean} compress
 * @return {import("inline-source").Handler}
 */
function inlineHandler(srcdir, modules, compress) {
    return async function(source) {
        // TODO split handlers
        if (source.content) {
            return;
        }

        // specific elements can be excluded at this point
        // (although, could be handled by jsdom afterwards; top elem does not usually have classList w/ module)
        for (let module of source.props?.module?.split(',') ?? []) {
            if (!modules[module]) {
                source.content = Buffer.from('');
                source.replace = '<div></div>';
                return;
            }
        }

        // main entrypoint of the app, usually a script bundle
        if (source.format === 'mjs') {
            const define = makeDefine(modules);

            const result = await inlineJavascriptBundle(
                source.sourcepath,
                source.fileContent,
                srcdir, define, compress);
            if (!result.outputFiles.length) {
                throw 'js bundle cannot be empty';
            }

            let content = Buffer.from(result.outputFiles[0].contents);

            if (!compress) {
                let prepend = '';
                for (const [key, value] of Object.entries(define)) {
                    prepend += `const ${key} = ${value};\n`;
                }

                content = Buffer.concat([
                    Buffer.from(prepend), content]);
            }

            source.content = content;
            return;
        }

        // <object type=text/html>. not handled by inline-source directly, only image blobs are expected
        if (source.props.raw) {
            source.content = source.fileContent;
            source.replace = source.content.toString();
            source.format = 'text';
            return;
        }

        // TODO import svg icon?
        if (source.sourcepath === 'favicon.ico') {
            source.format = 'x-icon';
            return;
        }
    };
}

/**
 * @param {HtmlModify[]} handlers
 * @return {StreamTransform}
 */
function modifyHtml(handlers) {
    return through.obj(function (source, _, callback) {
        const dom = new JSDOM(source.contents, {includeNodeLocations: true});

        let changed = false;

        for (let handler of handlers) {
            if (handler(dom)) {
                changed = true;
            }
        }

        if (changed) {
            source.contents = Buffer.from(dom.serialize());
        }

        callback(null, source);
    });
}

/**
 * optionally inject external libs paths
 * @return {HtmlModify}
 */
function injectVendor(compress) {
    return function(dom) {
        if (compress) {
            return false;
        }

        const script = dom.window.document.getElementsByTagName('script');

        const importmap = dom.window.document.createElement('script');
        importmap.setAttribute('type', 'importmap');
        importmap.textContent = JSON.stringify({imports: IMPORT_MAP});

        const head = dom.window.document.getElementsByTagName('head')[0];
        head.insertBefore(importmap, script[0]);

        return true;
    }
}

/**
 * generally a good idea to mitigate "tab-napping" attacks
 * per https://www.chromestatus.com/feature/6140064063029248
 * @return {HtmlModify}
 */
function externalBlank() {
    return function(dom) {
        let changed = false;

        for (let elem of dom.window.document.getElementsByTagName('a')) {
            if (elem.href.startsWith('http')) {
                elem.setAttribute('target', '_blank');
                elem.setAttribute('rel', 'noopener');
                elem.setAttribute('tabindex', '-1');
                changed = true;
            }
        }

        return changed;
    }
}

/**
 * with an explicit list of modules, strip anything not used by the bundle
 * @param {Modules} modules
 * @returns {HtmlModify}
 */
function stripModules(modules) {
    return function(dom) {
        let changed = false;

        for (const [module, value] of Object.entries(modules)) {
            if (value) {
                continue;
            }

            const className = `module-${module}`;
            for (let elem of dom.window.document.getElementsByClassName(className)) {
                elem.classList.remove(className);

                let remove = true;
                for (let name of elem.classList) {
                    if (name.startsWith('module-')) {
                        remove = false;
                        break;
                    }
                }

                if (remove) {
                    elem.parentElement.removeChild(elem);
                    changed = true;
                }
            }
        }

        return changed;
    }
}

/**
 * @param {string} srcdir
 * @param {Modules} modules
 * @param {boolean} compress
 * @return {StreamTransform}
 */
function makeInlineSource(srcdir, modules, compress) {
    return through.obj(async function (source, _, callback) {
        if (source.isNull()) {
            callback(null, source);
            return;
        }

        const result = await inlineSource(
            source.contents.toString(),
            {
                'compress': compress,
                'handlers': [inlineHandler(srcdir, modules, compress)],
                'rootpath': srcdir,
            });

        source.contents = Buffer.from(result);

        callback(null, source);
    });
}

/**
 * @param {string} name
 * @param {Modules} modules
 * @param {boolean} compress
 * @return {NodeJS.ReadWriteStream}
 */
function buildHtml(name, modules, compress) {
    if (modules === undefined) {
        modules = Object.assign({}, DEFAULT_MODULES);
        modules[NAMED_BUILD[name]] = true;
    }

    if (modules === undefined) {
        throw `'modules' argument / NAMED_BUILD['${name}'] is missing`;
    }

    const out = source(ENTRYPOINT)
        .pipe(makeInlineSource(SRC_DIR, modules, compress))
        .pipe(modifyHtml([
            injectVendor(compress),
            stripModules(modules),
            externalBlank(),
        ]));

    if (compress) {
        return out.pipe(
            toMinifiedHtml({
                collapseWhitespace: true,
                removeComments: true,
                minifyCSS: true,
                minifyJS: false
            })).pipe(
                replace('pure-', 'p-'));
    }

    return out;
}

/**
 * @param {string} name
 * @param {NodeJS.ReadWriteStream} stream
 * @return {NodeJS.ReadWriteStream}
 */
function buildOutputs(name, stream) {
    return stream
        .pipe(gzip({gzipOptions: {level: 9 }}))
        .pipe(adjustFileStat())
        .pipe(rename(`index.${name}.html.gz`))
        .pipe(logSource())
        .pipe(destination(DATA_DIR))
        .pipe(toHeader('webui_image'))
        .pipe(destination(STATIC_DIR));
}

/**
 * @param {string} name
 * @param {{[k: string]: boolean}} modules
 * @param {boolean} compress
 * @return {NodeJS.ReadWriteStream}
 */
function buildWebUI(name, modules, compress = true) {
    return buildOutputs(name, buildHtml(name, modules, compress));
}

/**
 * @param {string} name
 * @param {{[k: string]: boolean}} modules
 */
function serveWebUI(name, modules) {
    const server = http.createServer();

    function responseJsFile(response, path) {
        response.writeHead(200, {
            'Content-Type': 'text/javascript',
        });
        fs.createReadStream(path)
            .pipe(response);
    }

    fs.access

    server.on('request', (request, response) => {
        const url = new URL(`http://localhost${request.url}`);

        // serve bundled html as-is, do not minify
        switch (url.pathname) {
        case '/':
        case '/index.htm':
        case '/index.html':
            buildHtml(name, modules, false).pipe(
                through.obj(function(source, _, callback) {
                    response.writeHead(200, {
                        'Content-Type': 'text/html',
                        'Content-Length': source.contents.length,
                    });

                    response.write(source.contents);
                    response.end();

                    callback(null, source);
            }));

            return;
        }

        // when module files need browser repl. note the bundling scope,
        // only this way modules are actually modules and not inlined

        // external libs should be searched in node_modules/
        for (let value of Object.values(IMPORT_MAP)) {
            if (value === url.pathname) {
                responseJsFile(response, path.join(NODE_DIR, name));
                return;
            }
        }

        // everything else is attempted as html/src/${module}.mjs
        if (url.pathname.endsWith('.mjs')) {
            const name = url.pathname.split('/').at(-1);
            responseJsFile(response, path.join(SRC_DIR, name));
            return;
        }

        response.writeHead(500, {'content-type': 'text/plain'});
        response.end('500');
    });

    server.on('listening', () => {
        console.log(`Serving ${SRC_DIR} index and *.mjs at`, server.address());
    });

    server.listen(8080, 'localhost');
}

// -----------------------------------------------------------------------------
// Tasks
// -----------------------------------------------------------------------------

export function webui_serve() {
    return serveWebUI('all', MODULES_LOCAL);
}

export function webui_all() {
    return buildWebUI('all', MODULES_ALL);
}

export function webui_small() {
    return buildWebUI('small', DEFAULT_MODULES);
}

export function webui_curtain() {
    return buildWebUI('curtain');
}

export function webui_garland() {
    return buildWebUI('garland');
}

export function webui_light() {
    return buildWebUI('light');
}

export function webui_lightfox() {
    return buildWebUI('lightfox');
}

export function webui_rfbridge() {
    return buildWebUI('rfbridge');
}

export function webui_rfm69() {
    return buildWebUI('rfm69');
}

export function webui_sensor() {
    return buildWebUI('sensor');
}

export function webui_thermostat() {
    return buildWebUI('thermostat');
}

export default
    parallel(
        webui_all,
        webui_small,
        webui_curtain,
        webui_garland,
        webui_light,
        webui_lightfox,
        webui_rfbridge,
        webui_rfm69,
        webui_sensor,
        webui_thermostat);
