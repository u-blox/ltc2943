/* mbed Microcontroller Library
 * Copyright (c) 2017 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file ltc2943.cpp
 * This file defines the API to the Linear Technology LTC2943 battery gauge chip.
 */

/// Define this to print debug information.
//#define DEBUG_LTC2943

#include <mbed.h>
#include <battery_gauge_ltc2943.h>

#ifdef DEBUG_LTC2943
# include <stdio.h>
#endif

// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

/// How long to wait for one ADC read of temperature, voltage and current
// to be performed.
#define ADC_READ_WAIT_MS 100

/// The tolerance allowed for value conversions into the threshold registers.
#define LTC_2943_TOLERANCE 2

/// Check that a value is within tolerance.
#define TOLERANCE_CHECK(value, intendedValue, tolerance) ((value - intendedValue <=  tolerance) && (value - intendedValue >=  -tolerance))

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// ----------------------------------------------------------------
// GENERIC PRIVATE FUNCTIONS
// ----------------------------------------------------------------

/// Read two bytes from an address.
// Note: gpI2c should be locked before this is called.
bool BatteryGaugeLtc2943::getTwoBytes (uint8_t registerAddress, uint16_t *pBytes)
{
    bool success = false;
    char data[3];

    if (gpI2c != NULL) {
        // Send a command to read from registerAddress
        data[0] = registerAddress;
        data[1] = 0;
        data[2] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 2) == 0)) {
            success = true;
            if (pBytes) {
                *pBytes = (((uint16_t) data[1]) << 8) + data[2];
            }
        }
    }

    return success;
}

/// Set two bytes, starting from an address.
// Note: gpI2c should be locked before this is called.
bool BatteryGaugeLtc2943::setTwoBytes (uint8_t registerAddress, uint16_t bytes)
{
    bool success = false;
    char data[3];

    if (gpI2c != NULL) {
        // Send a command to write from registerAddress
        data[0] = registerAddress;
        data[1] = (char) (bytes >> 8);
        data[2] = (char) bytes;

        if (gpI2c->write(gAddress, &(data[0]), 3) == 0) {
            success = true;
        }
    }

    return success;
}

/// Ensure that the ADC of the chip has taken a reading recently.
// Note: gpI2c should be locked before this is called.
bool BatteryGaugeLtc2943::makeAdcReading (void)
{
    bool success = false;
    char storedControlRegister;
    char data[2];
    
    if (gReady && (gpI2c != NULL)){
        // Read the control register
        data[0] = 0x01;  // Address of the control register
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {
            storedControlRegister = data[1];
            // If the ADC mode, in bits 6 and 7, is zero, set
            // it to 01, which will take a manual reading of voltage,
            // current and temperature and then return them to zero.
            // Also make sure that the power-down bit is not set.
            if ((data[1] & 0xC0) == 0) {
                data[1] |= 0x40;
                data[1] &= ~0x01;
                if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
                    wait_ms(ADC_READ_WAIT_MS);
                    // Now set the shutdown bit to ensure lowest power state
                    data[1] = storedControlRegister | 0x01;
                    if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
                        success = true;
                    }
                }
            } else {
                // Readings are already being made, nothing to do here
                success = true;
            }
        }
    }
    
    return success;
}

/// Convert a 16 bit register reading into a temperature reading in C.
// Note: gpI2c should be locked before this is called.
int32_t BatteryGaugeLtc2943::registerToTemperatureC (uint16_t data)
{
    // From the data sheet the temperature (in Kelvin) is
    // T = 510 * data / 0xffff    
    return (((uint32_t) data) * 510 / 0xffff) - 273;
}

/// Convert a temperature in C to a register value.
// Note: no overflow checking is done here, such effects must be checked by the caller.
uint16_t BatteryGaugeLtc2943::temperatureCToRegister (int32_t temperatureC)
{
    int32_t registerValue;
    
    // Check against the laws of physics
    MBED_ASSERT (temperatureC >= -273);
    
    // Rearranging from the above 
    registerValue = (temperatureC + 273) * 0xffff / 510;
    
    if (registerValue > 0xffff) {
        registerValue = 0xffff;
    }
    if (registerValue < 0) {
        registerValue = 0;
    }
    
    return (uint16_t) registerValue;
}

