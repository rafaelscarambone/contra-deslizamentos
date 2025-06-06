/**
 * Sistema de Monitoramento com Dois Sensores de Umidade do Solo
 * Autor: Sistema Profissional de Monitoramento
 * Versão: 2.0.0
 * Data: 2024
 * 
 * Descrição: Sistema completo para leitura de dois sensores capacitivos
 * de umidade do solo com cálculo de média, filtros de ruído e 
 * tratamento avançado de erros.
 * 
 * Hardware necessário:
 * - ESP32 DevKit
 * - 2x Sensores Capacitivos de Umidade do Solo
 * - Jumpers para conexão
 * - Fonte de alimentação 3.3V/5V
 */

// ==================== BIBLIOTECAS ====================
#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

// ==================== CONFIGURAÇÕES DE HARDWARE ====================
// Pinos dos sensores (ADC1 do ESP32)
#define SENSOR1_PIN 34  // GPIO34 (ADC1_CH6) - Sensor 1
#define SENSOR2_PIN 35  // GPIO35 (ADC1_CH7) - Sensor 2

// Pinos de LED para indicação visual
#define LED_STATUS 2    // LED interno do ESP32
#define LED_ALERT 15    // LED externo para alertas

// ==================== CONFIGURAÇÕES DO SISTEMA ====================
// Intervalo entre leituras (em milissegundos)
#define INTERVALO_LEITURA 1000  // 1 segundo (configurável)

// Número de amostras para média móvel (reduz ruído)
#define NUM_AMOSTRAS 10

// Limites de calibração dos sensores
#define VALOR_SECO 3500      // Valor ADC quando solo está seco
#define VALOR_MOLHADO 1500   // Valor ADC quando solo está molhado

// Limites de validação (detecção de erro)
#define VALOR_MIN_VALIDO 100    // Valor mínimo aceitável (sensor desconectado)
#define VALOR_MAX_VALIDO 4095   // Valor máximo do ADC de 12 bits

// Limites de umidade para alertas
#define UMIDADE_CRITICA_BAIXA 20   // Abaixo disso, solo muito seco
#define UMIDADE_IDEAL_MIN 40        // Faixa ideal mínima
#define UMIDADE_IDEAL_MAX 60        // Faixa ideal máxima
#define UMIDADE_CRITICA_ALTA 80     // Acima disso, solo muito molhado

// ==================== VARIÁVEIS GLOBAIS ====================
// Arrays para armazenar múltiplas leituras (filtro de média móvel)
int leiturasSensor1[NUM_AMOSTRAS];
int leiturasSensor2[NUM_AMOSTRAS];
int indiceAmostra = 0;

// Variáveis para armazenar valores processados
float umidadeSensor1 = 0;
float umidadeSensor2 = 0;
float umidadeMedia = 0;

// Variáveis de controle de tempo
unsigned long ultimaLeitura = 0;

// Contadores de erro
int errosSensor1 = 0;
int errosSensor2 = 0;

// Estrutura para calibração ADC
esp_adc_cal_characteristics_t *adc_chars;

// ==================== ESTRUTURAS DE DADOS ====================
// Estrutura para armazenar dados de um sensor
struct DadosSensor {
    int pinoADC;
    float umidadeAtual;
    float umidadeAnterior;
    bool emErro;
    String nome;
};

// Instâncias dos sensores
DadosSensor sensor1 = {SENSOR1_PIN, 0, 0, false, "Sensor 1"};
DadosSensor sensor2 = {SENSOR2_PIN, 0, 0, false, "Sensor 2"};

// ==================== FUNÇÕES DE INICIALIZAÇÃO ====================
/**
 * Configura o hardware e inicializa o sistema
 */
void setup() {
    // Inicializa comunicação serial
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("Sistema de Monitoramento Duplo v2.0");
    Serial.println("Inicializando...");
    Serial.println("========================================");
    
    // Configura pinos de LED
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LED_ALERT, OUTPUT);
    
    // Pisca LEDs na inicialização
    for(int i = 0; i < 3; i++) {
        digitalWrite(LED_STATUS, HIGH);
        digitalWrite(LED_ALERT, HIGH);
        delay(100);
        digitalWrite(LED_STATUS, LOW);
        digitalWrite(LED_ALERT, LOW);
        delay(100);
    }
    
    // Configura ADC
    configurarADC();
    
    // Inicializa arrays de leitura
    for(int i = 0; i < NUM_AMOSTRAS; i++) {
        leiturasSensor1[i] = 0;
        leiturasSensor2[i] = 0;
    }
    
    // Realiza leituras iniciais para estabilizar
    Serial.println("Calibrando sensores...");
    for(int i = 0; i < NUM_AMOSTRAS; i++) {
        leiturasSensor1[i] = analogRead(SENSOR1_PIN);
        leiturasSensor2[i] = analogRead(SENSOR2_PIN);
        delay(50);
    }
    
    Serial.println("Sistema inicializado com sucesso!");
    Serial.println("========================================\n");
}

