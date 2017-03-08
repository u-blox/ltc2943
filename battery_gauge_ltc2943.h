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

#ifndef BATTERY_GAUGE_LTC2943_H
#define BATTERY_GAUGE_LTC2943_H

/**
 * @file battery_gauge_ltc2943.h
 * This file defines the API to the Linear Technology LTC2943 battery gauge chip.
 */

// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

/// Device I2C address.
#define BATTERY_GAUGE_LTC2943_ADDRESS 0x64

/// Default prescaler value.
#define BATTERY_GAUGE_LTC2943_PRESCALER_DEFAULT 4096

// ----------------------------------------------------------------
// CLASSES
// ----------------------------------------------------------------

/// LTC2943 battery gauge driver.
class BatteryGaugeLtc2943 {
public:

    /// ALCC usage.
    typedef enum {
        ALCC_OFF = 0,
        ALCC_CHARGE_COMPLETE_INPUT = 1,
        ALCC_ALERT_OUTPUT = 2,
        MAX_NUM_ALCCS = 3
    } Alcc;

    /// The alerts that ALCC can give when in "AL" mode.
    // The values form a bit-map.
    typedef enum {
        ALERT_NONE = 0,
        ALERT_UNDERVOLTAGE_LOCKOUT = 1 << 0,
        ALERT_VOLTAGE = 1 << 1,
        ALERT_CHARGE_LOW = 1 << 2,
        ALERT_CHARGE_HIGH = 1 << 3,
        ALERT_TEMPERATURE = 1 << 4,
        ALERT_CHARGE_OVER_UNDER_FLOW = 1 << 5,
        ALERT_CURRENT = 1 << 6
    } Alert;

    /// Constructor.
    BatteryGaugeLtc2943(void);
    /// Destructor.
    ~BatteryGaugeLtc2943(void);

    /// Initialise the LTC2943 chip.
    // After initialisation the chip will be put into its lowest power state.
    // \param pI2c a pointer to the I2C instance to use.
    // \param rSenseMOhm the value of the sense resistor being used, in milli Ohms.
    //\ param address 7-bit I2C address of the LiPo gauge chip.
    //\ param prescaler the prescaler value to use, valid values being 1, 4, 16, 64, 256, 1024 and 4096.
    //        Refer to page 11 of the LTC2943 data sheet for information on choosing the prescaler value
    //        for a given battery capacity and peak current.
    // \param alcc how to configure the Alert/Charge Complete pin, see page 10 of the LTC2943 data sheet.
    // \return true if successful, otherwise false.
    bool init (I2C * pI2c, int32_t rSenseMOhm, uint8_t address = BATTERY_GAUGE_LTC2943_ADDRESS,
               int32_t prescaler = BATTERY_GAUGE_LTC2943_PRESCALER_DEFAULT, Alcc alcc = ALCC_OFF);

    /// Determine whether a battery has been detected or not.
    // \return true if it is detected otherwise false.
    bool isBatteryDetected (void);
    
    /// Switch on the battery gauge.  The chip will consume more
    // power when battery gauging is enabled.
    // \param isSlow set this to true to save power if the battery current is not fluctuating very much.
    // \return true if successful, otherwise false.
    bool enableGauge (bool isSlow = false);

    /// Switch off the battery gauge.
    // \return true if successful, otherwise false.
    bool disableGauge (void);

    /// Read the temperature of the LTC2943 chip.  If gauging is
    // active the temperature will be that of the last reading; if
    // gauging is not active then a reading will be taken
    // immediately, which takes around 300 ms.
    // \param pTemperatureC place to put the temperature reading.
    // \return true if successful, otherwise false.
    bool getTemperature (int32_t *pTemperatureC);

    /// Read the voltage of the battery.  If gauging is
    // active the voltage will be that of the last reading; if
    // gauging is not active then a reading will be taken
    // immediately, which takes around 300 ms.
    // \param pVoltageMV place to put the voltage reading.
    // \return true if successful, otherwise false.
    bool getVoltage (int32_t *pVoltageMV);

    /// Read the current flowing through rSense.  The value will be
    // positive when the battery is charging and negative when it is
    // discharging.  If gauging is active the current will be
    // that of the last reading; if gauging is not active then a
    // reading will be taken immediately, which takes around 300 ms.
    // \param pCurrentMA place to put the current reading.
    // \return true if successful, otherwise false.
    bool getCurrent (int32_t *pCurrentMA);

    /// Tell the LTC2943 chip that charging is complete,
    // after which it can work out the remaining charge.
    // \param capacityMAH the capacity of the charged battery.
    // \return true if successful, otherwise false.
    bool setChargingComplete (int32_t capacityMAH);
        
