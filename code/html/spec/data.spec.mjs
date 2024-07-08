import { assert, expect, test, beforeAll } from 'vitest';
import { addFromTemplate } from '../src/template.mjs';
import {
    checkAndSetElementChanged,
    getData,
    groupSettingsAdd,
    groupSettingsDel,
    initInputKeyValueElement,
    isChangedElement,
    setInputValue,
} from '../src/settings.mjs';

beforeAll(() => {
    document.body.innerHTML += `
    <form id="gather">
        <fieldset id="gather-plain">
            <legend>Plain kvs</legend>
            <input name="plainText" type="text"></input>
            <input name="plainNumber" type="number"></input>
            <input name="plainRange" type="range"></input>
            <input name="plainBox" type="checkbox"></input>
        </fieldset>
        <div id="gather-group">
        </div>
    </form>
    <template id="template-gather-group">
        <fieldset>
            <legend>Group <span data-key="template-id" data-pre="#"></span></legend>
            <input name="groupName" type="text"></input>
            <input name="groupValue" type="number"></input>
        </fieldset>
    </template>
    `;

    for (let name of ['modify', 'append', 'remove']) {
        document.body.innerHTML += `
        <form id="${name}">
            <div id="${name}-group" class="settings-group" data-settings-target="foo">
            </div>
        </form>
        <template id="template-${name}">
            <fieldset>
                <legend>Foo <span data-key="template-id" data-pre="#"></span></legend>
                <input name="foo" type="text"></input>
            </fieldset>
        </template>
        `;
    }
});

/**
 * @param {import('../src/settings.mjs').ElementValue} lhs
 * @param {import('../src/settings.mjs').DataValue} rhs
 */
function expectData(lhs, rhs) {
    if (typeof lhs === 'boolean') {
        assert(typeof rhs === 'number');
        expect(lhs).toEqual(!!rhs);
    } else {
        expect(lhs).toEqual(rhs);
    }
}

test('processed data can be gathered back', () => {
    const PLAIN = {
        plainText: 'foobar',
        plainNumber: 12345,
        plainRange: 74,
        plainBox: true,
    };

    const CFGS = [
        {groupName: 'one', groupValue: 1},
        {groupName: 'five', groupValue: 5},
        {groupName: 'nine', groupValue: 9},
        {groupName: 'fifty-five', groupValue: 55},
        {groupName: 'one-hundred', groupValue: 100},
    ];

    const plain = /** @type {HTMLFieldSetElement | null} */
        (document.getElementById('gather-plain'));
    assert(plain);

    for (let [key, value] of Object.entries(PLAIN)) {
        initInputKeyValueElement(key, value);
    }

    const group = /** @type {HTMLDivElement | null} */
        (document.getElementById('gather-group'));
    assert(group);

    for (let cfg of CFGS) {
        addFromTemplate(group, 'gather-group', cfg);
    }

    expect(group.childElementCount)
        .toEqual(CFGS.length);

    const form = /** @type {HTMLFormElement | null} */
        (document.getElementById('gather'));
    assert(form);

    const form_data = new FormData(form);
    expect(form_data.getAll('groupValue'));

    // retrieves everything, regardless of 'changed' state
    const data = getData([form], {assumeChanged: true});
    expect(data.del.length)
        .toEqual(0);

    const dataset = data.set;
    expect(Object.entries(dataset).length)
        .toEqual(Object.keys(PLAIN).length + (2 * CFGS.length));

    for (let [key, value] of Object.entries(PLAIN)) {
        expectData(value, dataset[key]);
    }

    CFGS.forEach((cfg, index) => {
        for (let [key, value] of Object.entries(cfg)) {
            expectData(value, dataset[`${key}${index}`]);
        }
    })
});