/**
 * Configura o ADC do ESP32 para máxima precisão
 */
void configurarADC() {
    // Configura resolução de 12 bits (0-4095)
    analogReadResolution(12);
    
    // Configura atenuação para range completo (0-3.3V)
    analogSetAttenuation(ADC_11db);
    
    // Caracteriza o ADC para calibração
    adc_chars = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);
    
    Serial.println("ADC configurado: 12 bits, 0-3.3V");
}

// ==================== FUNÇÕES DE LEITURA ====================
/**
 * Realiza leitura de um sensor com validação
 * @param pino Pino do sensor a ser lido
 * @return Valor lido ou -1 em caso de erro
 */
int lerSensorComValidacao(int pino) {
    int leitura = analogRead(pino);
    
    // Valida se a leitura está dentro dos limites esperados
    if(leitura < VALOR_MIN_VALIDO || leitura > VALOR_MAX_VALIDO) {
        return -1; // Indica erro
    }
    
    return leitura;
}

/**
 * Realiza múltiplas leituras e retorna a média
 * @param pino Pino do sensor
 * @param numLeituras Número de leituras para média
 * @return Média das leituras válidas
 */
float lerSensorComMedia(int pino, int numLeituras = 5) {
    float soma = 0;
    int leiturasValidas = 0;
    
    for(int i = 0; i < numLeituras; i++) {
        int leitura = lerSensorComValidacao(pino);
        if(leitura != -1) {
            soma += leitura;
            leiturasValidas++;
        }
        delay(10); // Pequeno delay entre leituras
    }
    
    // Retorna -1 se não houver leituras válidas
    if(leiturasValidas == 0) return -1;
    
    return soma / leiturasValidas;
}

/**
 * Converte valor ADC para porcentagem de umidade
 * @param valorADC Valor lido do ADC
 * @return Porcentagem de umidade (0-100%)
 */
float converterParaUmidade(int valorADC) {
    // Inverte a escala (sensor capacitivo: menor valor = mais úmido)
    float umidade = map(valorADC, VALOR_SECO, VALOR_MOLHADO, 0, 100);
    
    // Limita entre 0 e 100%
    umidade = constrain(umidade, 0, 100);
    
    return umidade;
}

/**
 * Aplica filtro de média móvel para suavizar leituras
 * @param novaLeitura Nova leitura do sensor
 * @param arrayLeituras Array de leituras anteriores
 * @return Média móvel das leituras
 */
float aplicarFiltroMediaMovel(int novaLeitura, int* arrayLeituras) {
    // Adiciona nova leitura ao array circular
    arrayLeituras[indiceAmostra] = novaLeitura;
    
    // Calcula média de todas as amostras
    float soma = 0;
    for(int i = 0; i < NUM_AMOSTRAS; i++) {
        soma += arrayLeituras[i];
    }
    
    return soma / NUM_AMOSTRAS;
}

// ==================== FUNÇÕES DE PROCESSAMENTO ====================
/**
 * Processa leituras dos sensores e calcula médias
 */
void processarLeituras() {
    // Lê Sensor 1
    float leitura1 = lerSensorComMedia(SENSOR1_PIN);
    if(leitura1 != -1) {
        float mediaMovel1 = aplicarFiltroMediaMovel(leitura1, leiturasSensor1);
        sensor1.umidadeAnterior = sensor1.umidadeAtual;
        sensor1.umidadeAtual = converterParaUmidade(mediaMovel1);
        sensor1.emErro = false;
        errosSensor1 = 0;
    } else {
        sensor1.emErro = true;
        errosSensor1++;
    }
    
    // Lê Sensor 2
    float leitura2 = lerSensorComMedia(SENSOR2_PIN);
    if(leitura2 != -1) {
        float mediaMovel2 = aplicarFiltroMediaMovel(leitura2, leiturasSensor2);
        sensor2.umidadeAnterior = sensor2.umidadeAtual;
        sensor2.umidadeAtual = converterParaUmidade(mediaMovel2);
        sensor2.emErro = false;
        errosSensor2 = 0;
    } else {
        sensor2.emErro = true;
        errosSensor2++;
    }
    
    // Incrementa índice circular
    indiceAmostra = (indiceAmostra + 1) % NUM_AMOSTRAS;
    
    // Calcula média apenas se ambos sensores estão funcionando
    if(!sensor1.emErro && !sensor2.emErro) {
        umidadeMedia = (sensor1.umidadeAtual + sensor2.umidadeAtual) / 2.0;
    }
}

