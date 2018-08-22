#include <source/hal/adc.h>
#include <stdint.h>
#include <ti/devices/msp432e4/driverlib/driverlib.h>
#include "arm_math.h"

constexpr uint8_t n_channels = 2;

void init_adc() {
    /* Enable the clock to GPIO Port E and wait for it to be ready */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE))) {
    }

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD))) {
    }

    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_2);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);

    /* Enable the clock to ADC-0 and wait for it to be ready */
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))) {
    }

    // Deals with errata ADC#14
    SysCtlPeripheralReset(SYSCTL_PERIPH_ADC0);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))) {
    }

    ADCReferenceSet(ADC0_BASE, ADC_CTL_VREF_EXT_3V);

    /* Configure Sequencer 2 to sample the differential analog channels. The
     * end of conversion and interrupt generation is set for differential
     * pair 1 */

    ADCSequenceStepConfigure(ADC0_BASE, 2, 0, ADC_CTL_CH0 | ADC_CTL_D);
    ADCSequenceStepConfigure(
        ADC0_BASE, 2, 1, ADC_CTL_CH1 | ADC_CTL_D | ADC_CTL_IE | ADC_CTL_END);
    // ADCSequenceStepConfigure(ADC0_BASE, 2, 2, ADC_CTL_CH4);
    // ADCSequenceStepConfigure(ADC0_BASE, 2, 3,
    //                         ADC_CTL_CH5 | ADC_CTL_IE | ADC_CTL_END);

    /* Enable sample sequence 2 with a processor signal trigger.  Sequencer 2
     * will do a single sample when the processor sends a signal to start the
     * conversion */
    ADCSequenceConfigure(ADC0_BASE, 2, ADC_TRIGGER_PROCESSOR, 2);

    /* Since sample sequence 2 is now configured, it must be enabled. */
    ADCSequenceEnable(ADC0_BASE, 2);

    /* Clear the interrupt status flag.  This is done to make sure the
     * interrupt flag is cleared before we sample. */
    ADCIntClear(ADC0_BASE, 2);
}

float32_t convertAdjustedDifferential(int32_t raw_sample) {
    float refVoltage = 3.3f;

    if (raw_sample == 0x800) {
        return 0;
    } else {
        return refVoltage * (((float)raw_sample - 0x800) / 0x800);
    }
}

float32_t convertAdjustedSingle(int32_t raw_sample) {
    float refVoltage = 3.3f;
    return refVoltage * (((float)raw_sample) / 0x1000);
}

void read_adc(float32_t* reading) {
    uint32_t getADCValue[n_channels];
    float32_t internal_reading[n_channels];
    /* Sample the channels forever.  Display the value on the console. */
    /* Trigger the ADC conversion. */
    ADCProcessorTrigger(ADC0_BASE, 2);

    /* Wait for conversion to be completed. */
    while (!ADCIntStatus(ADC0_BASE, 2, false)) {
    }

    /* Clear the ADC interrupt flag. */
    ADCIntClear(ADC0_BASE, 2);

    /* Read ADC Value. */
    ADCSequenceDataGet(ADC0_BASE, 2, getADCValue);
    for (int i = 0; i < 2; i++) {
        internal_reading[i] = convertAdjustedDifferential(getADCValue[i]);
    }

    // for (int i = 2; i < 4; i++) {
    //     internal_reading[i] = convertAdjustedSingle(getADCValue[i]);
    // }

    for (int i = 0; i < n_channels; i++) {
        reading[i] = internal_reading[i];
    }
}
