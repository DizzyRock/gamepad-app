#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>

#define I2C_DEVICE "/dev/i2c-5"
#define GAMEPAD_ADDR 0x50

#define SEESAW_STATUS_BASE 0x00
#define SEESAW_STATUS_SWRST 0x7F
#define SEESAW_ADC_BASE 0x09
#define SEESAW_ADC_CHANNEL_OFFSET 0x07
#define SEESAW_GPIO_BASE 0x01
#define SEESAW_GPIO_BULK_INPUT 0x04

#define JOYSTICK_X_CHANNEL 14
#define JOYSTICK_Y_CHANNEL 15

#define CENTER_LOW 411
#define CENTER_HIGH 611
#define OFFSET 200

#define BUTTON_A 5
#define BUTTON_B 1
#define BUTTON_X 6
#define BUTTON_Y 2
#define BUTTON_SELECT 0
#define BUTTON_START 16

#define BUTTON_MASK ((1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_X) | \
                     (1UL << BUTTON_Y) | (1UL << BUTTON_SELECT) | (1UL << BUTTON_START))


void seesaw_init(int i2c_fd) {
    uint8_t reset_cmd[] = {SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, 0xFF};
    write(i2c_fd, reset_cmd, sizeof(reset_cmd));
    usleep(500 * 1000);  // Wait 500 ms for device reset
}

uint16_t seesaw_analog_read(int i2c_fd, uint8_t channel) {
    uint8_t cmd[2] = {SEESAW_ADC_BASE, SEESAW_ADC_CHANNEL_OFFSET + channel};
    uint8_t read_buf[2];
    uint16_t analog_value;

    write(i2c_fd, cmd, sizeof(cmd));  // Send the channel read command
    read(i2c_fd, read_buf, sizeof(read_buf));  // Read the analog value

    // Convert from big-endian if necessary
    analog_value = (read_buf[0] << 8) | read_buf[1];
    analog_value = 1023 - analog_value;  // Reverse value to match joystick orientation

    // Debug output to check raw values
    printf("Raw ADC data: %d, %d -> %u\n", read_buf[0], read_buf[1], analog_value);

    return analog_value;
}

uint32_t read_buttons(int i2c_fd) {
    uint8_t cmd[2] = {SEESAW_GPIO_BASE, SEESAW_GPIO_BULK_INPUT};
    uint32_t buttons_state = 0;
    uint32_t mask_state;

    write(i2c_fd, cmd, sizeof(cmd)); 
    read(i2c_fd, &buttons_state, sizeof(buttons_state)); 

    
    mask_state = ~buttons_state & BUTTON_MASK;

    return mask_state;
}


void print_button_state(uint32_t buttons) {
    if (buttons & (1 << BUTTON_A)) printf("Button A pressed\n");
    if (buttons & (1 << BUTTON_B)) printf("Button B pressed\n");
    if (buttons & (1 << BUTTON_X)) printf("Button X pressed\n");
    if (buttons & (1 << BUTTON_Y)) printf("Button Y pressed\n");
    if (buttons & (1 << BUTTON_SELECT)) printf("Button SELECT pressed\n");
    if (buttons & (1 << BUTTON_START)) printf("Button START pressed\n");
}

int main() {
    int i2c_fd = open(I2C_DEVICE, O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open the I2C bus");
        return EXIT_FAILURE;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, GAMEPAD_ADDR) < 0) {
        perror("Failed to set I2C address");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    seesaw_init(i2c_fd);

    while (1) {
        uint16_t x = seesaw_analog_read(i2c_fd, JOYSTICK_X_CHANNEL);
        uint16_t y = seesaw_analog_read(i2c_fd, JOYSTICK_Y_CHANNEL);
        uint32_t buttons = read_buttons(i2c_fd);
        
        printf("\033[2J\033[H");  // Clear the screen and reset cursor position
        if (x >= CENTER_LOW && x <= CENTER_HIGH && y >= CENTER_LOW && y <= CENTER_HIGH) {
            printf("Joystick is centered\n");
        } else if (x < CENTER_LOW && y < CENTER_LOW) {
            printf("Joystick moving down left\n");
        } else if (x > CENTER_HIGH && y < CENTER_LOW) {
            printf("Joystick moving down right\n");
        } else if (x < CENTER_LOW && y > CENTER_HIGH) {
            printf("Joystick moving up left\n");
        } else if (x > CENTER_HIGH && y > CENTER_HIGH) {
            printf("Joystick moving up right\n");
        } else if (x < CENTER_LOW) {
            printf("Joystick moving left\n");
        } else if (x > CENTER_HIGH) {
            printf("Joystick moving right\n");
        } else if (y < CENTER_LOW) {
            printf("Joystick moving down\n");
        } else if (y > CENTER_HIGH) {
            printf("Joystick moving up\n");
        }
        //printf("Raw button data: 0x%08X\n", buttons);

        //print_button_state(buttons);
        // Clear the screen and move the cursor to the top-left corner
        
        printf("Joystick X: %d, Y: %d\n", x, y);
        usleep(100000);  // Update every 100ms
    }

    close(i2c_fd);
    return EXIT_SUCCESS;
}
