#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "battery_gauge_ltc2943.h"

using namespace utest::v1;

// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

// Pick some sensible minimum and maximum numbers
#define MAX_TEMPERATURE_READING_C  80
#define MIN_TEMPERATURE_READING_C -20
#define MIN_VOLTAGE_READING_MV     0
#define MAX_VOLTAGE_READING_MV     12000 // Bigger than a 3 cell LiPo
#define MAX_CURRENT_READING_MA     2000
#define MIN_CURRENT_READING_MA    -2000

#ifndef RSENSE_MOHM
// RSense (in mOhm) on UTM board
#define RSENSE_MOHM 68
#endif

#ifndef PRESCALER
// Prescaler (may need to supply this if you supply RSENSE_MOHM)
#define PRESCALER 4096
#endif

#ifndef CURRENT_VALID_POSITIVE_MA
// A valid positive current threshold value (may need to supply this if you supply RSENSE_MOHM)
#define CURRENT_VALID_POSITIVE_MA 10
#endif

#ifndef CURRENT_VALID_NEGATIVE_MA
// A valid negative current threshold value (may need to supply this if you supply RSENSE_MOHM)
#define CURRENT_VALID_NEGATIVE_MA -10
#endif

#ifndef CURRENT_OOR_POSITIVE_MA
// An out of range positive current threshold value (may need to supply this if you supply RSENSE_MOHM)
#define CURRENT_OOR_POSITIVE_MA 1000
#endif

#ifndef CURRENT_OOR_NEGATIVE_MA
// An out of range negative current threshold value (may need to supply this if you supply RSENSE_MOHM)
#define CURRENT_OOR_NEGATIVE_MA -1000
#endif

#ifndef CHARGE_VALID_MAH
// A valid charge threshold value (may need to supply this if you supply RSENSE_MOHM or PRESCALER)
#define CHARGE_VALID_MAH 1000
#endif

#ifndef CHARGE_OOR_POSITIVE_MAH
// An out of range positive charge threshold value (may need to supply this if you supply RSENSE_MOHM or PRESCALER)
#define CHARGE_OOR_POSITIVE_MAH 10000
#endif

#ifndef CHARGE_OOR_NEGATIVE_MAH
// An out of range negative charge threshold value (may need to supply this if you supply RSENSE_MOHM or PRESCALER)
// Note: the conversion equations for the chip allow negative values to be set and we don't stand in the way
// of that, this is specifically an out of range negative value
#define CHARGE_OOR_NEGATIVE_MAH -1000
#endif

#ifndef BATTERY_CAPACITY_MAH
#define BATTERY_CAPACITY_MAH 2300
#endif

#ifndef PIN_I2C_SDA
// Default for UTM board
#define PIN_I2C_SDA P0_27
#endif

#ifndef PIN_I2C_SDC
// Default for UTM board
#define PIN_I2C_SDC P0_28
#endif

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

#ifdef TARGET_UBLOX_C027
// This required only for UTM board
static DigitalOut gI2CPullUpBar(P1_1, 0);
#endif
// I2C interface
I2C * gpI2C = new I2C(PIN_I2C_SDA, PIN_I2C_SDC);

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test that the LTC2943 battery gauge can be initialised
void test_init() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    
    TEST_ASSERT_FALSE(pBatteryGauge->init(NULL, RSENSE_MOHM));
    
    // Normal case
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    
    // Different prescaler values
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 1));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 4));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 16));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 64));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 256));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 1024));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 4096));

    // Different ALCC values    
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, PRESCALER, BatteryGaugeLtc2943::ALCC_OFF));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 1024, BatteryGaugeLtc2943::ALCC_CHARGE_COMPLETE_INPUT));
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 1, BatteryGaugeLtc2943::ALCC_ALERT_OUTPUT));
}

// Test that battery capacity monitoring can be performed
void test_monitor() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    
    // Call should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->enableGauge());
    
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    // Normal case
    TEST_ASSERT(pBatteryGauge->enableGauge());    
    // TODO do something to assess whether it's actually working
    TEST_ASSERT(pBatteryGauge->disableGauge());
    
    // Normal case, slow mode
    TEST_ASSERT(pBatteryGauge->enableGauge(true));    
    // TODO do something to assess whether it's actually working slowly
    TEST_ASSERT(pBatteryGauge->disableGauge());
}