/// Convert a 16 bit register reading into a voltage reading in mV.
int32_t BatteryGaugeLtc2943::registerToVoltageMV (uint16_t data)
{
    // From the data sheet the voltage (in mV) is
    // V = 23600 * data / 0xffff
    return (((int32_t) data) * 23600) / 0xffff;
}

/// Convert a voltage in mV to a register value.
// Note: no overflow checking is done here, such effects must be checked by the caller.
uint16_t BatteryGaugeLtc2943::voltageMVToRegister (int32_t voltageMV)
{
    int32_t registerValue;
    
    // Rearranging from the above 
    registerValue = (voltageMV * 0xffff) / 23600;
    
    if (registerValue > 0xffff) {
        registerValue = 0xffff;
    }
    if (registerValue < 0) {
        registerValue = 0;
    }
    
    return (uint16_t) registerValue;
}

/// Convert a 16 bit register reading into a current reading in mA.
int32_t BatteryGaugeLtc2943::registerToCurrentMA (uint16_t data, int32_t rSenseMOhm)
{
    // From the data sheet, max current (in Amps) corresponds to 0xffff
    // (which is 60 mV across RSense) while min (most negative)
    // current corresponds to 0 (which is -60 mV across RSense).
    return ((int32_t) (data - 0x7fff)) * 60 * 1000 / 0x7fff / rSenseMOhm;
}

/// Convert a current in mA to a register value.
// Note: no overflow checking is done here, such effects must be checked by the caller.
uint16_t BatteryGaugeLtc2943::currentMAToRegister (int32_t currentMA, int32_t rSenseMOhm)
{
    int32_t registerValue;
    int32_t value = currentMA * rSenseMOhm;
    
    // Rearranging from the above 
    registerValue = ((value) * 544 / 1000) + 0x7fff;
    
    if (registerValue > 0xffff) {
        registerValue = 0xffff;
    }
    if (registerValue < 0) {
        registerValue = 0;
    }
    
    return (uint16_t) registerValue;
}

/// Convert a 16 bit register reading into a charge reading in mAh.
int32_t BatteryGaugeLtc2943::registerToChargeMAH (uint16_t data, int32_t rSenseMOhm, int32_t prescaler)
{
    // From the data sheet, each bit of the charge registers has
    // the following value in mAh:
    // 0.34 * 50 * prescaler / RSense / 4096.
    return ((((int32_t) data) * 17 * prescaler) / rSenseMOhm) >> 12;
}

/// Convert a charge in mAh to a register value.
// Note: no overflow checking is done here, such effects must be checked by the caller.
uint16_t BatteryGaugeLtc2943::chargeMAHToRegister (int32_t chargeMAH, int32_t rSenseMOhm, int32_t prescaler)
{
    int32_t registerValue;
    
    // Rearranging from the above 
    registerValue = ((chargeMAH << 12) * rSenseMOhm) / prescaler / 17;
    
    if (registerValue > 0xffff) {
        registerValue = 0xffff;
    }
    if (registerValue < 0) {
        registerValue = 0;
    }
    
    return (uint16_t) registerValue;
}

//----------------------------------------------------------------
// PUBLIC FUNCTIONS: initialisation and monitoring
//----------------------------------------------------------------

/// Constructor.
BatteryGaugeLtc2943::BatteryGaugeLtc2943(void)
{
    gpI2c = NULL;
    gReady = false;
    gBatteryCapacityMAH = 0;
}

/// Destructor.
BatteryGaugeLtc2943::~BatteryGaugeLtc2943(void)
{
}

/// Initialise ourselves.
bool BatteryGaugeLtc2943::init (I2C * pI2c, int32_t rSenseMOhm, uint8_t address, int32_t prescaler, Alcc alcc)
{
    char data[2];

    gpI2c = pI2c;
    gAddress = address << 1;
    gRSenseMOhm = rSenseMOhm;
    gPrescaler = prescaler;
    
    MBED_ASSERT(alcc < MAX_NUM_ALCCS);

    if (gpI2c != NULL) {
        gpI2c->lock();
        // Set up the prescaler, ALCC and set everything to lowest power state for now
        data[0] = 0x01;  // Address of the control register
        
        // Bits 7:6 top two bits zero == ADC asleep
        // Bits 5:3 coded prescaler value
        // Bits 2:1 ALCC mode (10 for Alert Output, 01 for Charge Complete Input, 00 for Off
        // Bit  0   1 to power down analogue components
        
        data[1] = 0x01;
        data[1] |= alcc << 1;
        switch (prescaler) {
            case 1:
                // Coded prescaler value is zero, nothing to do
            break;
            case 4:
                data[1] |= 1 << 3;
            break;
            case 16:
                data[1] |= 2 << 3;
            break;
            case 64:
                data[1] |= 3 << 3;
            break;
            case 256:
                data[1] |= 4 << 3;
            break;
            case 1024:
                data[1] |= 5 << 3;
            break;
            case 4096:
                data[1] |= 6 << 3;
            break;
            default:
                gPrescaler = 0;
                MBED_ASSERT(false);
            break;
        }
        
        // Now write to the control register
        if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
            gReady = true;
        }
        
        gpI2c->unlock();
   }

