#include <src/chopper/chopper.h>
#include <src/hal/Board.h>
#include <src/reference_signal/sine_wave.h>
#include <src/system_config.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ti/devices/msp432e4/driverlib/driverlib.h>
#include <ti/drivers/Timer.h>
#include <xdc/runtime/System.h>

void applyPortSetting(uint32_t ui32Port);
void setPinsBuffered(uint32_t ui32Port, uint8_t ui8Pins, uint8_t ui8Val);
void svm_timer_callback(Timer_Handle handle);
void stair_case_timer_callback(Timer_Handle handle);

volatile uint64_t state_counter = 0;

// A phase - L
// B phase - K Up
// C phase - K Low

uint8_t svm_phase_levels_a[] = {0x04, 0x00, 0x02};
uint8_t svm_phase_levels_b[] = {128, 0x00, 64};
uint8_t svm_phase_levels_c[] = {32, 0x00, 16};

Timer_Handle timer1;
FILE *fp;

void *mainThread(void *arg0) {
    start_chopper();

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL)))
        ;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC)))
        ;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    while (!(SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK)))
        ;

    GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE,
                          GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE,
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);

    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE,
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);

    Timer_init();

    Timer_Handle timer0;
    Timer_Params params;

    Timer_Params_init(&params);
    params.period = svm_frequency_hz;
    params.periodUnits = Timer_PERIOD_HZ;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = stair_case_timer_callback;
    params.timerCallback = svm_timer_callback;

    timer0 = Timer_open(Board_TIMER0, &params);

    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        System_abort("SVM timer did not start");
    }

    Timer_Params params1;
    Timer_Params_init(&params1);
    params1.timerMode = Timer_FREE_RUNNING;
    params.period = 2 ^ 32 - 1;
    params.periodUnits = Timer_PERIOD_COUNTS;

    timer1 = Timer_open(Board_TIMER1, &params1);

    if (Timer_start(timer1) == Timer_STATUS_ERROR) {
        System_abort("SVM timer did not start");
    }

    return 0;
}

// TODO(akremor): Sizing and naming
static uint32_t port_buffer[15] = {0};

void setPinsBuffered(uint32_t ui32Port, uint8_t ui8Pins, uint8_t ui8Val) {
    // Takes in pins that can be | to set the value to val
    // Some freaky memory shifting to resolve the base into a 0-indexed sequence
    // GPIO_PORTA_BASE                 ((uint32_t)0x40058000)

    uint8_t port_buffer_index = ui32Port;
    //(ui32Port >> 3) - 88;

    if (ui8Val) {
        port_buffer[port_buffer_index] |= ui8Pins;
    } else {
        port_buffer[port_buffer_index] &= ~(ui8Pins);
    }
}

void applyPortSetting(uint32_t ui32Port) {
    // uint8_t port_buffer_index = (ui32Port >> 3) - 88;

    if (ui32Port == 1) {
        GPIOPinWrite(GPIO_PORTL_BASE, 0xFF, port_buffer[0]);
    }
    if (ui32Port == 2) {
        GPIOPinWrite(GPIO_PORTK_BASE, 0xFF, port_buffer[1]);
    }
    if (ui32Port == 3) {
        GPIOPinWrite(GPIO_PORTC_BASE, 0xFF, port_buffer[2]);
    }
}

void stair_case_timer_callback(Timer_Handle myHandle) {
    // setPinsBuffered(GPIO_PORTL_BASE, 0xFF, false);
    // setPinsBuffered(GPIO_PORTL_BASE, states[state_counter % sizeof(states)],
    //                true);
    // applyPortSetting(GPIO_PORTL_BASE);

    // state_counter++;
}

void svm_timer_callback(Timer_Handle handle) {
    float32_t Vdc = 1;
    abc_quantity value = SineWave::getValueAbc(state_counter);

    gh_quantity hex_value;
    hex_value.g = 1 / (3 * Vdc) * (2 * value.a - value.b - value.c);
    hex_value.h = 1 / (3 * Vdc) * (-1 * value.a + 2 * value.b - value.c);

    gh_quantity Vul = {ceilf(hex_value.g), floorf(hex_value.h)};
    gh_quantity Vlu = {floorf(hex_value.g), ceilf(hex_value.h)};
    gh_quantity Vuu = {ceilf(hex_value.g), ceilf(hex_value.h)};
    gh_quantity Vll = {floorf(hex_value.g), floorf(hex_value.h)};

    gh_quantity nearest_1 = Vul;
    gh_quantity nearest_2 = Vlu;
    gh_quantity nearest_3;

    if (copysignf(1.0, hex_value.g + hex_value.h - Vul.g - Vul.h) == 1) {
        nearest_3 = Vuu;
    } else {
        nearest_3 = Vll;
    }

    // Now we need to find an available voltage state.
    // This is a very rudimentary implementation

    int32_t k = 0;
    bool constraints_satisfied = false;
    int32_t g = nearest_1.g;
    int32_t h = nearest_1.h;
    int32_t n = 3;  // TODO
    while (!constraints_satisfied) {
        if (k >= 0 && k - g >= 0 && k - g - h >= 0 && k <= n - 1 &&
            k - g <= n - 1 && k - g - h <= n - 1) {
            constraints_satisfied = true;
            break;
        }
        if (k >= n - 1) {
            break;
        }
        k++;
    }

    int32_t a_phase = k;
    int32_t b_phase = k - nearest_1.g;
    int32_t c_phase = k - nearest_1.g - nearest_1.h;

    GPIOPinWrite(GPIO_PORTL_BASE, 0xFF, svm_phase_levels_a[a_phase]);
    GPIOPinWrite(GPIO_PORTK_BASE, 0xFF,
                 svm_phase_levels_b[b_phase] | svm_phase_levels_c[c_phase]);

    state_counter++;
}
