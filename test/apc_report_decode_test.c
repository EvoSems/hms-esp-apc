#include "apc_hid_parser.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)

uint32_t xTaskGetTickCount(void) { return 1234; }
size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t n = strlen(src);
    if (size) {
        size_t copy = n < size - 1 ? n : size - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return n;
}

int main(void)
{
    ups_metrics_t m;
    memset(&m, 0, sizeof(m));
    apc_hid_parser_init();

    /* Identity strings can contain a model plus appended firmware; the target
     * model override must remain a clean model string for HA discovery. */
    apc_hid_set_string(APC_STR_MODEL, "Back-UPS XS 1000M FW:945.d11 .D USB FW:");
    CHECK(strcmp(apc_hid_get_metrics()->ups_model, "Back-UPS XS 1000M") == 0,
          "APC product string strips appended firmware from model");
    apc_hid_set_string(APC_STR_MODEL, "Smart-UPS SRT2200XLI");
    CHECK(strcmp(apc_hid_get_metrics()->ups_model, "Smart-UPS SRT2200XLI") == 0,
          "corrected SRT2200XLI model string is retained cleanly");
    CHECK(apc_hid_get_metrics()->input_voltage_nominal == 230.0f,
          "SRT2200XLI model specification sets nominal input voltage to 230 V");
    CHECK(apc_hid_get_metrics()->nominal_power == 1980.0f,
          "SRT2200XLI model specification sets rated output power to 1980 W");

    const uint8_t charge[] = {0x0C, 0x42, 0x00};
    const uint8_t runtime[] = {0x0D, 0x35, 0x70, 0x00, 0x00};
    const uint8_t manufacture_date[] = {0x09, 0x23, 0x51};
    const uint8_t voltage[] = {0x0B, 0x27, 0x20};
    const uint8_t nominal_voltage[] = {0x0A, 0x20, 0x1C};
    const uint8_t low_runtime[] = {0x08, 0x3C, 0x00};
    const uint8_t warning[] = {0x0F, 0x32, 0x00};
    const uint8_t low_charge[] = {0x11, 0x0A};
    const uint8_t beeper[] = {0x14, 0x01};
    const uint8_t status[] = {0x07, 0x0D, 0x00};

    CHECK(apc_hid_parse_report(0x0C, charge, sizeof(charge), &m), "charge report accepted");
    CHECK(m.battery_charge == 66.0f, "charge decoded as 66 percent");
    CHECK(apc_hid_parse_report(0x0D, runtime, sizeof(runtime), &m), "runtime report accepted");
    CHECK(m.battery_runtime == 28725.0f, "runtime decoded as little-endian seconds");
    CHECK(apc_hid_parse_report(0x09, manufacture_date, sizeof(manufacture_date), &m), "manufacture date report accepted");
    CHECK(strcmp(m.battery_mfr_date, "2020/09/03") == 0, "manufacture date decoded from packed HID date");
    CHECK(apc_hid_parse_report(0x0B, voltage, sizeof(voltage), &m), "voltage report accepted");
    CHECK(fabsf(m.battery_voltage - 82.31f) < 0.01f, "voltage decoded as 82.31 volts");
    CHECK(apc_hid_parse_report(0x0A, nominal_voltage, sizeof(nominal_voltage), &m), "nominal voltage report accepted");
    CHECK(m.battery_nominal_voltage == 72.0f, "nominal voltage decoded as 72 volts");
    CHECK(apc_hid_parse_report(0x08, low_runtime, sizeof(low_runtime), &m), "low runtime report accepted");
    CHECK(m.low_battery_runtime_threshold == 60.0f, "low runtime threshold decoded as 60 seconds");
    CHECK(apc_hid_parse_report(0x0F, warning, sizeof(warning), &m), "warning threshold report accepted");
    CHECK(m.battery_warning_threshold == 50.0f, "warning threshold decoded as 50 percent");
    CHECK(apc_hid_parse_report(0x11, low_charge, sizeof(low_charge), &m), "low charge report accepted");
    CHECK(m.low_battery_charge_threshold == 10.0f, "low charge threshold decoded as 10 percent");
    CHECK(apc_hid_parse_report(0x14, beeper, sizeof(beeper), &m), "beeper report accepted");
    CHECK(strcmp(m.beeper_status, "disabled") == 0, "beeper control value 1 maps to disabled");
    CHECK(apc_hid_parse_report(0x07, status, sizeof(status), &m), "status report accepted");
    CHECK(m.status.online && m.status.charging && !m.status.discharging && !m.status.low_battery,
          "status flags decode OL CHRG without false low-battery");
    CHECK(strcmp(m.power_failure_status, "OK") == 0, "online status reports power failure OK");

    const uint8_t shutdown_delay[] = {0x12, 0x78, 0x56, 0x34, 0x12};
    CHECK(apc_hid_parse_report(0x12, shutdown_delay, sizeof(shutdown_delay), &m),
          "report 0x12 shutdown delay accepted");
    CHECK(m.delay_before_shutdown == 0x12345678u,
          "report 0x12 decodes DelayBeforeShutdown as 32-bit little-endian seconds");
    const uint8_t shutdown_sentinel[] = {0x12, 0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(!apc_hid_parse_report(0x12, shutdown_sentinel, sizeof(shutdown_sentinel), &m),
          "report 0x12 all-ones sentinel ignored");
    CHECK(m.delay_before_shutdown == 0x12345678u,
          "shutdown sentinel does not overwrite last valid value");

    const uint8_t reboot_delay[] = {0x13, 0x3C, 0x00, 0x00, 0x00};
    CHECK(apc_hid_parse_report(0x13, reboot_delay, sizeof(reboot_delay), &m),
          "report 0x13 reboot delay accepted");
    CHECK(m.delay_before_reboot == 60.0f,
          "report 0x13 decodes DelayBeforeReboot as seconds");
    const uint8_t reboot_sentinel[] = {0x13, 0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(!apc_hid_parse_report(0x13, reboot_sentinel, sizeof(reboot_sentinel), &m),
          "report 0x13 all-ones sentinel ignored");
    CHECK(m.delay_before_reboot == 60.0f,
          "reboot sentinel does not overwrite last valid value");

    const uint8_t legacy_input_voltage[] = {0x31, 0x78, 0x00};
    CHECK(!apc_hid_parse_report(0x31, legacy_input_voltage, sizeof(legacy_input_voltage), &m),
          "unadvertised legacy report ID rejected");
    CHECK(m.input_voltage == 0.0f, "legacy report cannot inject fake input voltage");

    printf("APC live-report decode tests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
