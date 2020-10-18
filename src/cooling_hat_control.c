#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "cooling_hat_fan.h"
#include "cooling_hat_i2c.h"
#include "cooling_hat_information.h"
#include "cooling_hat_oled.h"
#include "cooling_hat_rgb.h"
#include "cooling_hat_utils.h"

#define BASIC_DELAY_IN_SECONDS 1
#define DISK_REFRESH_DELAY_IN_LOOP_COUNT 600 // a hour
#define CPU_REFRESH_DELAY_IN_LOOP_COUNT 60 // a minute
#define RAM_REFRESH_DELAY_IN_LOOP_COUNT 60 // a minute
#define TEMP_REFRESH_DELAY_IN_LOOP_COUNT 8 // 8 seconds

static struct oled_information information = {0};
static unsigned int loop_counter = 0;
static double previous_temperature = 0, temperature = 0;
static bool has_information_changed = false;

struct temperature_fan_range {
    uint8_t temperature;
    enum fan_speed speed;
};

struct temperature_fan_range temperature_fan_ranges[] = {
        {.temperature = 45, .speed = fan_speed_0_percent},
        {.temperature = 50, .speed = fan_speed_40_percent},
        {.temperature = 60, .speed = fan_speed_60_percent},
        {.temperature = 70, .speed = fan_speed_80_percent},
        {.temperature = 255, .speed = fan_speed_100_percent},
};

int main(int argc, char *argv[]) {

    if (!i2c_init()) {
        return -1;
    }

    DEBUG_PRINT("[APP] Initialization");
    oled_initialization();
    set_fan_speed(fan_speed_0_percent);
    rgb_off();
    usleep(BASIC_DELAY_IN_SECONDS);
    DEBUG_PRINT("[APP] Initialization is over");

    // The IPv4 address will be retrieved only once.
    get_ip_address(information.m_network_address, sizeof(information.m_network_address));

    while (1) {
        if (loop_counter % CPU_REFRESH_DELAY_IN_LOOP_COUNT == 0) {
            get_average_load(information.m_average_load, sizeof(information.m_average_load));
            DEBUG_PRINT("[APP] CPU load string `%s`", information.m_average_load);
            has_information_changed = true;
        }
        if (loop_counter % DISK_REFRESH_DELAY_IN_LOOP_COUNT == 0) {
            get_disk_usage(information.m_disk_usage, sizeof(information.m_disk_usage));
            DEBUG_PRINT("[APP] Disk usage string `%s`", information.m_disk_usage);
            has_information_changed = true;
        }
        if (loop_counter % RAM_REFRESH_DELAY_IN_LOOP_COUNT == 0) {
            get_ram_usage(information.m_ram_usage, sizeof(information.m_ram_usage));
            DEBUG_PRINT("[APP] RAM usage string `%s`", information.m_ram_usage);
            has_information_changed = true;
        }
        if (loop_counter % TEMP_REFRESH_DELAY_IN_LOOP_COUNT == 0) {
            temperature = get_temperature_double();
            get_temperature(information.m_cpu_temperature, sizeof(information.m_cpu_temperature), temperature);
            DEBUG_PRINT("[APP] Temperature string `%s`", information.m_cpu_temperature);
        }

        if (previous_temperature != temperature) {
            int index = 0;
            has_information_changed = true;
            for (; index < sizeof(temperature_fan_ranges); ++index) {
                if ((uint8_t) (temperature + 0.5) < temperature_fan_ranges[index].temperature) {
                    DEBUG_PRINT("[APP] Temperature threshold found %uC, fan speed %u",
                                temperature_fan_ranges[index].temperature,
                                temperature_fan_ranges[index].speed);
                    set_fan_speed(temperature_fan_ranges[index].speed);
                    get_fan_speed(information.m_fan_speed, sizeof(information.m_fan_speed),
                                  temperature_fan_ranges[index].speed);
                    DEBUG_PRINT("[APP] Fan string `%s`", information.m_fan_speed);
                    break;
                }
            }

            previous_temperature = temperature;
        }

        if (has_information_changed) {
            oled_refresh(&information);
            has_information_changed = false;
        }

        ++loop_counter;
        sleep(BASIC_DELAY_IN_SECONDS);
    }

    return 0;
}
