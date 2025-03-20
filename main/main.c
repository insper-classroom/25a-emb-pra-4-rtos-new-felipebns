#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>
#include "pico/util/datetime.h"

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

SemaphoreHandle_t xSemaphore;
QueueHandle_t xQueueTempo;
QueueHandle_t xQueueDistancia;

const int X_PIN =  16;
const int Y_PIN =  17;

volatile uint64_t start_us = 0;
volatile uint64_t end_us = 0;

void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { //fall
        end_us = to_us_since_boot(get_absolute_time());
        uint64_t dt = end_us - start_us;
        xQueueSendFromISR(xQueueTempo, &dt, 0);
    } else if (events == 0x8) { //rise
        start_us = to_us_since_boot(get_absolute_time());
    }
}

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void timer_btn_dt_init(void) {
    gpio_init(Y_PIN);
    gpio_set_dir(Y_PIN, GPIO_OUT);

    gpio_init(X_PIN);
    gpio_set_dir(X_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(X_PIN, (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL), true, &btn_callback);
}

void oled1_demo_1(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 15;
    while (1) {

        if (gpio_get(BTN_1_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_1_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 1 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_2_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_2_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 2 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_3_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_3_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 3 - ON");
            gfx_show(&disp);
        } else {

            gpio_put(LED_1_OLED, 1);
            gpio_put(LED_2_OLED, 1);
            gpio_put(LED_3_OLED, 1);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "PRESSIONE ALGUM");
            gfx_draw_string(&disp, 0, 10, 1, "BOTAO");
            gfx_draw_line(&disp, 15, 27, cnt,
                          27);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (++cnt == 112)
                cnt = 15;

            gfx_show(&disp);
        }
    }
}

void oled_task(void *p) {
    printf("Começa o processo \n");
    float distancia;

    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 0; //max 112 -> 400
                //min 15 -> 2 
    
    while (1){
        char buffer[20]; 
        if ((xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) && ((xQueueReceive(xQueueDistancia, &distancia, pdMS_TO_TICKS(1000))))){
            if (distancia > 2 && distancia < 400){
                sprintf(buffer, "%.2f cm", distancia);
                cnt = distancia*4.103092784;
            } else {
                sprintf(buffer, "Fora de alcance");
            }
        } else {
            sprintf(buffer, "Fio Desconectado");
        }
        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, buffer); 
        gfx_draw_line(&disp, 15, 27, cnt, 27);
        gfx_show(&disp);
    }
}

void echo_task(void *p) {
    printf("Calcula Echo \n");
    
    float vel_som = 0.03403;
    uint64_t dt;

    while (1) {
        if (xQueueReceive(xQueueTempo, &dt,  pdMS_TO_TICKS(100))) {
            float distancia = vel_som*dt/2;
            xQueueSend(xQueueDistancia, &distancia, pdMS_TO_TICKS(10));
        }
    }
}

void trigger_task(void *p) {
    printf("Liga o Trigger \n");

    while (1) {
        gpio_put(Y_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_put(Y_PIN, 0);
    
        xSemaphoreGive(xSemaphore);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int main() {
    stdio_init_all();

    timer_btn_dt_init();

    xSemaphore = xSemaphoreCreateBinary();
    xQueueTempo = xQueueCreate(32, sizeof(uint64_t));
    xQueueDistancia = xQueueCreate(32, sizeof(float));

    xTaskCreate(trigger_task, "Trigger Task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "Oled Task", 4095, NULL, 1, NULL);

    // xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    // xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) {
    }
        ;
}