test('settings group modify', () => {
    const modify = /** @type {HTMLDivElement | null} */
        (document.getElementById('modify-group'));
    assert(modify);

    addFromTemplate(modify, 'modify', {foo: 'one'});
    addFromTemplate(modify, 'modify', {foo: 'two'});
    addFromTemplate(modify, 'modify', {foo: 'three'});

    const last = /** @type {HTMLInputElement | null} */
        (modify?.lastElementChild?.children[1]);
    assert(last);

    setInputValue(last, 'something else');
    assert(checkAndSetElementChanged(last));

    const first = /** @type {HTMLInputElement | null} */
        (modify?.firstElementChild?.children[1]);
    assert(first);

    setInputValue(first, 'complete opposite');
    assert(checkAndSetElementChanged(first));

    const form = /** @type {HTMLFormElement | null} */
        (document.getElementById('modify'));
    assert(form);

    const data = getData([form]);

    expect(data.del.length)
        .toEqual(0);

    expect(data.set)
        .toEqual({
            foo0: 'complete opposite',
            foo2: 'something else',
        });
});

test('settings group append', () => {
    const append = /** @type {HTMLDivElement | null} */
        (document.getElementById('append-group'));
    assert(append);

    addFromTemplate(append, 'append', {foo: 'first'});
    addFromTemplate(append, 'append', {foo: 'second'});
    addFromTemplate(append, 'append', {foo: 'third'});
    addFromTemplate(append, 'append', {foo: 'fourth'});
    expect(append.children.length).toEqual(4);

    addFromTemplate(append, 'append', {foo: 'fifth'});
    groupSettingsAdd(append);
    expect(append.children.length).toEqual(5);

    const last = /** @type {HTMLFieldSetElement | null} */
        (append?.lastElementChild);
    assert(last);

    const input = /** @type {HTMLInputElement | null} */
        (last.children[1]);
    assert(input);

    assert(isChangedElement(input));
    setInputValue(input, 'pending value');

    const form = /** @type {HTMLFormElement | null} */
        (document.getElementById('append'));
    assert(form);

    let data = getData([form]);

    expect(data.del.length)
        .toEqual(0);
    expect(data.set)
        .toEqual({
            foo4: 'pending value',
        });

    groupSettingsDel(append, last);
    expect(append.children.length).toEqual(4);

    data = getData([form]);

    expect(data.del.length)
        .toEqual(0);
    expect(data.set)
        .toEqual({});
});

test('settings group remove', () => {
    const remove = /** @type {HTMLDivElement | null} */
        (document.getElementById('remove-group'));
    assert(remove);

    addFromTemplate(remove, 'remove', {foo: '1111111'});
    addFromTemplate(remove, 'remove', {foo: '2222222'});
    addFromTemplate(remove, 'remove', {foo: '3333333'});
    addFromTemplate(remove, 'remove', {foo: '4444444'});
    expect(remove.children.length).toEqual(4);

    const second = /** @type {HTMLFieldSetElement | null} */
        (remove.children[1]);
    assert(second);

    groupSettingsDel(remove, second);
    expect(remove.children.length).toEqual(3);

    const form = /** @type {HTMLFormElement | null} */
        (document.getElementById('remove'));
    assert(form);

    let data = getData([form]);

    expect(data.del)
        .toEqual(['foo3']);
    expect(data.set)
        .toEqual({
            foo1: '3333333',
            foo2: '4444444',
        });

    addFromTemplate(remove, 'remove', {foo: '5555555'});
    groupSettingsAdd(remove);

    data = getData([form]);

    expect(data.del.length)
        .toEqual(0);
    expect(data.set)
        .toEqual({
            foo1: '3333333',
            foo2: '4444444',
            foo3: '5555555',
        });

    addFromTemplate(remove, 'remove', {foo: '6666666'});
    groupSettingsAdd(remove);

    data = getData([form]);

    expect(data.del.length)
        .toEqual(0);
    expect(data.set)
        .toEqual({
            foo1: '3333333',
            foo2: '4444444',
            foo3: '5555555',
            foo4: '6666666',
        });
});