// Test that a temperature reading can be performed
void test_temperature() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t temperatureC;
    
    // Call should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->getTemperature(&temperatureC));
    
    // Normal case
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    TEST_ASSERT(pBatteryGauge->getTemperature(&temperatureC));
    printf ("Temperature %d C.\n", temperatureC);
    // Range check
    TEST_ASSERT((temperatureC >= MIN_TEMPERATURE_READING_C) && (temperatureC <= MAX_TEMPERATURE_READING_C));
    
    // The parameter is allowed to be NULL
    TEST_ASSERT(pBatteryGauge->getTemperature(NULL));
}

// Test that a voltage reading can be performed
void test_voltage() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t voltageMV;
    
    // Call should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->getVoltage(&voltageMV));
    
    // Normal case
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    TEST_ASSERT(pBatteryGauge->getVoltage(&voltageMV));
    printf ("Voltage %.3f V.\n", ((float) voltageMV) / 1000);
    // Range check
    TEST_ASSERT((voltageMV >= MIN_VOLTAGE_READING_MV) && (voltageMV <= MAX_VOLTAGE_READING_MV));
    
    // The parameter is allowed to be NULL
    TEST_ASSERT(pBatteryGauge->getVoltage(NULL));
}

// Test that a current reading can be performed
void test_current() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t currentMA;
    
    // Call should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->getVoltage(&currentMA));
    
    // Normal case
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    TEST_ASSERT(pBatteryGauge->getCurrent(&currentMA));
    printf ("Current %.3f A.\n", ((float) currentMA) / 1000);
    // Range check
    TEST_ASSERT((currentMA >= MIN_CURRENT_READING_MA) && (currentMA <= MAX_CURRENT_READING_MA));
    
    // The parameter is allowed to be NULL
    TEST_ASSERT(pBatteryGauge->getCurrent(NULL));
}

// Test that we can set charging complete and read back the capacity
// TODO: find a way to test non-100% numbers
void test_charging() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t remainingChargeMAH;
    int32_t remainingPercentage;
    
    // Call should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setChargingComplete(BATTERY_CAPACITY_MAH));
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    
    // If setChargingComplete() has not been called then a check on the charge
    // level should fail but the remaining charge value should still be
    // filled in, just with the charge used instead
    remainingChargeMAH = 0x7FFFFFFF;
    TEST_ASSERT_FALSE(pBatteryGauge->getRemainingCharge(&remainingChargeMAH));
    TEST_ASSERT(remainingChargeMAH != 0x7FFFFFFF);

    // Normal case
    TEST_ASSERT(pBatteryGauge->setChargingComplete(BATTERY_CAPACITY_MAH));
    // Read the remaining capacity value back
    TEST_ASSERT(pBatteryGauge->getRemainingCharge(&remainingChargeMAH));
    // Check that it is the same as what we said
    TEST_ASSERT(remainingChargeMAH == BATTERY_CAPACITY_MAH);
    // Check that we are now at 100%
    TEST_ASSERT(pBatteryGauge->getRemainingPercentage(&remainingPercentage));
    TEST_ASSERT(remainingPercentage == 100);

    // Repeat with some different values
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM / 2));
    TEST_ASSERT(pBatteryGauge->setChargingComplete(BATTERY_CAPACITY_MAH * 2));
    // Read the remaining capacity value back    
    TEST_ASSERT(pBatteryGauge->getRemainingCharge(&remainingChargeMAH));
    // Check that it is the same as what we said
    TEST_ASSERT(remainingChargeMAH == BATTERY_CAPACITY_MAH * 2);
    // Check that we are still at 100%
    TEST_ASSERT(pBatteryGauge->getRemainingPercentage(&remainingPercentage));
    TEST_ASSERT(remainingPercentage == 100);

    // Repeat with non-default prescaler
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM, BATTERY_GAUGE_LTC2943_ADDRESS, 1024));
    TEST_ASSERT(pBatteryGauge->setChargingComplete(BATTERY_CAPACITY_MAH));
    // Read the remaining capacity value back    
    TEST_ASSERT(pBatteryGauge->getRemainingCharge(&remainingChargeMAH));
    // Check that it is the same as what we said
    TEST_ASSERT(remainingChargeMAH == BATTERY_CAPACITY_MAH);
    // Check that we are still at 100%
    TEST_ASSERT(pBatteryGauge->getRemainingPercentage(&remainingPercentage));
    TEST_ASSERT(remainingPercentage == 100);
}

