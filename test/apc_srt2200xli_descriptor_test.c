/* Regression test for the APC Smart-UPS SRT2200XLI (VID:PID 051D:0003).
 * Its live HID fields are incorrectly marked Constant (0x23/0xA3). The
 * parser must retain explicitly named Power Device/Battery fields but still
 * discard anonymous constant padding. */
#include "hid_pdc.h"
#include "ups_map.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks, failures;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const uint8_t desc[] = {
    0x05,0x84, 0x09,0x04, 0xA1,0x01, /* UPS */
    0x09,0x24, 0xA1,0x02,             /* PowerSummary */
    0x75,0x08, 0x95,0x01, 0x15,0x00, 0x26,0xFF,0x00,
    /* Report 0x0C: remaining capacity, marked Constant by APC */
    0x85,0x0C, 0x09,0x66, 0x81,0x23, 0x09,0x66, 0xB1,0xA3,
    /* Report 0x0D: runtime, 32-bit seconds, marked Constant by APC */
    0x85,0x0D, 0x05,0x85, 0x09,0x68, 0x66,0x01,0x10, 0x55,0x00,
    0x75,0x20, 0x17,0xFF,0xFF,0xFF,0xFF, 0x25,0x00,
    0x81,0xA3, 0x09,0x68, 0xB1,0xA3,
    /* Report 0x0B: measured battery voltage, unit is volt, exponent 5. */
    0x85,0x0B, 0x05,0x84, 0x09,0x30, 0x67,0x21,0xD1,0xF0,0x00,
    0x55,0x05, 0x75,0x10, 0x15,0x00, 0x27,0xFF,0xFF,0x00,0x00,
    0xB1,0xA3,
    /* Report 0x0A: nominal battery voltage (7200 -> 72.00 V) */
    0x85,0x0A, 0x09,0x40, 0x67,0x21,0xD1,0xF0,0x00, 0x55,0x05,
    0xB1,0x23, 0x55,0x00,
    /* Report 0x07 status fields. Each APC marks both IN and FEATURE Constant. */
    0x85,0x07, 0x05,0x85, 0x75,0x01, 0x15,0x00, 0x25,0x01,
    0x09,0x44, 0x81,0xA3, 0x09,0x44, 0xB1,0xA3, /* charging */
    0x09,0x45, 0x81,0xA3, 0x09,0x45, 0xB1,0xA3, /* discharging */
    0x09,0xD0, 0x81,0xA3, 0x09,0xD0, 0xB1,0xA3, /* AC present */
    0x09,0xD1, 0x81,0xA3, 0x09,0xD1, 0xB1,0xA3,
    0x09,0x42, 0x81,0xA3, 0x09,0x42, 0xB1,0xA3, /* low battery */
    0x05,0x84, 0x09,0x68, 0x81,0xA3, 0x09,0x68, 0xB1,0xA3,
    0x09,0x69, 0x81,0xA3, 0x09,0x69, 0xB1,0xA3,
    0x05,0x85, 0x09,0x43, 0x81,0xA3, 0x09,0x43, 0xB1,0xA3,
    0x05,0x84, 0x09,0x73, 0x81,0xA3, 0x09,0x73, 0xB1,0xA3,
    0x05,0x85, 0x09,0x4B, 0x81,0xA3, 0x09,0x4B, 0xB1,0xA3,
    0x05,0x84, 0x09,0x65, 0x81,0xA3, 0x09,0x65, 0xB1,0xA3,
    0x05,0x85, 0x09,0xDB, 0x81,0xA3, 0x09,0xDB, 0xB1,0xA3,
    0xC0,0xC0,
    /* Vendor report group from the actual SRT descriptor. These long reports
     * expand to 63 fields each and used to fill the 224-field parser table
     * before the later small Feature reports were reached. */
    0x06,0x86,0xFF,
    0x85,0x89, 0x09,0xFD, 0x15,0x00, 0x26,0xFF,0x00,
    0x75,0x08, 0x95,0x3F, 0x81,0x02,             /* 63-byte Input */
    0x85,0x90, 0x09,0xFC, 0x95,0x3F, 0x91,0x02, /* 63-byte Output */
    0x85,0x96, 0x09,0xF1, 0x95,0x3F, 0xB1,0x22, /* 63-byte Feature */
    0x85,0x8D, 0x09,0xF7, 0x95,0x02, 0xB1,0x22,
    0x85,0x8E, 0x09,0xF6, 0xB1,0x23,
    0x85,0x92, 0x09,0xF4, 0xB1,0x23,
    0x85,0x93, 0x09,0xF3, 0xB1,0x22,
    0x85,0x94, 0x09,0xF2, 0xB1,0x23
};