#ifdef DEBUG_LTC2943
    if (gReady) {
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): handler initialised (control register set to 0x%02x).\n", gAddress >> 1, data[1]);
    } else {
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): device NOT found.\r\n", gAddress >> 1);
    }
#endif

    return gReady;
}

/// Switch battery charge monitoring on.
bool BatteryGaugeLtc2943::enableGauge (bool isSlow)
{
    bool success = false;
    char data[2];
    
    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        // Read the control register
        data[0] = 0x01;  // Address of the control register
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {

            // Set the ADC mode in bits 6 and 7 to 11 or, if
            // isSlow is true, to 10 (in which case a measurement
            // is only performed every ten seconds).
            data[1] |= 0xc0;
            if (isSlow) {
                data[1] &= ~0x40;
            }
            // Also make sure that the power-down bit is not set.
            data[1] &= ~0x01;
            
            // Write the new value to the control register
            if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
                success = true;
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Switch battery charge monitoring off.
bool BatteryGaugeLtc2943::disableGauge (void)
{
    bool success = false;
    char data[2];
    
    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        // Read the control register
        data[0] = 0x01;  // Address of the control register
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {

            // Set the ADC mode to 00 and the power down bit to 1
            data[1] &= ~0xc0;
            data[1] |= 0x01;
            
            // Write the new value to the control register
            if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
                success = true;
            }
        }
        gpI2c->unlock();
    }

    return success;
}

//----------------------------------------------------------------
// PUBLIC FUNCTIONS: getting readings
//----------------------------------------------------------------

/// Get the temperature of the chip.
bool BatteryGaugeLtc2943::getTemperature (int32_t *pTemperatureC)
{
    bool success = false;
    uint16_t data;

    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        if (makeAdcReading ()) {
            // Read from the temperature register address
            if (getTwoBytes (0x14, &data)) {
                success = true;

                if (pTemperatureC) {
                    *pTemperatureC = registerToTemperatureC (data);
                }

#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature registers report 0x%04x, so %d C.\n",
                       gAddress >> 1, data, registerToTemperatureC (data));
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}


/// Get the voltage of the battery.
bool BatteryGaugeLtc2943::getVoltage (int32_t *pVoltageMV)
{
    bool success = false;
    uint16_t data;

    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        if (makeAdcReading()) {
            // Read from the voltage register address
            if (getTwoBytes (0x08, &data)) {
                success = true;

                if (pVoltageMV) {
                    *pVoltageMV = registerToVoltageMV (data);
                }

#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage registers report 0x%04x, giving a voltage of %.3f V.\n",
                       gAddress >> 1, data, (float) registerToVoltageMV (data) / 1000);
#endif
            }
        }
        gpI2c->unlock();
    }
        
    return success;
}

/// Get the current flowing through RSense.
bool BatteryGaugeLtc2943::getCurrent (int32_t *pCurrentMA)
{
    bool success = false;
    uint16_t data;

    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        if (makeAdcReading()) {
            // Read from the current register address
            if (getTwoBytes (0x0e, &data)) {
                success = true;

                if (pCurrentMA) {
                    *pCurrentMA = registerToCurrentMA (data, gRSenseMOhm);
                }
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): current registers report 0x%04x, giving a current of %.3f A.\n",
                       gAddress >> 1, data, (float) registerToCurrentMA (data, gRSenseMOhm) / 1000);
#endif
            }
        }
        gpI2c->unlock();
    }
        
    return success;
}