// Test that the alert reason can be retrieved
// TODO: find a way to test actual useful return values
void test_alert() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    
    // Call should return none if the battery gauge has not been initialised
    TEST_ASSERT(pBatteryGauge->getAlertReason() == BatteryGaugeLtc2943::ALERT_NONE);
    
    // Call also returns none if the battery gauge has been initialised and nothing has happened
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));
    TEST_ASSERT(pBatteryGauge->getAlertReason() == BatteryGaugeLtc2943::ALERT_NONE);
}

// Test that we can set, get, check and delete a high temperature threshold
void test_temperature_high() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t temperatureHighC = 60;
    int32_t temperatureC1 = 0;
    int32_t temperatureC2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setTemperatureHigh(temperatureHighC));
    TEST_ASSERT_FALSE(pBatteryGauge->getTemperatureHigh(&temperatureC1));
    TEST_ASSERT_FALSE(pBatteryGauge->isTemperatureHighSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearTemperatureHigh());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearTemperatureHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isTemperatureHighSet());
    
    // Set a high temperature limit
    TEST_ASSERT(pBatteryGauge->setTemperatureHigh(temperatureHighC));
    TEST_ASSERT(pBatteryGauge->getTemperatureHigh(&temperatureC1));
    printf ("Temperature threshold set was %d C, temperature threshold read back was %d C.\n", temperatureHighC, temperatureC1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, temperatureHighC, temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureHighSet());

    // Try to set an out of range temperature limit
    temperatureHighC = 240;
    TEST_ASSERT_FALSE(pBatteryGauge->setTemperatureHigh(temperatureHighC));
    TEST_ASSERT(pBatteryGauge->getTemperatureHigh(&temperatureC2));
    printf ("Temperature threshold set was (silly) %d C, temperature threshold read back was %d C.\n", temperatureHighC, temperatureC2);
    // Temperature threshold should be unchanged
    TEST_ASSERT(temperatureC2 == temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureHighSet());

    // Set a zero temperature limit
    temperatureHighC = 0;
    TEST_ASSERT(pBatteryGauge->setTemperatureHigh(temperatureHighC));
    TEST_ASSERT(pBatteryGauge->getTemperatureHigh(&temperatureC1));
    printf ("Temperature threshold set was %d C, temperature threshold read back was %d C.\n", temperatureHighC, temperatureC1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, temperatureHighC, temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureHighSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearTemperatureHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isTemperatureHighSet());
}

// Test that we can set, get, check and delete a low temperature threshold
void test_temperature_low() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t temperatureLowC = 10;
    int32_t temperatureC1 = 0;
    int32_t temperatureC2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setTemperatureLow(temperatureLowC));
    TEST_ASSERT_FALSE(pBatteryGauge->getTemperatureLow(&temperatureC1));
    TEST_ASSERT_FALSE(pBatteryGauge->isTemperatureLowSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearTemperatureLow());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearTemperatureLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isTemperatureLowSet());
    
    // Set a low temperature limit
    TEST_ASSERT(pBatteryGauge->setTemperatureLow(temperatureLowC));
    TEST_ASSERT(pBatteryGauge->getTemperatureLow(&temperatureC1));
    printf ("Temperature threshold set was %d C, temperature threshold read back was %d C.\n", temperatureLowC, temperatureC1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, temperatureLowC, temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureLowSet());

    // Set a zero temperature limit
    temperatureLowC = 0;
    TEST_ASSERT(pBatteryGauge->setTemperatureLow(temperatureLowC));
    TEST_ASSERT(pBatteryGauge->getTemperatureLow(&temperatureC1));
    printf ("Temperature threshold set was %d C, temperature threshold read back was %d C.\n", temperatureLowC, temperatureC1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, temperatureLowC, temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureLowSet());

    // Set a negative temperature limit
    temperatureLowC = -50;
    TEST_ASSERT(pBatteryGauge->setTemperatureLow(temperatureLowC));
    TEST_ASSERT(pBatteryGauge->getTemperatureLow(&temperatureC1));
    printf ("Temperature threshold set was %d C, temperature threshold read back was %d C.\n", temperatureLowC, temperatureC1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, temperatureLowC, temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureLowSet());

    // Try to set an out of range temperature limit
    temperatureLowC = -273;
    TEST_ASSERT_FALSE(pBatteryGauge->setTemperatureLow(temperatureLowC));
    TEST_ASSERT(pBatteryGauge->getTemperatureLow(&temperatureC2));
    printf ("Temperature threshold set was (silly) %d C, temperature threshold read back was %d C.\n", temperatureLowC, temperatureC2);
    // Temperature threshold should be unchanged
    TEST_ASSERT(temperatureC2 == temperatureC1);
    TEST_ASSERT(pBatteryGauge->isTemperatureLowSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearTemperatureLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isTemperatureLowSet());
}

