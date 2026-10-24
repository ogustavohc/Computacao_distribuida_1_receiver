#include <stdio.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define TAG "Sender"
#define LED_BRANCO 19
#define LED_VERMELHO 5
#define LED_VERDE 18
uint8_t mac_address[6];
int timeout_conexao = 0;

typedef struct __attribute__((packed)) {
    uint8_t n_piscadas;
    uint8_t cor_led;
    uint8_t velocidade_piscadas;
} dados_tarefa_t;

typedef struct __attribute__((packed)){
    uint8_t tipo_pacote;
    uint8_t esp_mac_address[6];
    bool concluida;
} dados_resposta_t;

typedef enum {
    TAREFA_RECEBIDA,
    TAREFA_CONCLUIDA,
} state_machine_tarefas;

state_machine_tarefas st_tarefas;
dados_tarefa_t resposta;

uint8_t transmissor_mac[] = {0xB8, 0x1F, 0x3F, 0xDA, 0x2A, 0xB8};

void on_data_sent(const esp_now_send_info_t *tx_info , esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Entregue com sucesso");
    } else {
        ESP_LOGW(TAG, "Falha na entrega");
    }
}

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len){
    if (len != sizeof(dados_tarefa_t)) return;

    memcpy(&resposta, data, sizeof(dados_tarefa_t));
    st_tarefas = TAREFA_RECEBIDA;
}

void task_tarefa_leds(void *params){
    int contador = 0;
    int led_piscante;
    for (;;){
        switch(st_tarefas){
            case TAREFA_RECEBIDA:
            timeout_conexao = 0;
                if (contador > resposta.n_piscadas){
                    st_tarefas = TAREFA_CONCLUIDA;
                    dados_resposta_t resposta = {.tipo_pacote = 2, .concluida = true};
                    memcpy(resposta.esp_mac_address, mac_address, sizeof(resposta.esp_mac_address));
                    esp_now_send(transmissor_mac, (uint8_t*)&resposta, sizeof(resposta));
                    contador = 0;
                    break;
                } else {
                    if (resposta.cor_led == 1){
                        led_piscante = LED_VERMELHO;
                    }
                    else if (resposta.cor_led == 2){
                        led_piscante = LED_VERDE;
                    }
                    else {
                        led_piscante = LED_BRANCO;
                    }
                    gpio_set_level(led_piscante, 1);
                    vTaskDelay(pdMS_TO_TICKS(resposta.velocidade_piscadas));
                    gpio_set_level(led_piscante, 0);
                    vTaskDelay(pdMS_TO_TICKS(resposta.velocidade_piscadas));
                    contador++;
                    break;
                }
            case TAREFA_CONCLUIDA:
                vTaskDelay(pdMS_TO_TICKS(1000));
                timeout_conexao++;
                if (timeout_conexao >= 30) {
                    dados_resposta_t config_esp_mac = {.concluida = 1,.tipo_pacote = 1};
                    memcpy(config_esp_mac.esp_mac_address, mac_address, sizeof(config_esp_mac.esp_mac_address));
                    esp_now_send(transmissor_mac, (uint8_t*)&config_esp_mac, sizeof(dados_resposta_t));
                    esp_now_send(transmissor_mac, (uint8_t*)&resposta, sizeof(resposta));
                    timeout_conexao = 0;
                }
                break;
        }
    }

}

void app_main(){
    gpio_set_direction(LED_BRANCO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_VERDE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_VERMELHO, GPIO_MODE_OUTPUT);

        nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, transmissor_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    esp_now_register_recv_cb(on_data_recv);

    esp_wifi_get_mac(WIFI_IF_STA, mac_address);
    dados_resposta_t config_esp_mac = {.concluida = 1,.tipo_pacote = 1};
    memcpy(config_esp_mac.esp_mac_address, mac_address, sizeof(config_esp_mac.esp_mac_address));

    esp_now_send(transmissor_mac, (uint8_t*)&config_esp_mac, sizeof(dados_resposta_t));

    xTaskCreate(task_tarefa_leds, "Tarefa Leds", 2048, NULL, 5, NULL);
}