    /// Read the remaining available battery energy.
    // NOTE: this function can only return a value if the battery
    // has been fully charged and the setChargingComplete()
    // function called since the LTC2943 chip was last physically reset.
    // If this is not the case the pChargeMAh value will be filled in
    // with the charge used instead (in case this is useful) and the
    // return value will be set to false.
    // \param pChargeMAH place to put the charge reading.
    // \return true if successful, otherwise false.
    bool getRemainingCharge (int32_t *pChargeMAH);

    /// Read the state of charge of the battery as a percentage.
    // NOTE: this function can only return an accurate value
    // if the battery has been fully charged and the setChargingComplete()
    // function called since the LTC2943 chip was last physically reset.
    // \param pBatteryPercent place to put the reading.
    // \return true if successful, otherwise false.
    bool getRemainingPercentage (int32_t *pBatteryPercent);
    
    /// Get the reason(s) for an alert.
    // Note: as with all the other API functions here, this should
    // not be called from an interrupt function as the comms with the
    // LTC2943 chip over I2C will take too long.
    // \return a bit-map containing the Alert reasons that
    //         can be tested against the values of the Alert enum.
    char getAlertReason (void);
        
    /// Set temperature alert upper threshold.
    // \param temperatureC the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setTemperatureHigh (int32_t temperatureC);

    /// Get temperature alert upper threshold.
    // \param pTemperatureC place to put the temperature threshold.
    // \return true if successful, otherwise false.
    bool getTemperatureHigh (int32_t *pTemperatureC);

    /// Determine whether the temperature high threshold is set or not.
    // \return true if it is set otherwise false.
    bool isTemperatureHighSet (void);
    
    /// Clear temperature alert upper threshold.
    // \return true if successful, otherwise false.
    bool clearTemperatureHigh (void);

    /// Set temperature alert lower threshold.
    // \param temperatureC the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setTemperatureLow (int32_t temperatureC);
    
    /// Get temperature alert lower threshold.
    // \param pTemperatureC place to put the temperature threshold.
    // \return true if successful, otherwise false.
    bool getTemperatureLow (int32_t *pTemperatureC);

    /// Determine whether the temperature low threshold is set or not.
    // \return true if it is set otherwise false.
    bool isTemperatureLowSet (void);
    
    /// Clear temperature alert lower threshold.
    // \return true if successful, otherwise false.
    bool clearTemperatureLow (void);

    /// Set voltage alert upper threshold.
    // \param voltageMV the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setVoltageHigh (int32_t voltageMV);
    
    /// Get voltage alert upper threshold.
    // \param pVoltageMV place to put the voltage threshold.
    // \return true if successful, otherwise false.
    bool getVoltageHigh (int32_t *pVoltageMV);

    /// Determine whether the voltage high threshold is set or not.
    // \return true if it is set otherwise false.
    bool isVoltageHighSet (void);
    
    /// Clear voltage alert upper threshold.
    // \return true if successful, otherwise false.
    bool clearVoltageHigh (void);

    /// Set voltage alert lower threshold.
    // \param voltageMV the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setVoltageLow (int32_t voltageMV);
    
    /// Get voltage alert lower threshold.
    // \param pVoltageMV place to put the voltage threshold.
    // \return true if successful, otherwise false.
    bool getVoltageLow (int32_t *pVoltageMV);

    /// Determine whether the voltage low threshold is set or not.
    // \return true if it is set otherwise false.
    bool isVoltageLowSet (void);
    
    /// Clear voltage alert lower threshold.
    // \return true if successful, otherwise false.
    bool clearVoltageLow (void);

    /// Set current alert upper threshold.
    // \param currentMA the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setCurrentHigh (int32_t currentMA);

    /// Get current alert upper threshold.
    // \param pCurrentMA place to put the current threshold.
    // \return true if successful, otherwise false.
    bool getCurrentHigh (int32_t *pCurrentMA);
    
    /// Determine whether the current high threshold is set or not.
    // \return true if it is set otherwise false.
    bool isCurrentHighSet (void);
    
    /// Clear current alert upper threshold.
    // \return true if successful, otherwise false.
    bool clearCurrentHigh (void);

    /// Set current alert lower threshold.
    // \param currentMA the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setCurrentLow (int32_t currentMA);
    
    /// Get current alert lower threshold.
    // \param pCurrentMA place to put the current threshold.
    // \return true if successful, otherwise false.
    bool getCurrentLow (int32_t *pCurrentMA);
    