// Test that we can set, get, check and delete a high voltage threshold
void test_voltage_high() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t voltageHighMV = 1000;
    int32_t voltageMV1 = 0;
    int32_t voltageMV2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setVoltageHigh(voltageHighMV));
    TEST_ASSERT_FALSE(pBatteryGauge->getVoltageHigh(&voltageMV1));
    TEST_ASSERT_FALSE(pBatteryGauge->isVoltageHighSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearVoltageHigh());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearVoltageHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isVoltageHighSet());
    
    // Set a high voltage limit
    TEST_ASSERT(pBatteryGauge->setVoltageHigh(voltageHighMV));
    TEST_ASSERT(pBatteryGauge->getVoltageHigh(&voltageMV1));
    printf ("Voltage threshold set was %d mV, voltage threshold read back was %d mV.\n", voltageHighMV, voltageMV1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, voltageHighMV, voltageMV1);
    TEST_ASSERT(pBatteryGauge->isVoltageHighSet());

    // Try to set an out of range voltage limit
    voltageHighMV = 10000;
    TEST_ASSERT_FALSE(pBatteryGauge->setVoltageHigh(voltageHighMV));
    TEST_ASSERT(pBatteryGauge->getVoltageHigh(&voltageMV2));
    printf ("Voltage threshold set was (silly) %d mV, voltage threshold read back was %d mV.\n", voltageHighMV, voltageMV2);
    // Voltage threshold should be unchanged
    TEST_ASSERT(voltageMV2 == voltageMV1);
    TEST_ASSERT(pBatteryGauge->isVoltageHighSet());

    // Set a zero voltage limit
    voltageHighMV = 0;
    TEST_ASSERT(pBatteryGauge->setVoltageHigh(voltageHighMV));
    TEST_ASSERT(pBatteryGauge->getVoltageHigh(&voltageMV1));
    printf ("Voltage threshold set was %d mV, voltage threshold read back was %d mV.\n", voltageHighMV, voltageMV1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, voltageHighMV, voltageMV1);
    TEST_ASSERT(pBatteryGauge->isVoltageHighSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearVoltageHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isVoltageHighSet());
}

// Test that we can set, get, check and delete a low voltage threshold
void test_voltage_low() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t voltageLowMV = 1000;
    int32_t voltageMV1 = 0;
    int32_t voltageMV2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setVoltageLow(voltageLowMV));
    TEST_ASSERT_FALSE(pBatteryGauge->getVoltageLow(&voltageMV1));
    TEST_ASSERT_FALSE(pBatteryGauge->isVoltageLowSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearVoltageLow());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearVoltageLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isVoltageLowSet());
    
    // Set a low voltage limit
    TEST_ASSERT(pBatteryGauge->setVoltageLow(voltageLowMV));
    TEST_ASSERT(pBatteryGauge->getVoltageLow(&voltageMV1));
    printf ("Voltage threshold set was %d mV, voltage threshold read back was %d mV.\n", voltageLowMV, voltageMV1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, voltageLowMV, voltageMV1);
    TEST_ASSERT(pBatteryGauge->isVoltageLowSet());

    // Try to set an out of range voltage limit
    voltageLowMV = 0;
    TEST_ASSERT_FALSE(pBatteryGauge->setVoltageLow(voltageLowMV));
    TEST_ASSERT(pBatteryGauge->getVoltageLow(&voltageMV2));
    printf ("Voltage threshold set was (silly) %d mV, voltage threshold read back was %d mV.\n", voltageLowMV, voltageMV2);
    // Voltage threshold should be unchanged
    TEST_ASSERT(voltageMV2 == voltageMV1);
    TEST_ASSERT(pBatteryGauge->isVoltageLowSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearVoltageLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isVoltageLowSet());
}

