#include <source/hal/hil.h>
#include <source/system_config.h>
#include <source/system_state.h>
#include <ti/drivers/UART.h>

UART_Handle uart;
extern SystemState state;

void uartcallback(UART_Handle handle, void *buf, size_t count) {}

void receive_state_from_simulator() {
    int8_t read_buffer[20];
    while (1) {
        int8_t header[1] = {NULL};

        bool has_sync1 = false;

        while (!has_sync1) {
            if (UART_read(uart, header, 1)) {
                if (header[0] == 'A') {
                    has_sync1 = true;
                }
            }
        }

        bool has_sync2 = false;

        while (!has_sync2) {
            if (UART_read(uart, header, 1)) {
                if (header[0] == 'a') {
                    has_sync2 = true;
                }
            }
        }

        // Sync'd
        UART_read(uart, read_buffer, 9);

        abc_quantity load_voltage = {(float32_t)read_buffer[0],
                                     (float32_t)read_buffer[1],
                                     (float32_t)read_buffer[2]};
        abc_quantity load_line_current = {(float32_t)(read_buffer[3] / 64.0),
                                          (float32_t)(read_buffer[4] / 64.0),
                                          (float32_t)(read_buffer[5] / 64.0)};
        abc_quantity load_ll_voltage = {(float32_t)read_buffer[6],
                                        (float32_t)read_buffer[7],
                                        (float32_t)read_buffer[8]};

        state.load_voltage.set_abc(load_voltage);
        state.load_line_current.set_abc(load_line_current);
        state.load_ll_voltage.set_abc(load_ll_voltage);
    }
}
void init_hil() {
    UART_init();

    UART_Params uart_params;
    UART_Params_init(&uart_params);
    uart_params.readEcho = UART_ECHO_OFF;
    uart_params.readDataMode = UART_DATA_BINARY;
    uart_params.writeDataMode = UART_DATA_BINARY;
    uart_params.baudRate = 921600;
    uart_params.readMode = UART_MODE_BLOCKING;
    uart_params.writeMode = UART_MODE_CALLBACK;
    uart_params.writeCallback = uartcallback;

    uart = UART_open(0, &uart_params);
}

void send_state_to_simulator(SystemState state) {
    float scaling_factor = 1;
    int8_t ref_val_a = state.reference.get_abc().a * scaling_factor;
    int8_t ref_val_b = state.reference.get_abc().b * scaling_factor;
    int8_t ref_val_c = state.reference.get_abc().c * scaling_factor;

    int8_t a_9_cell = ((svm_phase_levels_a[state.a_phase] & A_POS9) ? 1 : 0) +
                      ((svm_phase_levels_a[state.a_phase] & A_NEG9) ? -1 : 0);
    int8_t a_3_cell = ((svm_phase_levels_a[state.a_phase] & A_POS3) ? 1 : 0) +
                      ((svm_phase_levels_a[state.a_phase] & A_NEG3) ? -1 : 0);
    int8_t a_1_cell = ((svm_phase_levels_a[state.a_phase] & A_POS1) ? 1 : 0) +
                      ((svm_phase_levels_a[state.a_phase] & A_NEG1) ? -1 : 0);

    int8_t b_9_cell = ((svm_phase_levels_b[state.b_phase] & B_POS9) ? 1 : 0) +
                      ((svm_phase_levels_b[state.b_phase] & B_NEG9) ? -1 : 0);
    int8_t b_3_cell = ((svm_phase_levels_b[state.b_phase] & B_POS3) ? 1 : 0) +
                      ((svm_phase_levels_b[state.b_phase] & B_NEG3) ? -1 : 0);
    int8_t b_1_cell = ((svm_phase_levels_b[state.b_phase] & B_POS1) ? 1 : 0) +
                      ((svm_phase_levels_b[state.b_phase] & B_NEG1) ? -1 : 0);

    int8_t c_9_cell = ((svm_phase_levels_c[state.c_phase] & C_POS9) ? 1 : 0) +
                      ((svm_phase_levels_c[state.c_phase] & C_NEG9) ? -1 : 0);
    int8_t c_3_cell = ((svm_phase_levels_c[state.c_phase] & C_POS3) ? 1 : 0) +
                      ((svm_phase_levels_c[state.c_phase] & C_NEG3) ? -1 : 0);
    int8_t c_1_cell = ((svm_phase_levels_c[state.c_phase] & C_POS1) ? 1 : 0) +
                      ((svm_phase_levels_c[state.c_phase] & C_NEG1) ? -1 : 0);

    int8_t buffer[20] = {
        65,
        97,
        (int8_t)((state.a_phase - sizeof(svm_phase_levels_a) / 2) -
                 (state.b_phase - sizeof(svm_phase_levels_b) / 2)),
        (int8_t)((state.b_phase - sizeof(svm_phase_levels_b) / 2) -
                 (state.c_phase - sizeof(svm_phase_levels_c) / 2)),
        (int8_t)((state.c_phase - sizeof(svm_phase_levels_c) / 2) -
                 (state.a_phase - sizeof(svm_phase_levels_a) / 2)),
        ref_val_a,
        ref_val_b,
        ref_val_c,
        a_9_cell,
        a_3_cell,
        a_1_cell,
        b_9_cell,
        b_3_cell,
        b_1_cell,
        c_9_cell,
        c_3_cell,
        c_1_cell};
    UART_write(uart, buffer, sizeof(buffer));
}
