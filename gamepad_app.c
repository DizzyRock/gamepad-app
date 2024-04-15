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

#define JOYSTICK_X_CHANNEL 14
#define JOYSTICK_Y_CHANNEL 15

#define CENTER_LOW 411
#define CENTER_HIGH 611
#define OFFSET 200


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
        printf("\033[2J\033[H");  // Clear the screen and reset cursor position
        if (x >= CENTER_LOW && x <= CENTER_HIGH && y >= CENTER_LOW && y <= CENTER_HIGH) {
            printf("Joystick is centered\n");
        } else if (x < CENTER_LOW && y < CENTER_LOW) {
            printf("Joystick moving up left\n");
        } else if (x > CENTER_HIGH && y < CENTER_LOW) {
            printf("Joystick moving up right\n");
        } else if (x < CENTER_LOW && y > CENTER_HIGH) {
            printf("Joystick moving down left\n");
        } else if (x > CENTER_HIGH && y > CENTER_HIGH) {
            printf("Joystick moving down right\n");
        } else if (x < CENTER_LOW) {
            printf("Joystick moving left\n");
        } else if (x > CENTER_HIGH) {
            printf("Joystick moving right\n");
        } else if (y < CENTER_LOW) {
            printf("Joystick moving up\n");
        } else if (y > CENTER_HIGH) {
            printf("Joystick moving down\n");
        }

        // Clear the screen and move the cursor to the top-left corner
        //printf("\033[2J\033[H");
        printf("Joystick X: %d, Y: %d\n", x, y);
        usleep(100000);  // Update every 100ms
    }

    close(i2c_fd);
    return EXIT_SUCCESS;
}
