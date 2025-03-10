/*

WIFI MODULE

Copyright (C) 2016-2019 by Xose Pérez <xose dot perez at gmail dot com>

*/

#pragma once

#include <Arduino.h>

#include <lwip/init.h>
#if LWIP_VERSION_MAJOR == 1
#include <netif/etharp.h>
#elif LWIP_VERSION_MAJOR >= 2
#include <lwip/etharp.h>
#endif

// (HACK) allow us to use internal lwip struct.
// esp8266 re-defines enum values from tcp header... include them first
#define LWIP_INTERNAL
#include <ESP8266WiFi.h>
#undef LWIP_INTERNAL

extern "C" {
  #include <lwip/opt.h>
  #include <lwip/ip.h>
  #include <lwip/tcp.h>
  #include <lwip/inet.h> // ip_addr_t
  #include <lwip/err.h> // ERR_x
  #include <lwip/dns.h> // dns_getserver, dns_gethostbyname
  #include <lwip/ip_addr.h> // ip4/ip6 helpers
};

// ref: https://github.com/me-no-dev/ESPAsyncTCP/pull/115/files#diff-e2e636049095cc1ff920c1bfabf6dcacR8
// This is missing with Core 2.3.0 and is sometimes missing from the build flags. Assume HIGH_BANDWIDTH version.
#ifndef TCP_MSS
#define TCP_MSS (1460)
#endif

namespace espurna {
namespace wifi {

using Mac = std::array<uint8_t, 6>;

struct StaNetwork {
    Mac bssid;
    String ssid;
    String passphrase;
    int8_t rssi;
    uint8_t channel;
};

struct SoftApNetwork {
    Mac bssid;
    String ssid;
    String passphrase;
    uint8_t channel;
    AUTH_MODE authmode;
};

enum class Event {
    Initial,              // aka boot
    Mode,                 // when opmode changes
    StationInit,          // station initialized by the connetion routine
    StationScan,          // scanning before the connection
    StationConnecting,    // network was selected and connection is in progress
    StationConnected,     // successful connection
    StationDisconnected,  // disconnected from the current network
    StationTimeout,       // timeout after the previous connecting state
    StationReconnect      // timeout after all connection loops failed
};

using EventCallback = void(*)(Event event);

enum class BootMode {
    Disabled,
    Enabled,
};

enum class StaMode {
    Disabled,
    Enabled,
};

enum class ApMode {
    Disabled,
    Enabled,
    Fallback,
};

} // namespace wifi

namespace settings {
namespace internal {

String serialize(wifi::Mac);

} // namespace internal
} // namespace settings
} // namespace espurna

// Note that 'connected' status is *only* for the WiFi STA.
// Overall connectivity depends on low-level network stack and it may be
// useful to check whether relevant interfaces are up and have a routable IP
// instead of exclusively depending on the WiFi API.
// (e.g. when we have injected ethernet, wireguard, etc. interfaces.
// esp8266 implementation specifically uses lwip, ref. `netif_list`)
bool wifiConnected();

// When AP is up and running
espurna::wifi::SoftApNetwork wifiApInfo();
bool wifiConnectable();
size_t wifiApStations();
IPAddress wifiApIp();

// Current STA connection
espurna::wifi::StaNetwork wifiStaInfo();
String wifiStaSsid();
IPAddress wifiStaIp();

// Request to change the current STA / AP status
// Current state persists until reset or configuration reload
void wifiStartAp();
void wifiToggleAp();
void wifiToggleSta();

// Disconnects STA intefrace, will trigger reconnection
void wifiDisconnect();

// Toggle WiFi modem
bool wifiDisabled();
void wifiDisable();

void wifiTurnOff();
void wifiTurnOn();

// Trigger fallback check for the AP
void wifiApCheck();

void wifiRegister(espurna::wifi::EventCallback);
void wifiSetup();