/// Tell the LTC2943 chip that charging is complete.
bool BatteryGaugeLtc2943::setChargingComplete (int32_t capacityMAH)
{
    bool success = false;
    bool analoguePowerReady = true;
    char data[2];
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // First read the control register as we have to power
        // down the analogue parts when setting the charge registers
        data[0] = 0x01;  // Address of the control register
        data[1] = 0;
        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {
                
            // If the power down bit is not set, set it
            if ((data[1] & 0x01) != 0x01) {
                data[1] |= 0x01;
                if (gpI2c->write(gAddress, &(data[0]), 2) != 0) {
                    analoguePowerReady = false;
                }
            }
            
            if (analoguePowerReady) {
                // Remember the battery charge
                gBatteryCapacityMAH = capacityMAH;
                
                // Set the charge counter to max
                success = setTwoBytes(0x02, 0xffff);
    
                // If the ADC mode, in bits 6 and 7, is not zero then
                // we must switch on the analogue power again.
                if ((data[1] & 0xc0) != 0) {
                    data[1] &= ~0x01;
                    if (gpI2c->write(gAddress, &(data[0]), 2) != 0) {
                        success = false;
                    }
                }
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get the remaining battery charge.
bool BatteryGaugeLtc2943::getRemainingCharge (int32_t *pChargeMAH)
{
    bool success = false;
    int32_t chargeMAH;
    uint16_t data;

    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        // Read from the charge accumulator register address
        if (getTwoBytes (0x02, &data)) {
            success = true;
            
            // Full scale corresponds to the capacity of the battery when
            // fully charged, so the capacity used is 0xffff - data            
            chargeMAH = registerToChargeMAH (0xffff - data, gRSenseMOhm, gPrescaler);
            
            if (pChargeMAH) {
                *pChargeMAH = gBatteryCapacityMAH - chargeMAH;
                if (gBatteryCapacityMAH == 0) {
                    // If gBatteryCapacityMAH is 0, we can't actually calculate the remaining
                    // capacity but we can calculate the charge used using the fact that the default
                    // starting value is 0x7FFF.  So set success to false so that the user knows
                    // we can't supply what was requested but report the charge used number anyway
                    // in case it is useful
                    success = false;
                    *pChargeMAH = registerToChargeMAH (0x7FFF - data, gRSenseMOhm, gPrescaler);
                }
            }

#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge accumulator registers report 0x%04x, battery capacity is %d giving a charge remaining of %.3f AH.\n",
                   gAddress >> 1, data, gBatteryCapacityMAH, (float) (gBatteryCapacityMAH - chargeMAH) / 1000);
#endif
        }
        gpI2c->unlock();
    }
        
    return success;
}

/// Get the battery percentage remaining.
bool BatteryGaugeLtc2943::getRemainingPercentage (int32_t *pBatteryPercent)
{
    bool success;
    int32_t chargeMAH;

    success = getRemainingCharge (&chargeMAH);

    if (success) {
        if (pBatteryPercent) {
            *pBatteryPercent = 100 * gBatteryCapacityMAH / chargeMAH;
        }
        
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): remaining charge is %d mAh, battery capacity is %d mAh, so percentage remaining is %d%%.\n",
               gAddress >> 1, chargeMAH, gBatteryCapacityMAH, 100 * gBatteryCapacityMAH / chargeMAH);
#endif
    }
    
    return success;
}

/// Get the reason(s) for an alert.
char BatteryGaugeLtc2943::getAlertReason (void)
{
    char result = 0;
    char data[2];
    
    if (gReady && (gpI2c != NULL)){
        gpI2c->lock();
        data[0] = 0x00;  // Address of the control register
        data[1] = 0;
        // Read the status register
        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1))) {
            result = data[1];
        }
        gpI2c->unlock();
    }
    
    return result;
}
        
//----------------------------------------------------------------
// PUBLIC FUNCTIONS: setting/getting/checking/clearing thresholds
//----------------------------------------------------------------