int main(void)
{
    hid_pdc_map_t map;
    ups_metrics_t m;
    memset(&m, 0, sizeof(m));
    CHECK(hid_pdc_parse(desc, sizeof(desc), &map), "APC-style descriptor parses");
    CHECK(map.has_power_page, "Power Device page detected");
    CHECK(!map.truncated, "full vendor report table retained (count=%d)", map.count);
    CHECK(map.count > 224, "fixture exercises more than the old 224-field limit (count=%d)", map.count);

    int n = ups_map_apply(&map, 0x0C, HID_PDC_INPUT,
                          (const uint8_t[]){0x4A,0x00}, 2, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_charge - 74.0f) < 0.01f,
          "report 0x0C charge = %.2f, expected 74", (double)m.battery_charge);
    n = ups_map_apply(&map, 0x0C, HID_PDC_INPUT,
                      (const uint8_t[]){0x5D,0x00}, 2, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_charge - 93.0f) < 0.01f,
          "prior live report 0x0C charge = %.2f, expected 93", (double)m.battery_charge);
    n = ups_map_apply(&map, 0x0C, HID_PDC_INPUT,
                      (const uint8_t[]){0x5E,0x00}, 2, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_charge - 94.0f) < 0.01f,
          "latest live report 0x0C charge = %.2f, expected 94", (double)m.battery_charge);

    n = ups_map_apply(&map, 0x0D, HID_PDC_INPUT,
                      (const uint8_t[]){0xF6,0x7C,0x00,0x00}, 4, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_runtime - 31990.0f) < 0.01f,
          "report 0x0D runtime = %.2f, expected 31990 seconds", (double)m.battery_runtime);
    n = ups_map_apply(&map, 0x0D, HID_PDC_INPUT,
                      (const uint8_t[]){0xFF,0xFF,0xFF,0xFF}, 4, &m, NULL, NULL);
    CHECK(n == 0 && fabsf(m.battery_runtime - 31990.0f) < 0.01f,
          "all-ones runtime sentinel ignored without overwriting last value");
    n = ups_map_apply(&map, 0x0D, HID_PDC_INPUT,
                      (const uint8_t[]){0xFF,0x9E,0x00,0x00}, 4, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_runtime - 40703.0f) < 0.01f,
          "latest live report 0x0D runtime = %.2f, expected 40703 seconds", (double)m.battery_runtime);

    n = ups_map_apply(&map, 0x0B, HID_PDC_FEATURE,
                      (const uint8_t[]){0x27,0x20}, 2, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_voltage - 82.31f) < 0.01f,
          "report 0x0B voltage = %.2f, expected 82.31 V", (double)m.battery_voltage);
    n = ups_map_apply(&map, 0x0B, HID_PDC_FEATURE,
                      (const uint8_t[]){0xFF,0xFF}, 2, &m, NULL, NULL);
    CHECK(n == 0 && fabsf(m.battery_voltage - 82.31f) < 0.01f,
          "all-ones voltage sentinel ignored without overwriting last value");

    n = ups_map_apply(&map, 0x0A, HID_PDC_FEATURE,
                      (const uint8_t[]){0x20,0x1C}, 2, &m, NULL, NULL);
    CHECK(n >= 1 && fabsf(m.battery_nominal_voltage - 72.0f) < 0.01f,
          "report 0x0A nominal voltage = %.2f, expected 72 V", (double)m.battery_nominal_voltage);

    n = ups_map_apply(&map, 0x07, HID_PDC_INPUT,
                      (const uint8_t[]){0x0D,0x00}, 2, &m, NULL, NULL);
    CHECK(n >= 2 && m.status.charging && m.status.online && !m.status.discharging && !m.status.low_battery,
          "report 0x07 status decode: online=%d charging=%d discharging=%d low=%d",
          m.status.online, m.status.charging, m.status.discharging, m.status.low_battery);

    uint8_t ids[32];
    int id_count = ups_map_feature_report_ids(&map, ids, 32);
    bool found_0a=false, found_0b=false, found_0c=false, found_0d=false, found_07=false;
    bool found_96=false, found_8d=false, found_8e=false, found_92=false, found_93=false, found_94=false;
    for (int i=0; i<id_count; i++) {
        if (ids[i] == 0x0A) found_0a = true;
        if (ids[i] == 0x0B) found_0b = true;
        if (ids[i] == 0x0C) found_0c = true;
        if (ids[i] == 0x0D) found_0d = true;
        if (ids[i] == 0x07) found_07 = true;
        if (ids[i] == 0x96) found_96 = true;
        if (ids[i] == 0x8D) found_8d = true;
        if (ids[i] == 0x8E) found_8e = true;
        if (ids[i] == 0x92) found_92 = true;
        if (ids[i] == 0x93) found_93 = true;
        if (ids[i] == 0x94) found_94 = true;
    }
    CHECK(found_0a && found_0b && found_0c && found_0d && found_07,
          "derived feature polling includes standard SRT IDs (count %d)", id_count);
    CHECK(found_96 && found_8d && found_8e && found_92 && found_93 && found_94,
          "later vendor Feature IDs 96/8D/8E/92/93/94 survive parsing (count %d)", id_count);

    printf("APC SRT2200XLI descriptor regressions: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
