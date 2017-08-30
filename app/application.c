#include <application.h>
#include <bcl.h>

#define COUNT 70

#define MODE_EFFECT 0
#define MODE_SPEED 1
#define MODE_BRIGHTNESS 2

bc_led_t led;
bc_led_strip_t led_strip;
uint32_t color;
int mode = MODE_EFFECT;
int effect = 0;
int wait = 45;
int brightness = 255;

bc_scheduler_task_id_t write_task_id;
bc_scheduler_task_id_t stroboscope_task_id;
bc_scheduler_task_id_t snake_task_id;

static uint32_t _dma_buffer[COUNT * 4 * 2]; // count * type * 2

static const bc_led_strip_buffer_t _led_strip_buffer =
{
    .type = BC_LED_STRIP_TYPE_RGBW,
    .count = COUNT,
    .buffer = _dma_buffer
};

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
void led_strip_event_handler(bc_led_strip_t *self, bc_led_strip_event_t event, void *event_param);
void encoder_event_handler(bc_module_encoder_event_t event, void *event_param);
void write_task(void *param);
void stroboscope_task(void *param);
void snake_task(void *param);

void application_init(void)
{

    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);


    static bc_button_t button;
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_module_encoder_init();
    bc_module_encoder_set_event_handler(encoder_event_handler, NULL);

    bc_module_power_init();
    bc_led_strip_init(&led_strip, bc_module_power_get_led_strip_driver(), &_led_strip_buffer);
    bc_led_strip_set_event_handler(&led_strip, led_strip_event_handler, NULL);

    bc_led_strip_effect_rainbow(&led_strip, wait);

    write_task_id = bc_scheduler_register(write_task, NULL, BC_TICK_INFINITY);
    stroboscope_task_id = bc_scheduler_register(stroboscope_task, NULL, BC_TICK_INFINITY);
    snake_task_id = bc_scheduler_register(snake_task, NULL, BC_TICK_INFINITY);

    bc_led_set_mode(&led, BC_LED_MODE_OFF);
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        if (++mode > 2)
        {
            mode = 0;
        }
    }
//    else if (event == BC_BUTTON_EVENT_HOLD)
//    {
//        bc_scheduler_plan_now(set_brightness_task_id);
//    }
//    else if (event == BC_BUTTON_EVENT_RELEASE)
//    {
//        bc_scheduler_plan_absolute(set_brightness_task_id, BC_TICK_INFINITY);
//    }

}

void led_strip_event_handler(bc_led_strip_t *self, bc_led_strip_event_t event, void *event_param)
{
    (void) event_param;

    if (event == BC_LED_STRIP_EVENT_EFFECT_DONE)
    {
        if (effect == 2)
        {
            color >>= 8;
            if (color == 0x00000000)
            {
                color = 0xff000000;
            }
            bc_led_strip_effect_color_wipe(self, color, wait);
        }
    }
}

void mode_effect(int increment)
{
    effect += increment;
    if (effect > 7)
    {
        effect = 0;
    }
    else if (effect < 0)
    {
        effect = 7;
    }

    bc_scheduler_plan_absolute(stroboscope_task_id, BC_TICK_INFINITY);
    bc_scheduler_plan_absolute(snake_task_id, BC_TICK_INFINITY);
    bc_led_strip_effect_stop(&led_strip);

    if (effect == 0)
    {
        bc_led_strip_effect_rainbow(&led_strip, wait);
    }
    else if (effect == 1)
    {
        bc_led_strip_effect_rainbow_cycle(&led_strip, wait);
    }
    else if (effect == 2)
    {
        color = 0xff000000;
        bc_led_strip_fill(&led_strip, 0x00000000);
        bc_led_strip_effect_color_wipe(&led_strip, color, wait);
    }
    else if (effect == 3)
    {
        bc_led_strip_effect_theater_chase(&led_strip, color, wait);
    }
    else if (effect == 4)
    {
        bc_led_strip_effect_theater_chase_rainbow(&led_strip, wait);
    }
    else if (effect == 5)
    {
        bc_scheduler_plan_now(stroboscope_task_id);
    }
    else if (effect == 6)
    {
        bc_led_strip_fill(&led_strip, 0x00000000);
        bc_scheduler_plan_now(snake_task_id);
    }
    else if (effect == 7)
    {
        bc_led_strip_fill(&led_strip, 0x00000000);
        bc_led_strip_write(&led_strip);
        bc_scheduler_plan_now(write_task_id);
    }
}

void mode_speed(int increment)
{
    wait += increment * 10;

    if (wait < 10)
    {
        wait = 10;
    }
    else if (wait > 1000)
    {
        wait = 1000;
    }

    led_strip._effect.wait = (bc_tick_t) wait;
}

void mode_brightness(int increment)
{
    brightness += increment * 5;
    if (brightness < 0)
    {
        brightness = 0;
    }
    else if (brightness > 255)
    {
        brightness = 255;
    }
    bc_led_strip_set_brightness(&led_strip, brightness);
    bc_scheduler_plan_current_relative(100);
}

void encoder_event_handler(bc_module_encoder_event_t event, void *event_param)
{
    (void) event_param;

    if (event == BC_MODULE_ENCODER_EVENT_ROTATION)
    {
        int increment = bc_module_encoder_get_increment();

        if (mode == MODE_EFFECT)
        {
            mode_effect(increment > 0 ? 1 : -1);
        }
        else if (mode == MODE_SPEED)
        {
            mode_speed(increment);
        }
        else if (mode == MODE_BRIGHTNESS)
        {
            mode_brightness(increment);
        }
    }
}

void write_task(void *param)
{
    (void) param;
    if (!bc_led_strip_write(&led_strip))
    {
        bc_scheduler_plan_current_now();
    }
}

void stroboscope_task(void *param)
{
    (void) param;
    bc_led_strip_fill(&led_strip, color);
    color = color == 0xff ? 0 : 0xff;
    bc_led_strip_write(&led_strip);
    bc_scheduler_plan_current_relative(wait);
}

void snake_task(void *param)
{
    (void) param;
    const int length = 10;
    static int position = -length;

    for (int i = position; (i < position + length) && i < COUNT; i++) {
        if (i < 0)
        {
            continue;
        }
        bc_led_strip_set_pixel(&led_strip, i, 0x00);
    }

    position++;
    if (position == COUNT)
    {
        position = -length;
    }

    uint32_t color = 0xff / length;

    for (int i = position; (i < position + length) && i < COUNT; i++) {
        if (i < 0)
        {
            continue;
        }
        bc_led_strip_set_pixel(&led_strip, i, color | 0x20000000);
        color += 0xff / length;
    }

    bc_led_strip_write(&led_strip);
    bc_scheduler_plan_current_relative(wait);
}

