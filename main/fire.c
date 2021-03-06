
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/queue.h"
#include "fire.h"

#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (900) // Frequency in Hertz. Set frequency at 5 kHz

#define LUTFRAMERATE (33) // ms  
#define POWEOFFTIME (60000 / LUTFRAMERATE ) // 60sec

#define INFO_LED_GPIO 5
#define LED0_GPIO 19
#define LED1_GPIO 18
#define LED2_GPIO 10
#define LED3_GPIO 7
#define LED4_GPIO 6

#define GPIO_OUTPUT_PIN_SEL  ((1ULL << INFO_LED_GPIO ))

#define LUTSIZE 1024

typedef enum ledstat_e {
    state_on,
    state_off,
    state_ramping_up,
    state_ramping_down
} ledstate_t;

static const char *TAG = "fire";

extern QueueHandle_t ledCommand_queue;
const uint16_t data[LUTSIZE] = {
    0x5EA, 0x5FC, 0x5F8, 0x5FE, 0x63A, 0x68E, 0x5D9, 0x580, 0x581, 0x5AC,
    0x5C3, 0x596, 0x590, 0x59E, 0x596, 0x5A7, 0x5CB, 0x5B4, 0x59F, 0x5A0,
    0x523, 0x4FA, 0x595, 0x625, 0x612, 0x58F, 0x527, 0x4B6, 0x48F, 0x4D9,
    0x4E7, 0x53A, 0x5C9, 0x62D, 0x65C, 0x686, 0x6AB, 0x69C, 0x6C8, 0x71C,
    0x711, 0x6C1, 0x675, 0x63A, 0x666, 0x678, 0x637, 0x690, 0x697, 0x68D,
    0x68F, 0x668, 0x63B, 0x62D, 0x5DE, 0x5C7, 0x5C8, 0x567, 0x584, 0x5D1,
    0x5BA, 0x5B0, 0x615, 0x6B0, 0x72D, 0x6EB, 0x6E2, 0x793, 0x85D, 0x95E,
    0xA97, 0x98F, 0x911, 0x8D8, 0x954, 0x8BB, 0x79E, 0x7B5, 0x78B, 0x6E3,
    0x71D, 0x706, 0x733, 0x85A, 0x7FB, 0x829, 0x73F, 0x794, 0x79C, 0x7B5,
    0x924, 0xACD, 0x9B6, 0x760, 0x69E, 0x83D, 0x7C2, 0x785, 0x7E9, 0x75B,
    0x81B, 0x861, 0x819, 0x7AF, 0x81B, 0x7B0, 0x7A0, 0x87D, 0xA4D, 0x88B,
    0x717, 0x592, 0x600, 0xC09, 0xA5E, 0x9D8, 0xA25, 0xB0B, 0xA30, 0x96C,
    0x851, 0x841, 0x7F3, 0x9A2, 0x820, 0x8F2, 0x917, 0xA16, 0xA4C, 0xA00,
    0xA53, 0x91D, 0x909, 0xA1A, 0x9D6, 0xA07, 0xA7D, 0x93C, 0x80B, 0x6B6,
    0x703, 0x891, 0xAD1, 0xA26, 0x646, 0x89D, 0x948, 0xA09, 0xA68, 0x8A3,
    0x71D, 0x993, 0xB3A, 0xA82, 0x7E9, 0x65D, 0x4D1, 0x6F6, 0x80C, 0x7D4,
    0x768, 0x7C8, 0x8B4, 0x900, 0x908, 0x6DD, 0x694, 0x820, 0x889, 0x807,
    0x8F1, 0x8F7, 0x8C1, 0x911, 0x809, 0x78F, 0x81F, 0x7E4, 0x76B, 0x762,
    0x73A, 0x744, 0x73A, 0x880, 0x92B, 0x853, 0x74F, 0x673, 0x6D7, 0x77A,
    0x708, 0x76C, 0x839, 0x8DA, 0x878, 0x6DC, 0x5CB, 0x6BC, 0x8E9, 0x8E5,
    0x752, 0x9A8, 0xB2F, 0xA21, 0x95A, 0x911, 0x863, 0x736, 0x6F9, 0x75F,
    0x778, 0x716, 0x6EA, 0x6CF, 0x689, 0x63E, 0x627, 0x69F, 0x6FF, 0x6B5,
    0x67C, 0x6AE, 0x689, 0x641, 0x64B, 0x64A, 0x66D, 0x6BF, 0x6EA, 0x6E1,
    0x6CB, 0x6B5, 0x6BD, 0x6C5, 0x6C5, 0x6BD, 0x6AD, 0x670, 0x624, 0x5F4,
    0x5CE, 0x5B8, 0x5A4, 0x599, 0x591, 0x591, 0x598, 0x5A2, 0x5C8, 0x607,
    0x675, 0x6F6, 0x75B, 0x789, 0x77F, 0x763, 0x738, 0x713, 0x6F1, 0x6D0,
    0x6AE, 0x699, 0x675, 0x636, 0x60C, 0x5FC, 0x635, 0x6D4, 0x765, 0x71E,
    0x633, 0x5C4, 0x5FE, 0x5C5, 0x514, 0x542, 0x579, 0x573, 0x59F, 0x5B0,
    0x5AC, 0x5B0, 0x5A0, 0x596, 0x592, 0x585, 0x57B, 0x56E, 0x569, 0x569,
    0x56F, 0x571, 0x568, 0x578, 0x5B3, 0x5A5, 0x59F, 0x59C, 0x58B, 0x57E,
    0x558, 0x529, 0x4D1, 0x47D, 0x52E, 0x5B1, 0x540, 0x504, 0x562, 0x56D,
    0x525, 0x528, 0x52D, 0x574, 0x5CB, 0x5FA, 0x5FE, 0x5AA, 0x52B, 0x4FA,
    0x4FD, 0x549, 0x5CA, 0x5DA, 0x57B, 0x60E, 0x670, 0x74B, 0x70C, 0x781,
    0x737, 0x610, 0x560, 0x621, 0x786, 0x780, 0x5BC, 0x596, 0x570, 0x588,
    0x5FE, 0x615, 0x5EB, 0x615, 0x5C3, 0x5E9, 0x62D, 0x5CC, 0x58C, 0x5C0,
    0x5EF, 0x57D, 0x539, 0x56B, 0x500, 0x547, 0x5BA, 0x5E2, 0x576, 0x55C,
    0x595, 0x59C, 0x581, 0x54C, 0x4F8, 0x504, 0x516, 0x521, 0x5AC, 0x639,
    0x54B, 0x53E, 0x5C1, 0x589, 0x5B9, 0x4F5, 0x5C9, 0x63E, 0x5D1, 0x678,
    0x6A9, 0x6C2, 0x682, 0x648, 0x526, 0x5F4, 0x781, 0x78E, 0x703, 0x693,
    0x58F, 0x620, 0x711, 0x5A7, 0x4B4, 0x4C7, 0x4C4, 0x4D8, 0x527, 0x586,
    0x571, 0x513, 0x557, 0x53C, 0x54A, 0x61D, 0x650, 0x697, 0x627, 0x568,
    0x414, 0x5D1, 0x743, 0x66F, 0x63F, 0x67E, 0x641, 0x544, 0x506, 0x4FD,
    0x4D9, 0x4D6, 0x4C9, 0x4E6, 0x5D4, 0x63B, 0x609, 0x639, 0x599, 0x6AA,
    0x724, 0x6FF, 0x69C, 0x645, 0x612, 0x5CD, 0x6C7, 0x7C0, 0x6F3, 0x564,
    0x53F, 0x5C7, 0x5CE, 0x56B, 0x536, 0x54B, 0x4FD, 0x54D, 0x6A3, 0x706,
    0x6B9, 0x6B4, 0x6E3, 0x6C1, 0x67C, 0x65E, 0x60F, 0x5CE, 0x5DC, 0x5F8,
    0x613, 0x633, 0x670, 0x6B1, 0x6A2, 0x609, 0x579, 0x56F, 0x59E, 0x5B9,
    0x5D2, 0x617, 0x5CF, 0x531, 0x539, 0x56C, 0x5E0, 0x6D7, 0x73D, 0x718,
    0x60F, 0x5B3, 0x655, 0x6CB, 0x6E0, 0x653, 0x708, 0x8A2, 0x7E8, 0x6AB,
    0x731, 0x813, 0x7A2, 0x7A7, 0x701, 0x590, 0x43B, 0x637, 0x8D2, 0x877,
    0x5FB, 0x5F9, 0x5D3, 0x583, 0x546, 0x636, 0x6E7, 0x693, 0x67A, 0x693,
    0x6B3, 0x672, 0x62C, 0x650, 0x6A0, 0x62A, 0x596, 0x531, 0x52E, 0x4E2,
    0x5A2, 0x621, 0x60A, 0x5BB, 0x5EC, 0x645, 0x650, 0x642, 0x659, 0x67E,
    0x66C, 0x671, 0x6B4, 0x6C6, 0x6E4, 0x6E1, 0x660, 0x619, 0x66A, 0x677,
    0x679, 0x68E, 0x652, 0x558, 0x528, 0x5A2, 0x5DA, 0x5DE, 0x5F6, 0x5F0,
    0x5E0, 0x5D4, 0x5B6, 0x5B3, 0x5EF, 0x5FA, 0x618, 0x61F, 0x5FC, 0x60B,
    0x5DA, 0x5AA, 0x59A, 0x5C4, 0x5FA, 0x636, 0x663, 0x653, 0x676, 0x68B,
    0x636, 0x575, 0x54C, 0x5A3, 0x5E2, 0x5F6, 0x659, 0x669, 0x5F1, 0x4E5,
    0x44E, 0x683, 0x7B7, 0x710, 0x708, 0x734, 0x715, 0x70B, 0x6D8, 0x682,
    0x662, 0x661, 0x658, 0x5C9, 0x4F8, 0x53A, 0x600, 0x5B6, 0x53E, 0x522,
    0x617, 0x617, 0x67A, 0x684, 0x6D9, 0x639, 0x582, 0x5A6, 0x565, 0x49A,
    0x4F3, 0x5D4, 0x586, 0x501, 0x4B8, 0x4C4, 0x5E1, 0x680, 0x583, 0x494,
    0x4E1, 0x5A7, 0x5F6, 0x5C7, 0x5E3, 0x603, 0x533, 0x54F, 0x59E, 0x573,
    0x56B, 0x587, 0x5CE, 0x5B0, 0x590, 0x593, 0x57A, 0x579, 0x596, 0x5CD,
    0x5EB, 0x5A6, 0x561, 0x526, 0x522, 0x542, 0x55D, 0x56D, 0x559, 0x533,
    0x519, 0x506, 0x510, 0x551, 0x58D, 0x595, 0x575, 0x59D, 0x5D2, 0x5EA,
    0x5EE, 0x5FA, 0x61B, 0x633, 0x606, 0x5CE, 0x5AC, 0x597, 0x58C, 0x58D,
    0x595, 0x59E, 0x598, 0x591, 0x599, 0x5A9, 0x5CA, 0x5D8, 0x5DB, 0x5D3,
    0x5C7, 0x5AE, 0x58A, 0x5B3, 0x628, 0x677, 0x6B7, 0x660, 0x5A2, 0x57B,
    0x638, 0x68F, 0x62F, 0x63F, 0x623, 0x629, 0x630, 0x691, 0x6EA, 0x722,
    0x76D, 0x7E5, 0x807, 0x82D, 0x884, 0x8A6, 0x8DE, 0x7F1, 0x7CC, 0x7EC,
    0x819, 0x817, 0x7E2, 0x6E6, 0x677, 0x747, 0x74E, 0x860, 0x8DB, 0x830,
    0x6C1, 0x717, 0x7E7, 0x80A, 0x75A, 0x6A2, 0x6DE, 0x6F2, 0x617, 0x5BC,
    0x5B4, 0x605, 0x670, 0x671, 0x5CA, 0x5B1, 0x628, 0x6A2, 0x6AD, 0x65E,
    0x662, 0x693, 0x68A, 0x622, 0x64F, 0x6C5, 0x6F4, 0x69F, 0x663, 0x653,
    0x629, 0x628, 0x614, 0x5BB, 0x56D, 0x52F, 0x517, 0x521, 0x540, 0x563,
    0x58D, 0x5C9, 0x603, 0x62E, 0x667, 0x687, 0x6A1, 0x6A8, 0x69F, 0x684,
    0x681, 0x69D, 0x6A6, 0x66A, 0x63B, 0x60F, 0x623, 0x650, 0x61B, 0x5F0,
    0x5FE, 0x616, 0x622, 0x61C, 0x60D, 0x631, 0x691, 0x67E, 0x5DA, 0x60C,
    0x5F3, 0x59B, 0x5CA, 0x61F, 0x5D3, 0x560, 0x537, 0x51A, 0x521, 0x535,
    0x540, 0x548, 0x53C, 0x529, 0x528, 0x553, 0x5A1, 0x5DD, 0x5E9, 0x5D6,
    0x5B2, 0x59E, 0x5A1, 0x58E, 0x56B, 0x577, 0x572, 0x554, 0x599, 0x642,
    0x587, 0x516, 0x59E, 0x5C5, 0x5C6, 0x5BC, 0x582, 0x543, 0x536, 0x56E,
    0x589, 0x57F, 0x593, 0x59D, 0x5AC, 0x55B, 0x573, 0x5FA, 0x5E1, 0x59C,
    0x5D6, 0x61E, 0x608, 0x59F, 0x50E, 0x519, 0x51F, 0x555, 0x61A, 0x659,
    0x658, 0x646, 0x62F, 0x5D3, 0x5CA, 0x62D, 0x5CD, 0x5D2, 0x5B6, 0x5A9,
    0x5D8, 0x6B6, 0x62C, 0x6C9, 0x6EE, 0x6FA, 0x738, 0x73C, 0x6EB, 0x6C6,
    0x6C0, 0x5FB, 0x5F5, 0x516, 0x51A, 0x5D0, 0x6A7, 0x68D, 0x62E, 0x597,
    0x59B, 0x83E, 0x7C9, 0x673, 0x657, 0x6D6, 0x698, 0x621, 0x63B, 0x61A,
    0x60A, 0x626, 0x684, 0x607, 0x568, 0x69D, 0x7A7, 0x67A, 0x586, 0x854,
    0x844, 0x61F, 0x721, 0x6FC, 0x682, 0x5EE, 0x4FB, 0x4F8, 0x53E, 0x591,
    0x5BF, 0x5F8, 0x5AF, 0x5D4, 0x5C0, 0x5BF, 0x586, 0x56E, 0x597, 0x5B0,
    0x580, 0x609, 0x6A9, 0x616, 0x5F7, 0x696, 0x763, 0x5F8, 0x64A, 0x6A8,
    0x555, 0x4D0, 0x590, 0x608, 0x735, 0x5CC, 0x418, 0x3C1, 0x4F7, 0x640,
    0x50E, 0x4AB, 0x5EA, 0x5FB, 0x5F3, 0x5FE, 0x63C, 0x68C, 0x5D5, 0x57F,
    0x57E, 0x5AD, 0x5C4, 0x597, 0x590, 0x59C, 0x594, 0x5AB, 0x5CD, 0x5B5,
    0x59E, 0x59B, 0x520, 0x4F9, 0x594, 0x620, 0x611, 0x58E, 0x51F, 0x4B1,
    0x48A, 0x4D3, 0x4E7, 0x53A, 0x5CC, 0x62C, 0x65E, 0x684, 0x6AC, 0x69B,
    0x6C7, 0x720, 0x710, 0x6C2, 0x673, 0x638, 0x66B, 0x679, 0x633, 0x68D,
    0x693, 0x68C, 0x690, 0x669, 0x63F, 0x62D, 0x5DE, 0x5C9, 0x5C9, 0x567,
    0x588, 0x5D0, 0x5BB, 0x5AA, 0x618, 0x6B2, 0x72C, 0x6EE, 0x6EA, 0x79E,
    0x865, 0x964, 0xA91, 0x991, 0x914, 0x8E1, 0x95C, 0x8BC, 0x79C, 0x7B9,
    0x789, 0x6E5, 0x71A, 0x703
};