// Test that we can set, get, check and delete a high current threshold
void test_current_high() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t currentHighMA = CURRENT_VALID_POSITIVE_MA;
    int32_t currentMA1 = 0;
    int32_t currentMA2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setCurrentHigh(currentHighMA));
    TEST_ASSERT_FALSE(pBatteryGauge->getCurrentHigh(&currentMA1));
    TEST_ASSERT_FALSE(pBatteryGauge->isCurrentHighSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearCurrentHigh());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearCurrentHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isCurrentHighSet());
    
    // Set a high current limit
    TEST_ASSERT(pBatteryGauge->setCurrentHigh(currentHighMA));
    TEST_ASSERT(pBatteryGauge->getCurrentHigh(&currentMA1));
    printf ("Current threshold set was %d mA, current threshold read back was %d mA.\n", currentHighMA, currentMA1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, currentHighMA, currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentHighSet());

    // Set a zero current limit
    currentHighMA = 0;
    TEST_ASSERT(pBatteryGauge->setCurrentHigh(currentHighMA));
    TEST_ASSERT(pBatteryGauge->getCurrentHigh(&currentMA1));
    printf ("Current threshold set was %d mA, current threshold read back was %d mA.\n", currentHighMA, currentMA1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, currentHighMA, currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentHighSet());

    // Set a negative current limit
    currentHighMA = CURRENT_VALID_NEGATIVE_MA;
    TEST_ASSERT(pBatteryGauge->setCurrentHigh(currentHighMA));
    TEST_ASSERT(pBatteryGauge->getCurrentHigh(&currentMA1));
    printf ("Current threshold set was %d mA, current threshold read back was %d mA.\n", currentHighMA, currentMA1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, currentHighMA, currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentHighSet());

    // Try to set an out of range current limit
    currentHighMA = CURRENT_OOR_POSITIVE_MA;
    TEST_ASSERT_FALSE(pBatteryGauge->setCurrentHigh(currentHighMA));
    TEST_ASSERT(pBatteryGauge->getCurrentHigh(&currentMA2));
    printf ("Current threshold set was (silly) %d mA, current threshold read back was %d mA.\n", currentHighMA, currentMA2);
    // Current threshold should be unchanged
    TEST_ASSERT(currentMA2 == currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentHighSet());

    // Try to set an out of range, but negative, current limit
    currentHighMA = CURRENT_OOR_NEGATIVE_MA;
    TEST_ASSERT_FALSE(pBatteryGauge->setCurrentHigh(currentHighMA));
    TEST_ASSERT(pBatteryGauge->getCurrentHigh(&currentMA2));
    printf ("Current threshold set was (silly) %d mA, current threshold read back was %d mA.\n", currentHighMA, currentMA2);
    // Current threshold should be unchanged
    TEST_ASSERT(currentMA2 == currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentHighSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearCurrentHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isCurrentHighSet());
}