/**
 * Verifica condições de alerta baseadas na umidade
 */
void verificarAlertas() {
    // LED de status pisca durante operação normal
    digitalWrite(LED_STATUS, millis() % 1000 < 100);
    
    // Verifica alertas críticos
    bool alertaCritico = false;
    
    if(umidadeMedia < UMIDADE_CRITICA_BAIXA) {
        alertaCritico = true;
        Serial.println("⚠️  ALERTA: Solo muito seco! Irrigação necessária!");
    } else if(umidadeMedia > UMIDADE_CRITICA_ALTA) {
        alertaCritico = true;
        Serial.println("⚠️  ALERTA: Solo muito úmido! Risco de encharcamento!");
    }
    
    // Aciona LED de alerta se necessário
    digitalWrite(LED_ALERT, alertaCritico);
    
    // Verifica erros persistentes
    if(errosSensor1 > 5) {
        Serial.println("❌ ERRO: Sensor 1 com falha persistente!");
    }
    if(errosSensor2 > 5) {
        Serial.println("❌ ERRO: Sensor 2 com falha persistente!");
    }
}

// ==================== FUNÇÕES DE EXIBIÇÃO ====================
/**
 * Exibe dados formatados no Serial Monitor
 */
void exibirDados() {
    Serial.println("\n--- LEITURA DE SENSORES ---");
    Serial.print("Hora: ");
    Serial.print(millis() / 1000);
    Serial.println(" segundos");
    
    // Dados do Sensor 1
    Serial.print("📍 ");
    Serial.print(sensor1.nome);
    Serial.print(": ");
    if(!sensor1.emErro) {
        Serial.print(sensor1.umidadeAtual, 1);
        Serial.print("% ");
        
        // Mostra tendência
        if(sensor1.umidadeAtual > sensor1.umidadeAnterior + 1) {
            Serial.print("↗️");
        } else if(sensor1.umidadeAtual < sensor1.umidadeAnterior - 1) {
            Serial.print("↘️");
        } else {
            Serial.print("→");
        }
        
        // Mostra status
        if(sensor1.umidadeAtual < UMIDADE_CRITICA_BAIXA) {
            Serial.print(" [SECO]");
        } else if(sensor1.umidadeAtual > UMIDADE_CRITICA_ALTA) {
            Serial.print(" [ENCHARCADO]");
        } else if(sensor1.umidadeAtual >= UMIDADE_IDEAL_MIN && 
                  sensor1.umidadeAtual <= UMIDADE_IDEAL_MAX) {
            Serial.print(" [IDEAL]");
        }
    } else {
        Serial.print("ERRO!");
    }
    Serial.println();
    
    // Dados do Sensor 2
    Serial.print("📍 ");
    Serial.print(sensor2.nome);
    Serial.print(": ");
    if(!sensor2.emErro) {
        Serial.print(sensor2.umidadeAtual, 1);
        Serial.print("% ");
        
        // Mostra tendência
        if(sensor2.umidadeAtual > sensor2.umidadeAnterior + 1) {
            Serial.print("↗️");
        } else if(sensor2.umidadeAtual < sensor2.umidadeAnterior - 1) {
            Serial.print("↘️");
        } else {
            Serial.print("→");
        }
        
        // Mostra status
        if(sensor2.umidadeAtual < UMIDADE_CRITICA_BAIXA) {
            Serial.print(" [SECO]");
        } else if(sensor2.umidadeAtual > UMIDADE_CRITICA_ALTA) {
            Serial.print(" [ENCHARCADO]");
        } else if(sensor2.umidadeAtual >= UMIDADE_IDEAL_MIN && 
                  sensor2.umidadeAtual <= UMIDADE_IDEAL_MAX) {
            Serial.print(" [IDEAL]");
        }
    } else {
        Serial.print("ERRO!");
    }
    Serial.println();
    
    // Média dos sensores
    Serial.print("📊 MÉDIA: ");
    if(!sensor1.emErro && !sensor2.emErro) {
        Serial.print(umidadeMedia, 1);
        Serial.print("% ");
        
        // Barra de progresso visual
        Serial.print("[");
        int barras = map(umidadeMedia, 0, 100, 0, 20);
        for(int i = 0; i < 20; i++) {
            if(i < barras) Serial.print("█");
            else Serial.print("░");
        }
        Serial.print("]");
        
        // Diferença entre sensores
        float diferenca = abs(sensor1.umidadeAtual - sensor2.umidadeAtual);
        if(diferenca > 10) {
            Serial.print(" ⚠️  Diferença alta: ");
            Serial.print(diferenca, 1);
            Serial.print("%");
        }
    } else {
        Serial.print("Não disponível (erro em sensor)");
    }
    Serial.println();
    
    // Linha separadora
    Serial.println("----------------------------");
}