static void ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    // All Led will use the same timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY, 
        .clk_cfg          = LEDC_AUTO_CLK
    };

    ledc_timer.timer_num = LEDC_TIMER_0;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .intr_type      = LEDC_INTR_DISABLE,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };

    ledc_channel.channel        = LEDC_CHANNEL_0;
    ledc_channel.timer_sel      = LEDC_TIMER_0;
    ledc_channel.gpio_num       = LED0_GPIO;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel        = LEDC_CHANNEL_1;
    ledc_channel.timer_sel      = LEDC_TIMER_0;
    ledc_channel.gpio_num       = LED1_GPIO;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel        = LEDC_CHANNEL_2;
    ledc_channel.timer_sel      = LEDC_TIMER_0;
    ledc_channel.gpio_num       = LED2_GPIO;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel        = LEDC_CHANNEL_3;
    ledc_channel.timer_sel      = LEDC_TIMER_0;
    ledc_channel.gpio_num       = LED3_GPIO;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel        = LEDC_CHANNEL_4;
    ledc_channel.timer_sel      = LEDC_TIMER_0;
    ledc_channel.gpio_num       = LED4_GPIO;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}


void fire_task(void *pvParameters){

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    ledc_init();

    int led0Index = 4;
    int led1Index = 3;
    int led2Index = 2;
    int led3Index = 1;
    int led4Index = 0;

    ledstate_t state = state_on;

    unsigned int powerOffCounter = POWEOFFTIME;


    command_t cmd;
    while(1) {
        cmd = none;
        command_t tmp_cmd = none;
        if(xQueueReceive(ledCommand_queue, &tmp_cmd, 0) == pdTRUE){
            cmd = tmp_cmd;
        }

        switch(cmd)
        {
            case off:
            ESP_LOGI(TAG, "CMD off");
            if (state != state_off) {
                state = state_off;
                powerOffCounter = POWEOFFTIME;
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2));
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3));
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_4, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_4));
            }
            break;

            case on:
            ESP_LOGI(TAG, "CMD on");
            if (state != state_on) {
                led0Index = 4;
                led1Index = 3;
                led2Index = 2;
                led3Index = 1;
                led4Index = 0;
                state = state_on;
            }
            break;  
            default:
            break;          
        }

        if (state == state_on) {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, data[led0Index]));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, data[led1Index]));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, data[led2Index]));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, data[led3Index]));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_4, data[led4Index]));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_4));
        } else if (state == state_off) {
            powerOffCounter--;
            if (powerOffCounter == 0) {
                state = state_on;
            }
        }

        
        if(++led0Index >= LUTSIZE)
            led0Index = 0;

        if(++led1Index >= LUTSIZE)
            led1Index = 0;

        if(++led2Index >= LUTSIZE)
            led2Index = 0;

        if(++led3Index >= LUTSIZE)
            led3Index = 0;

        if(++led4Index >= LUTSIZE)
            led4Index = 0;

        vTaskDelay(LUTFRAMERATE / portTICK_RATE_MS);
    }

}