// Test that we can set, get, check and delete a low current threshold
void test_current_low() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t currentLowMA = CURRENT_VALID_POSITIVE_MA;
    int32_t currentMA1 = 0;
    int32_t currentMA2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setCurrentLow(currentLowMA));
    TEST_ASSERT_FALSE(pBatteryGauge->getCurrentLow(&currentMA1));
    TEST_ASSERT_FALSE(pBatteryGauge->isCurrentLowSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearCurrentLow());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearCurrentLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isCurrentLowSet());
    
    // Set a low current limit
    TEST_ASSERT(pBatteryGauge->setCurrentLow(currentLowMA));
    TEST_ASSERT(pBatteryGauge->getCurrentLow(&currentMA1));
    printf ("Current threshold set was %d mA, current threshold read back was %d mA.\n", currentLowMA, currentMA1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, currentLowMA, currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentLowSet());

    // Set a zero current limit
    currentLowMA = 0;
    TEST_ASSERT(pBatteryGauge->setCurrentLow(currentLowMA));
    TEST_ASSERT(pBatteryGauge->getCurrentLow(&currentMA1));
    printf ("Current threshold set was %d mA, current threshold read back was %d mA.\n", currentLowMA, currentMA1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, currentLowMA, currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentLowSet());

    // Set a negative current limit
    currentLowMA = CURRENT_VALID_NEGATIVE_MA;
    TEST_ASSERT(pBatteryGauge->setCurrentLow(currentLowMA));
    TEST_ASSERT(pBatteryGauge->getCurrentLow(&currentMA1));
    printf ("Current threshold set was %d mA, current threshold read back was %d mA.\n", currentLowMA, currentMA1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, currentLowMA, currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentLowSet());

    // Try to set an out of range current limit
    currentLowMA = CURRENT_OOR_NEGATIVE_MA;
    TEST_ASSERT_FALSE(pBatteryGauge->setCurrentLow(currentLowMA));
    TEST_ASSERT(pBatteryGauge->getCurrentLow(&currentMA2));
    printf ("Current threshold set was (silly) %d mA, current threshold read back was %d mA.\n", currentLowMA, currentMA2);
    // Current threshold should be unchanged
    TEST_ASSERT(currentMA2 == currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentLowSet());

    // Try to set an out of range, but positive, current limit
    currentLowMA = CURRENT_OOR_POSITIVE_MA;
    TEST_ASSERT_FALSE(pBatteryGauge->setCurrentLow(currentLowMA));
    TEST_ASSERT(pBatteryGauge->getCurrentLow(&currentMA2));
    printf ("Current threshold set was (silly) %d mA, current threshold read back was %d mA.\n", currentLowMA, currentMA2);
    // Current threshold should be unchanged
    TEST_ASSERT(currentMA2 == currentMA1);
    TEST_ASSERT(pBatteryGauge->isCurrentLowSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearCurrentLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isCurrentLowSet());
}

// Test that we can set, get, check and delete a high charge threshold
void test_charge_high() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t chargeHighMAH = CHARGE_VALID_MAH;
    int32_t chargeMAH1 = 0;
    int32_t chargeMAH2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeHigh(chargeHighMAH));
    TEST_ASSERT_FALSE(pBatteryGauge->getChargeHigh(&chargeMAH1));
    TEST_ASSERT_FALSE(pBatteryGauge->isChargeHighSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearChargeHigh());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearChargeHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isChargeHighSet());
    
    // Set a high charge limit
    TEST_ASSERT(pBatteryGauge->setChargeHigh(chargeHighMAH));
    TEST_ASSERT(pBatteryGauge->getChargeHigh(&chargeMAH1));
    printf ("Charge threshold set was %d mAh, charge threshold read back was %d mAh.\n", chargeHighMAH, chargeMAH1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, chargeHighMAH, chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeHighSet());

    // Try to set an out of range positive charge limit
    chargeHighMAH = CHARGE_OOR_POSITIVE_MAH;
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeHigh(chargeHighMAH));
    TEST_ASSERT(pBatteryGauge->getChargeHigh(&chargeMAH2));
    printf ("Charge threshold set was (silly) %d mAh, charge threshold read back was %d mAh.\n", chargeHighMAH, chargeMAH2);
    // Charge threshold should be unchanged
    TEST_ASSERT(chargeMAH2 == chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeHighSet());

    // Try to set an out of range negative, charge limit
    chargeHighMAH = CHARGE_OOR_NEGATIVE_MAH;
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeHigh(chargeHighMAH));
    TEST_ASSERT(pBatteryGauge->getChargeHigh(&chargeMAH2));
    printf ("Charge threshold set was (silly) %d mAh, charge threshold read back was %d mAh.\n", chargeHighMAH, chargeMAH2);
    // Charge threshold should be unchanged
    TEST_ASSERT(chargeMAH2 == chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeHighSet());

    // Set a zero charge limit
    chargeHighMAH = 0;
    TEST_ASSERT(pBatteryGauge->setChargeHigh(chargeHighMAH));
    TEST_ASSERT(pBatteryGauge->getChargeHigh(&chargeMAH1));
    printf ("Charge threshold set was %d mAh, charge threshold read back was %d mAh.\n", chargeHighMAH, chargeMAH1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, chargeHighMAH, chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeHighSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearChargeHigh());
    TEST_ASSERT_FALSE(pBatteryGauge->isChargeHighSet());
}

