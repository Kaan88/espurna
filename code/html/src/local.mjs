import { updateVariables } from './settings.mjs';

export function init() {
    updateVariables({
        webMode: 0,
        useWhite: false,
        useCCT: false,
        useColor: true,
        useRGB: true,
        now: "2024-01-01T00:00:00+01:00",
        light: {
            channels: ['r', 'g', 'b'],
        },
        gpioConfig: {
            types: [['hardware', 1]],
            hardware: [
                0,
                1,
                2,
                3,
                4,
                5,
                9,
                10,
                12,
                13,
                14,
                15,
                16,
            ],
        },
        curtainType: 0,
        curtainBoot: 0,
        relayConfig: {
            size: 5,
            start: 0,
            schema: [
                'relayName',
                'relayProv',
                'relayType',
                'relayGpioType',
                'relayGpio',
                'relayResetGpio',
                'relayBoot',
                'relayDelayOn',
                'relayDelayOff',
                'relayTime',
                'relayPulse',
                'relayTopicPub',
                'relayTopicSub',
                'relayTopicMode',
                'relayMqttDisc',
            ],
            values: [
                ['', 'gpio', 0, 1, 4, 153, 0, 0, 0, 0, 0, '', '', 0, 0],
                ['222', 'gpio', 0, 1, 5, 153, 0, 0, 0, 0, 0, '', '', 0, 0],
                ['', 'gpio', 0, 1, 12, 153, 0, 0, 0, 0, 0, '', '', 0, 0],
                ['444', 'gpio', 0, 1, 13, 153, 0, 0, 0, 0, 0, '', '', 0, 0],
                ['foo', 'gpio', 0, 1, 14, 153, 0, 0, 0, 0, 0, '', '', 0, 0],
            ],
        },
        ledConfig: {
            schema: [
                'ledGpio',
                'ledInv',
                'ledMode',
                'ledRelay',
            ],
            leds: [
                [15, 0, 'relay-inverse', 0],
                [16, 0, 'relay', 1],
            ],
        },
        rfm69Topic: 'foo',
        rfm69: {
            packets: 123,
            nodes: 456,
            schema: [
                'rfm69Node',
                'rfm69Key',
                'rfm69Topic',
            ],
            mapping: [
                [1, 'one', 'two'],
                [2, 'three', 'four'],
                [3, 'five', 'six'],
                [4, 'seven', 'eight'],
                [5, 'nine', 'ten'],
            ],
        },
        rfbRepeat: 25,
        rfbCount: 5,
        rfbRX: 1,
        rfbTX: 3,
    });
    updateVariables({
        hostname: 'localhost',
        desc: `${new URL(import.meta.url)}`,
        ssid: 'ESPURNA-12345',
        bssid: '11:22:33:aa:ee',
        channel: 13,
        rssi: -54,
        staip: '10.10.10.5',
        apip: '192.168.4.1',
        app_name: 'ESPurna',
        app_version: '9.9.9-dev',
        app_build: '9999-12-12 23:59:59',
        sketch_size: '555555',
        free_size: 493021,
        uptime: '5d 30m 13s',
        manufacturer: 'WEB',
        device: navigator.userAgent,
        chipid: window.name,
        sdk: 'WEB',
        core: 'WEB',
        heap: 999999,
        loadaverage: 99,
        vcc: '3.3',
        mqttStatus: true,
        ntpStatus: true,
        dczRelays: [
            0,
            0,
            0,
            0,
            0,
        ],
        tspkRelays: [
            0,
            0,
            0,
            0,
            0,
        ],
        light: {
            rgb: "#aaffaa",
            brightness: 100,
            state: true,
        },
        curtainState: {
            get: 20,
            set: 50,
            button: 0,
            moving: 0,
            type: 2,
        },
        rfb: {
            start: 0,
            codes: [
                ['aaaaaa', 'bbbbbb'],
                ['cccccc', 'dddddd'],
                ['ffffff', 'ffffff'],
                ['333333', '000000'],
                ['222222', '111111'],
            ],
        },
        rpnRules: [
            '2 2 +',
            '5 5 *',
            'yield',
            'rssi not',
            '$value $value + "foo/bar" mqtt.send'
        ],
        rpnTopics: {
            schema: [
                'rpnName',
                'rpnTopic',
            ],
            topics: [
                ['value', 'hello/world'],
                ['other', 'world/hello'],
            ],
        },
        schConfig: {
            schedules: [
                [2, 0, 'relay 0 1', '05,10:00'],
                [2, 0, 'relay 1 2', '05:00'],
                [1, 0, 'relay 2 0\nrelay 0 0', '10:00'],
            ],
            schema: [
                'schType',
                'schRestore',
                'schAction',
                'schTime',
            ],
            max: 4,
        },
        'magnitudes-init': {
            types: {
                schema: [
                    'type',
                    'prefix',
                    'name',
                ],
                values: [
                    [0, 'foo', 'Foo'],
                    [1, 'bar', 'Bar'],
                    [2, 'baz', 'Baz'],
                ],
            },
            errors: {
                schema: [
                    'type',
                    'name',
                ],
                values: [
                    [0, 'OK'],
                    [1, 'FAIL'],
                    [99, 'OUT_OF_RANGE'],
                ],
            },
            units: [
                [[1, 'C']],
                [[2, 'F']],
                [[3, 'K']],
            ],
        },
        'magnitudes-settings': {
            values: [
                [0,null,"NaN",0,0],
                [0,null,"NaN",0,0],
                [0,null,"NaN",0,0],
                [0,null,"NaN",0,0],
                [null,null,"NaN",0,0],
                [null,1,"NaN",0,0],
                [null,null,"NaN",0,0]
            ],
            schema: [
                "Correction",
                "Ratio",
                "ZeroThreshold",
                "MinDelta",
                "MaxDelta"
            ]
        },
        snsRealTime: false,
        snsRead: 6,
        snsInit: 10,
        snsReport: 10,
        snsSave: 0,
        thermostatMode: true,
        thermostatTmpUnits: 'C',
        thermostatOperationMode: 'local window',
        remoteTmp: '23',
        wifiConfig: {
            schema: [
                'ssid',
                'pass',
                'ip',
                'gw',
                'mask',
                'dns',
                'bssid',
                'chan',
            ],
            networks: [
                ['ESPURNA-12345', 'fibonacci', '', '', '', '', '', ''],
            ],
            max: 5,
        },
    });

    for (let module of ["dcz", "tspk"]) {
        updateVariables({
            'magnitudes-module': {
                prefix: module,
                values: [
                    [1, 0, 0],
                    [2, 0, 0],
                    [3, 0, 0],
                ],
                schema: [
                    "type",
                    "index_global",
                    "index_module"
                ]
            },
        });
    }

    updateVariables({
        'magnitudes-list': {
            schema: [
                'index_global',
                'type',
                'description',
                'units',
            ],
            values: [
                [0, 1, "Foo measurements", 1],
                [0, 2, "Bar simulation", 2],
                [0, 3, "Baz calculation", 2],
            ],
        },
    });

    updateVariables({
        magnitudes: {
            schema: [
                'value',
                'units',
                'error',
            ],
            values: [
                ["23", 1, 0],
                ["98", 2, 0],
                ["296", 3, 99],
            ],
        },
        relayState: {
            schema: [
                'status',
                'lock',
            ],
            values: [
                [0, 0],
                [1, 2],
                [0, 1],
                [1, 0],
                [1, 0],
            ],
        },
    });

    for (let n = 100; n < 200; n += 15) {
        updateVariables({
            scanResult: [
                'aa:bb:cc:ee:ff',
                `WPA${n / 100}PSK`,
                -Math.ceil(100 * Math.random()),
                Math.ceil(Math.random() * 13),
                `FooBar${n / 50}`,
            ],
            rfm69: {
                message: [
                    n,
                    456,
                    789,
                    'one',
                    'eno',
                    -127,
                    0,
                    0,
                ],
            },
        });
    }
}