/**
 * Exibe dados em formato JSON (útil para integração)
 */
void exibirJSON() {
    Serial.print("{");
    Serial.print(""timestamp":");
    Serial.print(millis());
    Serial.print(","sensor1":{");
    Serial.print(""umidade":");
    Serial.print(sensor1.emErro ? "null" : String(sensor1.umidadeAtual));
    Serial.print(","erro":");
    Serial.print(sensor1.emErro ? "true" : "false");
    Serial.print("},"sensor2":{");
    Serial.print(""umidade":");
    Serial.print(sensor2.emErro ? "null" : String(sensor2.umidadeAtual));
    Serial.print(","erro":");
    Serial.print(sensor2.emErro ? "true" : "false");
    Serial.print("},"media":");
    Serial.print((!sensor1.emErro && !sensor2.emErro) ? String(umidadeMedia) : "null");
    Serial.println("}");
}

// ==================== LOOP PRINCIPAL ====================
/**
 * Loop principal do programa
 */
void loop() {
    // Verifica se é hora de fazer nova leitura
    if(millis() - ultimaLeitura >= INTERVALO_LEITURA) {
        ultimaLeitura = millis();
        
        // Processa leituras
        processarLeituras();
        
        // Verifica alertas
        verificarAlertas();
        
        // Exibe dados
        exibirDados();
        
        // Descomente a linha abaixo para saída em JSON
        // exibirJSON();
    }
    
    // Permite que outras tarefas do ESP32 sejam executadas
    yield();
}

// ==================== FUNÇÕES AUXILIARES ====================
/**
 * Calibra os sensores (executar quando solicitado via Serial)
 */
void calibrarSensores() {
    Serial.println("\n🔧 MODO DE CALIBRAÇÃO");
    Serial.println("1. Coloque ambos sensores em solo SECO");
    Serial.println("2. Aguarde 10 segundos...");
    
    delay(10000);
    
    float valorSeco1 = lerSensorComMedia(SENSOR1_PIN, 20);
    float valorSeco2 = lerSensorComMedia(SENSOR2_PIN, 20);
    
    Serial.print("Valores secos - Sensor 1: ");
    Serial.print(valorSeco1);
    Serial.print(" | Sensor 2: ");
    Serial.println(valorSeco2);
    
    Serial.println("\n3. Coloque ambos sensores em solo MOLHADO");
    Serial.println("4. Aguarde 10 segundos...");
    
    delay(10000);
    
    float valorMolhado1 = lerSensorComMedia(SENSOR1_PIN, 20);
    float valorMolhado2 = lerSensorComMedia(SENSOR2_PIN, 20);
    
    Serial.print("Valores molhados - Sensor 1: ");
    Serial.print(valorMolhado1);
    Serial.print(" | Sensor 2: ");
    Serial.println(valorMolhado2);
    
    Serial.println("\n✅ Calibração concluída!");
    Serial.println("Atualize as constantes VALOR_SECO e VALOR_MOLHADO com a média dos valores");
}

/**
 * Função para testar os sensores individualmente
 */
void testarSensores() {
    Serial.println("\n🧪 TESTE DE SENSORES");
    
    // Testa Sensor 1
    Serial.print("Testando Sensor 1... ");
    int teste1 = analogRead(SENSOR1_PIN);
    if(teste1 > VALOR_MIN_VALIDO && teste1 < VALOR_MAX_VALIDO) {
        Serial.print("OK (");
        Serial.print(teste1);
        Serial.println(")");
    } else {
        Serial.println("FALHA!");
    }
    
    // Testa Sensor 2
    Serial.print("Testando Sensor 2... ");
    int teste2 = analogRead(SENSOR2_PIN);
    if(teste2 > VALOR_MIN_VALIDO && teste2 < VALOR_MAX_VALIDO) {
        Serial.print("OK (");
        Serial.print(teste2);
        Serial.println(")");
    } else {
        Serial.println("FALHA!");
    }
    
    Serial.println("Teste concluído!");
}