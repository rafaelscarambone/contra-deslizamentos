/**
 * Monitoramento de Dois Sensores de Umidade do Solo no ESP32
 * Sensor exemplo: YL-69/HL-69 (analógico) ou similar
 * Autor: AI Assistant - Junho/2024
 * Compatível: Arduino IDE 1.8.8+
 */

#define SENSOR1_PIN 34   // Pino analógico do ESP32 (ex: GPIO34 -> VP)
#define SENSOR2_PIN 35   // Pino analógico do ESP32 (ex: GPIO35 -> VN)
#define NUM_LEITURAS 10  // Quantas leituras por sensor para filtragem de ruído
#define INTERVALO_LEITURA_MS 1000  // Intervalo entre leituras (em milissegundos)

// Defina o valor máximo e mínimo esperado para o seu sensor (dependendo do solo e sensor, ajuste se necessário)
#define UMI_MIN 0        // Valor mínimo que o sensor pode ler (100% úmido)
#define UMI_MAX 4095     // Valor máximo que o sensor pode ler (0% úmido) --> para ESP32 ADC de 12 bits

unsigned long ultimoMillis = 0;  // Armazena quando foi feita a última leitura

/**
 * Função para ler o sensor analógico com média de múltiplas leituras,
 * para maior estabilidade e filtragem de ruídos.
 */
int lerSensorMedia(uint8_t pino) {
  long soma = 0;
  for (int i = 0; i < NUM_LEITURAS; i++) {
    soma += analogRead(pino);
    delay(10); // Pequeno delay para melhor estabilidade entre leituras
  }
  return soma / NUM_LEITURAS;
}

/**
 * Função para validar se a leitura está dentro do intervalo esperado.
 * Retorna true caso leitura esteja OK.
 */
bool leituraValida(int leitura) {
  return (leitura >= UMI_MIN) && (leitura <= UMI_MAX);
}

/**
 * Função para converter valor analógico em percentual de umidade do solo.
 * Retorna valor entre 0 (seco) e 100 (molhado).
 */
int valorParaPercentual(int leitura) {
  if (leitura > UMI_MAX) leitura = UMI_MAX;
  if (leitura < UMI_MIN) leitura = UMI_MIN;
  // O sensor geralmente retorna valores ALTOS para solo seco e BAIXOS para molhado
  return map(leitura, UMI_MAX, UMI_MIN, 0, 100);
}

void setup() {
  Serial.begin(115200); // Inicializa a porta serial
  delay(1000);
  Serial.println("===== Monitoramento de Dois Sensores de Umidade - ESP32 =====");
  
  // Inicializa ADC para pinos analógicos no ESP32 (opcional, depende da placa)
  analogReadResolution(12); // Garante resolução de 12 bits (0-4095)
  analogSetAttenuation(ADC_11db); // Ajusta atenuação p/ melhor faixa de leitura
}

void loop() {
  unsigned long agora = millis();

  // Executa ciclo de leitura a cada INTERVALO_LEITURA_MS milissegundos
  if (agora - ultimoMillis >= INTERVALO_LEITURA_MS) {
    ultimoMillis = agora;

    // Lê cada sensor com média de NUM_LEITURAS
    int leituraS1 = lerSensorMedia(SENSOR1_PIN);
    int leituraS2 = lerSensorMedia(SENSOR2_PIN);

    // Valida as leituras
    bool validoS1 = leituraValida(leituraS1);
    bool validoS2 = leituraValida(leituraS2);

    // Converte para percentual (0 a 100%)
    int percS1 = valorParaPercentual(leituraS1);
    int percS2 = valorParaPercentual(leituraS2);

    // Calcula média entre os dois sensores (seguro: só se ambos válidos)
    int mediaPerc = (validoS1 && validoS2) ? (percS1 + percS2) / 2 : -1;

    // Exibe informações detalhadas no Serial Monitor
    Serial.println("------------------------------------------------------------");
    Serial.print("Sensor 1 [Raw]: "); Serial.print(leituraS1);
    Serial.print(" | Percentual: ");
    if (validoS1) Serial.print(percS1); else Serial.print("ERRO");

    Serial.print(" % | Sensor 2 [Raw]: "); Serial.print(leituraS2);
    Serial.print(" | Percentual: ");
    if (validoS2) Serial.print(percS2); else Serial.print("ERRO");

    Serial.println(" %");

    if (validoS1 && validoS2) {
      Serial.print("MÉDIA dos dois sensores: ");
      Serial.print(mediaPerc);
      Serial.println(" % umidade");
    } else {
      Serial.println("[!] ERRO: Leitura inválida encontrada! Verifique sensores/conexões.");
    }
    Serial.println("------------------------------------------------------------");
  }
}