/// Set temperature alert upper threshold.
bool BatteryGaugeLtc2943::setTemperatureHigh (int32_t temperatureC)
{
    bool success = false;
    uint16_t registerValue = temperatureCToRegister(temperatureC);
    char data[2];

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Note that the temperature threshold is 8 bit, not 16 bits
        // like all the other thresholds
        registerValue >>= 8;
        // Check for overflow in conversion to the register value
        printf ("registerValue %d, temperatureC %d, registerToTemperatureC(registerValue << 8) %d\n",
                registerValue, (int) temperatureC, (int) registerToTemperatureC(registerValue << 8));
        if (TOLERANCE_CHECK (registerToTemperatureC(registerValue << 8), temperatureC, LTC_2943_TOLERANCE)) {
            data[0] = 0x16; // Temperature threshold high address
            // Only write the value if it fits (and taking into
            // account the fact that 0xFF means "no upper threshold")
            if ((registerValue < 0xff)) {
                data[1] = (char) registerValue;
                if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
                    success = true;
#ifdef DEBUG_LTC2943
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature high threshold register set to 0x%02x, (%d C).\n",
                           gAddress >> 1, (char) registerValue, registerToTemperatureC(registerValue << 8));
#endif
                }
            } else {
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): trying to set temperature high threshold to %d C, value is too large (0x%04x < 0xff).\n",
                       gAddress >> 1, temperatureC, registerValue);
#endif
            }
        }
        gpI2c->unlock();
    }
    
    return success;
}