// Test that we can set, get, check and delete a low charge threshold
void test_charge_low() {
    BatteryGaugeLtc2943 * pBatteryGauge = new BatteryGaugeLtc2943();
    int32_t chargeLowMAH = CHARGE_VALID_MAH;
    int32_t chargeMAH1 = 0;
    int32_t chargeMAH2 = 0;
    
    // All calls should fail if the battery gauge has not been initialised
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeLow(chargeLowMAH));
    TEST_ASSERT_FALSE(pBatteryGauge->getChargeLow(&chargeMAH1));
    TEST_ASSERT_FALSE(pBatteryGauge->isChargeLowSet());
    TEST_ASSERT_FALSE(pBatteryGauge->clearChargeLow());
    
    // Initialise the battery gauge
    TEST_ASSERT(pBatteryGauge->init(gpI2C, RSENSE_MOHM));

    // Check that the threshold can be cleared at the outset
    TEST_ASSERT(pBatteryGauge->clearChargeLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isChargeLowSet());
    
    // Set a low charge limit
    TEST_ASSERT(pBatteryGauge->setChargeLow(chargeLowMAH));
    TEST_ASSERT(pBatteryGauge->getChargeLow(&chargeMAH1));
    printf ("Charge threshold set was %d mAh, charge threshold read back was %d mAh.\n", chargeLowMAH, chargeMAH1);
    // Allow some variance as there will be conversion loss
    TEST_ASSERT_INT32_WITHIN(2, chargeLowMAH, chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeLowSet());

    // Try to set a zero charge limit
    chargeLowMAH = 0;
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeLow(chargeLowMAH));
    TEST_ASSERT(pBatteryGauge->getChargeLow(&chargeMAH2));
    printf ("Charge threshold set was (silly) %d mAh, charge threshold read back was %d mAh.\n", chargeLowMAH, chargeMAH2);
    // Charge threshold should be unchanged
    TEST_ASSERT(chargeMAH2 == chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeLowSet());

    // Try to set an out of range positive charge limit
    chargeLowMAH = CHARGE_OOR_POSITIVE_MAH;
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeLow(chargeLowMAH));
    TEST_ASSERT(pBatteryGauge->getChargeLow(&chargeMAH2));
    printf ("Charge threshold set was (silly) %d mAh, charge threshold read back was %d mAh.\n", chargeLowMAH, chargeMAH2);
    // Charge threshold should be unchanged
    TEST_ASSERT(chargeMAH2 == chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeLowSet());

    // Try to set an out of range negative, charge limit
    chargeLowMAH = CHARGE_OOR_NEGATIVE_MAH;
    TEST_ASSERT_FALSE(pBatteryGauge->setChargeLow(chargeLowMAH));
    TEST_ASSERT(pBatteryGauge->getChargeLow(&chargeMAH2));
    printf ("Charge threshold set was (silly) %d mAh, charge threshold read back was %d mAh.\n", chargeLowMAH, chargeMAH2);
    // Charge threshold should be unchanged
    TEST_ASSERT(chargeMAH2 == chargeMAH1);
    TEST_ASSERT(pBatteryGauge->isChargeLowSet());

    // Clear the limit
    TEST_ASSERT(pBatteryGauge->clearChargeLow());
    TEST_ASSERT_FALSE(pBatteryGauge->isChargeLowSet());
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea, timeout is long enough to run these tests with
    // DEBUG_LTC2943 defined
    GREENTEA_SETUP(20, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Initialisation", test_init),
    Case("Monitor", test_monitor),
    Case("Temperature read", test_temperature),
    Case("Voltage read", test_voltage),
    Case("Current read", test_current),
    Case("Charging", test_charging),
    Case("Alert", test_alert),
    Case("High temperature threshold", test_temperature_high),
    Case("Low temperature threshold", test_temperature_low),
    Case("High voltage threshold", test_voltage_high),
    Case("Low voltage threshold", test_voltage_low),
    Case("High current threshold", test_current_high),
    Case("Low current threshold", test_current_low),
    Case("High charge threshold", test_charge_high),
    Case("Low charge threshold", test_charge_low)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

// Entry point into the tests
int main() {    
    bool success = false;
    
    if (gpI2C != NULL) {        
        success = !Harness::run(specification);
    } else {
        printf ("Unable to instantiate I2C interface.\n");
    }
    
    return success;
}

// End Of File