    /// Determine whether the current low threshold is set or not.
    // \return true if it is set otherwise false.
    bool isCurrentLowSet (void);
    
    /// Clear current alert lower threshold.
    // \return true if successful, otherwise false.
    bool clearCurrentLow (void);

    /// Set capacity alert upper threshold.
    // \param chargeMAH the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setChargeHigh (int32_t chargeMAH);

    /// Get capacity alert upper threshold.
    // \param pChargeMAh place to put the capacity threshold.
    // \return true if successful, otherwise false.
    bool getChargeHigh (int32_t *pChargeMAH);

    /// Determine whether the charge high threshold is set or not.
    // \return true if it is set otherwise false.
    bool isChargeHighSet (void);
    
    /// Clear capacity alert upper threshold.
    // \return true if successful, otherwise false.
    bool clearChargeHigh (void);

    /// Set capacity alert lower threshold.
    // \param chargeMAH the value to set.
    // \return true if successful, otherwise false.  Trying
    //         to set an out of range value will return false.
    bool setChargeLow (int32_t chargeMAH);
    
    /// Get capacity alert lower threshold.
    // \param pChargeMAH place to put the capacity threshold.
    // \return true if successful, otherwise false.
    bool getChargeLow (int32_t *pChargeMAH);

    /// Determine whether the charge low threshold is set or not.
    // \return true if it is set otherwise false.
    bool isChargeLowSet (void);
    
    /// Clear capacity alert lower threshold.
    // \return true if successful, otherwise false.
    bool clearChargeLow (void);

protected:
    /// Pointer to the I2C interface.
    I2C * gpI2c;
    /// Value of RSense.
    int32_t gRSenseMOhm;
    /// Value of Prescaler.
    int32_t gPrescaler;
    /// The address of the device.
    uint8_t gAddress;
    /// Flag to indicate device is ready.
    bool gReady;
    /// The capacity of the battery.
    int32_t gBatteryCapacityMAH;

    /// Read two bytes starting at a given address.
    // \param registerAddress the register address to start reading from.
    // \param pBytes place to put the two bytes.
    // \return true if successful, otherwise false.
    bool getTwoBytes (uint8_t registerAddress, uint16_t *pBytes);
    
    /// Set two bytes, starting at a given address.
    // \param registerAddress the register address to start reading from.
    // \param bytes the two bytes.
    // \return true if successful, otherwise false.
    bool setTwoBytes (uint8_t registerAddress, uint16_t bytes);
    
    /// Make sure that an ADC reading has been performed recently.
    // \return true if successful, otherwise false.
    bool makeAdcReading (void);
    
    /// Convert a 16 bit register reading into a temperature reading in C
    // \param data the register reading.
    // \return the temperature reading in C.
    int32_t registerToTemperatureC (uint16_t data);

    /// Convert a temperature in C to a register value.
    // \param temperatureC the temperature in C.
    // \return the register value.
    uint16_t temperatureCToRegister (int32_t temperatureC);

    /// Convert a 16 bit register reading into a voltage reading in mV
    // \param data the register reading.
    // \return the voltage reading in mV.
    int32_t registerToVoltageMV (uint16_t data);
    
    /// Convert a voltage in mV to a register value.
    // \param voltageMV the voltage in mV.
    // \return  the register value.
    uint16_t voltageMVToRegister (int32_t voltageMV);
    
    /// Convert a 16 bit register reading into a current reading in mA
    // \param data the register reading.
    // \param rSenseMOhm the rSense value.
    // \return the current reading in mA.
    int32_t registerToCurrentMA (uint16_t data, int32_t rSenseMOhm);
    
    /// Convert a current in mA to a register value.
    // \param currentMA the current in mA.
    // \param rSenseMOhm the rSense value.
    // \return  the register value.
    uint16_t currentMAToRegister (int32_t currentMA, int32_t rSenseMOhm);

    /// Convert a 16 bit register reading into a charge reading in mAh
    // \param data the register reading.
    // \param rSenseMOhm the rSense value.
    // \param prescaler the prescaler value.
    // \return the charge reading in mAh.
    int32_t registerToChargeMAH (uint16_t data, int32_t rSenseMOhm, int32_t prescaler);

    /// Convert a charge in mAh to a register value.
    // \param currentMAH the current in mAh.
    // \param rSenseMOhm the rSense value.
    // \param prescaler the prescaler value.
    // \return the register value.
    uint16_t chargeMAHToRegister (int32_t currentMAH, int32_t rSenseMOhm, int32_t prescaler);
};

#endif

// End Of File