/// Get temperature alert upper threshold.
bool BatteryGaugeLtc2943::getTemperatureHigh (int32_t *pTemperatureC)
{
    bool success = false;
    char data[2];

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        data[0] = 0x16; // Temperature threshold high address
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {
            success = true;
            
            if (pTemperatureC) {
                // Note that the temperature threshold is 8 bit, not 16 bits
                // like all the other thresholds
                *pTemperatureC = registerToTemperatureC(((uint16_t) data[1]) << 8);
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature high threshold register reports 0x%02x (a temperature of %d C).\n",
                       gAddress >> 1, ((uint16_t) data[1]) << 8, registerToTemperatureC(((uint16_t) data[1]) << 8));
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the temperature alert upper threshold set.
bool BatteryGaugeLtc2943::isTemperatureHighSet (void)
{
    bool isSet = false;
    char data[2];

    // Note that the temperature threshold is 8 bit, not 16 bits
    // like all the other thresholds
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        data[0] = 0x16; // Temperature threshold high address
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {            
            if (data[1] < 0xff) {
                isSet = true;
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature high threshold register is set.\n", gAddress >> 1);
#endif
            }
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear temperature alert upper threshold.
bool BatteryGaugeLtc2943::clearTemperatureHigh (void)
{
    bool success = false;
    char data[2];

    // Note that the temperature threshold is 8 bit, not 16 bits
    // like all the other thresholds
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        data[0] = 0x16;    // Temperature threshold high address
        data[1] = 0xff;    // 0xFF means no upper threshold set
        if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
            success = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature high threshold register cleared.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Set temperature alert lower threshold.
bool BatteryGaugeLtc2943::setTemperatureLow (int32_t temperatureC)
{
    bool success = false;
    uint16_t registerValue = temperatureCToRegister (temperatureC);
    char data[2];

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Note that the temperature threshold is 8 bit, not 16 bits
        // like all the other thresholds
        registerValue >>= 8;
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToTemperatureC(registerValue << 8), temperatureC, LTC_2943_TOLERANCE)) {
            data[0] = 0x17; // Temperature threshold low address
            // Only write the value if it fits (and taking into
            // account the fact that 0 means "no lower threshold")
            if (registerValue > 0) {
                data[1] = (char) registerValue;
                if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
                    success = true;
#ifdef DEBUG_LTC2943
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature low threshold register set to 0x%02x, (%d C).\n",
                        gAddress >> 1, (char) registerValue, registerToTemperatureC(registerValue << 8));
#endif
                }
            } else {
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): trying to set temperature low threshold to %d C, value is too small (0x%04x > 0).\n",
                       gAddress >> 1, temperatureC, registerValue);
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// get temperature alert lower threshold.
bool BatteryGaugeLtc2943::getTemperatureLow (int32_t *pTemperatureC)
{
    bool success = false;
    char data[2];

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        data[0] = 0x17; // Temperature threshold low address
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {
            success = true;
            
            if (pTemperatureC) {
                // Note that the temperature threshold is 8 bit, not 16 bits
                // like all the other thresholds
                *pTemperatureC = registerToTemperatureC(((uint16_t) data[1]) << 8);
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature low threshold register reports 0x%02x (a temperature of %d C).\n",
                       gAddress >> 1, ((uint16_t) data[1]) << 8, registerToTemperatureC(((uint16_t) data[1]) << 8));
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the temperature alert lower threshold set.
bool BatteryGaugeLtc2943::isTemperatureLowSet (void)
{
    bool isSet = false;
    char data[2];

    // Note that the temperature threshold is 8 bit, not 16 bits
    // like all the other thresholds
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        data[0] = 0x17; // Temperature threshold low address
        data[1] = 0;

        if ((gpI2c->write(gAddress, &(data[0]), 1, true) == 0) &&
            (gpI2c->read(gAddress, &(data[1]), 1) == 0)) {
            
            if (data[1] > 0) {
                isSet = true;
#ifdef DEBUG_LTC2943
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature low threshold register is set.\n", gAddress >> 1);
#endif
            }
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear temperature alert lower threshold.
bool BatteryGaugeLtc2943::clearTemperatureLow (void)
{
    bool success = false;
    char data[2];

    // Note that the temperature threshold is 8 bit, not 16 bits
    // like all the other thresholds
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        data[0] = 0x17; // Temperature threshold low address
        data[1] = 0;    // 0 means no lower threshold set
        
        if (gpI2c->write(gAddress, &(data[0]), 2) == 0) {
            success = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): temperature low threshold register is cleared.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Set voltage alert upper threshold.
bool BatteryGaugeLtc2943::setVoltageHigh (int32_t voltageMV)
{
    bool success = false;
    uint16_t registerValue = voltageMVToRegister(voltageMV);

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToVoltageMV(registerValue), voltageMV, LTC_2943_TOLERANCE)) {
            // Only write the value if it is less than 0xffff (as
            // 0xffff means no threshold set)
            if (registerValue < 0xfff) {
                success = setTwoBytes (0x0a, registerValue);
#ifdef DEBUG_LTC2943
                if (success) {
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage high threshold register set to 0x%04x, (%d mV).\n",
                           gAddress >> 1, registerValue, registerToVoltageMV(registerValue));
                }
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get voltage alert upper threshold.
bool BatteryGaugeLtc2943::getVoltageHigh (int32_t *pVoltageMV)
{
    bool success = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = getTwoBytes (0x0a, &data);
        if (success && pVoltageMV) {
            *pVoltageMV = registerToVoltageMV (data);
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage high threshold register is 0x%04x, (%d mV).\n",
                   gAddress >> 1, data, registerToVoltageMV(data));
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the voltage alert upper threshold set.
bool BatteryGaugeLtc2943::isVoltageHighSet (void)
{
    bool isSet = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Default value is 0xffff, so if it is not that it is set
        if (getTwoBytes (0x0a, &data) && (data != 0xffff)) {
            isSet = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage high threshold register is set.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear voltage alert upper threshold.
bool BatteryGaugeLtc2943::clearVoltageHigh (void)
{
    bool success = false;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = setTwoBytes (0x0a, 0xffff);
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage high threshold register is cleared.\n", gAddress >> 1);
#endif
        gpI2c->unlock();
    }

    return success;
}

/// Set voltage alert lower threshold.
bool BatteryGaugeLtc2943::setVoltageLow (int32_t voltageMV)
{
    bool success = false;
    uint16_t registerValue = voltageMVToRegister (voltageMV);

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToVoltageMV(registerValue), voltageMV, LTC_2943_TOLERANCE)) {
            // Only write the value if it is greater than 0 (as
            // 0 means no threshold set)
            if (registerValue > 0) {
                success = setTwoBytes (0x0c, registerValue);
#ifdef DEBUG_LTC2943
            if (success) {
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage low threshold register set to 0x%04x, (%d mV).\n",
                       gAddress >> 1, registerValue, registerToVoltageMV(registerValue));
            }
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get voltage alert lower threshold.
bool BatteryGaugeLtc2943::getVoltageLow (int32_t *pVoltageMV)
{
    bool success = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = getTwoBytes (0x0c, &data);
        if (success && pVoltageMV) {
            *pVoltageMV = registerToVoltageMV (data);
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage low threshold register is 0x%04x, (%d mV).\n",
                   gAddress >> 1, data, registerToVoltageMV(data));
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the voltage alert lower threshold set.
bool BatteryGaugeLtc2943::isVoltageLowSet (void)
{
    bool isSet = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Default value is 0, so if it is not that it is set
        if (getTwoBytes (0x0c, &data) && (data != 0)) {
            isSet = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage low threshold register is set.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear voltage alert lower threshold.
bool BatteryGaugeLtc2943::clearVoltageLow(void)
{
    bool success = false;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = setTwoBytes (0x0c, 0);
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): voltage low threshold register is cleared.\n", gAddress >> 1);
#endif
        gpI2c->unlock();
    }

    return success;
}

/// Set current alert upper threshold.
bool BatteryGaugeLtc2943::setCurrentHigh(int32_t currentMA)
{
    bool success = false;
    uint16_t registerValue;

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        registerValue = currentMAToRegister (currentMA, gRSenseMOhm);
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToCurrentMA(registerValue, gRSenseMOhm), currentMA, LTC_2943_TOLERANCE)) {
            // Only write the value if it is less than 0xffff (as
            // 0xffff means no threshold set)
            if (registerValue < 0xffff) {
                success = setTwoBytes (0x10, registerValue);
#ifdef DEBUG_LTC2943
                if (success) {
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): current high threshold register set to 0x%04x (%d mA).\n",
                    gAddress >> 1, registerValue, registerToCurrentMA(registerValue, gRSenseMOhm));
                }
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get current alert upper threshold.
bool BatteryGaugeLtc2943::getCurrentHigh(int32_t *pCurrentMA)
{
    bool success = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = getTwoBytes (0x10, &data);
        if (success && pCurrentMA) {
            *pCurrentMA = registerToCurrentMA(data, gRSenseMOhm);
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): current high threshold register is 0x%04x (%d mA).\n",
            gAddress >> 1, data, registerToCurrentMA(data, gRSenseMOhm));
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the current alert upper threshold set.
bool BatteryGaugeLtc2943::isCurrentHighSet(void)
{
    bool isSet = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Default value is 0xffff, so if it is not that it is set
        if (getTwoBytes (0x10, &data) && (data != 0xffff)) {
            isSet = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): current high threshold register is set.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear current alert upper threshold.
bool BatteryGaugeLtc2943::clearCurrentHigh(void)
{
    bool success = false;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = setTwoBytes (0x10, 0xffff);
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): current high threshold register is cleared.\n", gAddress >> 1);
#endif
        gpI2c->unlock();
    }

    return success;
}

/// Set current alert lower threshold.
bool BatteryGaugeLtc2943::setCurrentLow(int32_t currentMA)
{
    bool success = false;
    uint16_t registerValue;

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        registerValue = currentMAToRegister (currentMA, gRSenseMOhm);
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToCurrentMA(registerValue, gRSenseMOhm), currentMA, LTC_2943_TOLERANCE)) {
            // Only write the value if it is greate than 0 (as
            // 0 means no threshold set)
            if (registerValue > 0) {
                success = setTwoBytes (0x12, registerValue);
#ifdef DEBUG_LTC2943
                if (success) {
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): current low threshold register set to 0x%04x (%d mA).\n",
                           gAddress >> 1, registerValue, registerToCurrentMA(registerValue, gRSenseMOhm));
                }
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get current alert lower threshold.
bool BatteryGaugeLtc2943::getCurrentLow(int32_t *pCurrentMA)
{
    bool success = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = getTwoBytes (0x12, &data);
        if (success && pCurrentMA) {
            *pCurrentMA = registerToCurrentMA(data, gRSenseMOhm);
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): current low threshold register is 0x%04x (%d mA).\n",
            gAddress >> 1, data, registerToCurrentMA(data, gRSenseMOhm));
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the current alert lower threshold set.
bool BatteryGaugeLtc2943::isCurrentLowSet(void)
{
    bool isSet = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Default value is 0, so if it is not that it is set
        if (getTwoBytes (0x12, &data) && (data != 0)) {
            isSet = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): current low threshold register is set.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear current alert lower threshold.
bool BatteryGaugeLtc2943::clearCurrentLow(void)
{
    bool success = false;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = setTwoBytes (0x12, 0);
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): current low threshold register is cleared.\n", gAddress >> 1);
#endif
        gpI2c->unlock();
    }

    return success;
}

/// Set charge alert upper threshold.
bool BatteryGaugeLtc2943::setChargeHigh(int32_t chargeMAH)
{
    bool success = false;
    uint16_t registerValue;

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        registerValue = chargeMAHToRegister (chargeMAH, gRSenseMOhm, gPrescaler);
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToChargeMAH(registerValue, gRSenseMOhm, gPrescaler), chargeMAH, LTC_2943_TOLERANCE)) {
            // Only write the value if it is less than 0xffff (as
            // 0xffff means no threshold set)
            if (registerValue < 0xffff) {
                success = setTwoBytes (0x04, registerValue);
#ifdef DEBUG_LTC2943
                if (success) {
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge high threshold register set to 0x%04x (%d mAh).\n",
                           gAddress >> 1, registerValue, registerToChargeMAH(registerValue, gRSenseMOhm, gPrescaler));
                }
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get charge alert upper threshold.
bool BatteryGaugeLtc2943::getChargeHigh(int32_t *pChargeMAH)
{
    bool success = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = getTwoBytes (0x04, &data);
        if (success && pChargeMAH) {
            *pChargeMAH = registerToChargeMAH(data, gRSenseMOhm, gPrescaler);
#ifdef DEBUG_LTC2943
            if (success) {
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge high threshold register is 0x%04x (%d mAh).\n",
                       gAddress >> 1, data, registerToChargeMAH(data, gRSenseMOhm, gPrescaler));
            }
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the charge alert upper threshold set.
bool BatteryGaugeLtc2943::isChargeHighSet(void)
{
    bool isSet = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Default value is 0xffff, so if it is not that it is set
        if (getTwoBytes (0x04, &data) && (data != 0xffff)) {
            isSet = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge high threshold register is set.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear charge alert upper threshold.
bool BatteryGaugeLtc2943::clearChargeHigh(void)
{
    bool success = false;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = setTwoBytes (0x04, 0xffff);
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge high threshold register is cleared.\n", gAddress >> 1);
#endif
        gpI2c->unlock();
    }

    return success;
}

/// Set charge alert lower threshold.
bool BatteryGaugeLtc2943::setChargeLow(int32_t chargeMAH)
{
    bool success = false;
    uint16_t registerValue;

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        registerValue = chargeMAHToRegister (chargeMAH, gRSenseMOhm, gPrescaler);
        // Check for overflow in conversion to the register value
        if (TOLERANCE_CHECK (registerToChargeMAH(registerValue, gRSenseMOhm, gPrescaler), chargeMAH, LTC_2943_TOLERANCE)) {
            // Only write the value if it is greater than 0 (as
            // 0 means no threshold set)
            if (registerValue > 0) {
                success = setTwoBytes (0x06, registerValue);
#ifdef DEBUG_LTC2943
                if (success) {
                    printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge low threshold register set to 0x%04x (%d mAh).\n",
                           gAddress >> 1, registerValue, registerToChargeMAH(registerValue, gRSenseMOhm, gPrescaler));
                }
#endif
            }
        }
        gpI2c->unlock();
    }

    return success;
}

/// Get charge alert lower threshold.
bool BatteryGaugeLtc2943::getChargeLow(int32_t *pChargeMAH)
{
    bool success = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = getTwoBytes (0x06, &data);
        if (success && pChargeMAH) {
            *pChargeMAH = registerToChargeMAH(data, gRSenseMOhm, gPrescaler);
#ifdef DEBUG_LTC2943
            if (success) {
                printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge low threshold register is 0x%04x (%d mAh).\n",
                       gAddress >> 1, data, registerToChargeMAH(data, gRSenseMOhm, gPrescaler));
            }
#endif
        }
        gpI2c->unlock();
    }

    return success;
}

/// Is the charge alert lower threshold set.
bool BatteryGaugeLtc2943::isChargeLowSet(void)
{
    bool isSet = false;
    uint16_t data;
    
    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        // Default value is 0, so if it is not that it is set
        if (getTwoBytes (0x06, &data) && (data != 0)) {
            isSet = true;
#ifdef DEBUG_LTC2943
            printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge low threshold register is set.\n", gAddress >> 1);
#endif
        }
        gpI2c->unlock();
    }

    return isSet;
}

/// Clear charge alert lower threshold.
bool BatteryGaugeLtc2943::clearChargeLow(void)
{
    bool success = false;

    if (gReady && (gpI2c != NULL)) {
        gpI2c->lock();
        success = setTwoBytes (0x06, 0);
#ifdef DEBUG_LTC2943
        printf("BatteryGaugeLtc2943 (I2C 0x%02x): charge low threshold register is cleared.\n", gAddress >> 1);
#endif
        gpI2c->unlock();
    }

    return success;
}

// End Of File
