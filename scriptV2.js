// Variáveis globais para conexão serial
let port = null;
let reader = null;
let writer = null;
let encoder = new TextEncoder();
let decoder = new TextDecoder();
let keepReading = true;

// Elementos DOM
const connectBtn = document.getElementById('connectBtn');
const testBtn = document.getElementById('testBtn');
const resetBtn = document.getElementById('resetBtn');
const connectionStatus = document.getElementById('connectionStatus');
const consoleOutput = document.getElementById('consoleOutput');
const commandInput = document.getElementById('commandInput');

// Event listeners
connectBtn.addEventListener('click', toggleConnection);
testBtn.addEventListener('click', () => enviarComando('TEST'));
resetBtn.addEventListener('click', () => enviarComando('RESET'));

// Função principal de conexão/desconexão
async function toggleConnection() {
    if (port) {
        await desconectar();
    } else {
        await conectar();
    }
}

// Função para conectar ao ESP32
async function conectar() {
    try {
        // Verifica se o navegador suporta Web Serial API
        if (!('serial' in navigator)) {
            alert('Seu navegador não suporta Web Serial API. Use Chrome 89+ ou Edge 89+');
            return;
        }

        // Solicita porta serial ao usuário
        port = await navigator.serial.requestPort();
        
        // Abre a porta com configurações para ESP32
        await port.open({ 
            baudRate: 115200,
            dataBits: 8,
            stopBits: 1,
            parity: "none",
            flowControl: "none"
        });

        // Configura leitor e escritor
        const textDecoder = new TextDecoderStream();
        const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
        reader = textDecoder.readable.getReader();

        const textEncoder = new TextEncoderStream();
        const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
        writer = textEncoder.writable.getWriter();

        // Atualiza interface
        connectBtn.innerHTML = '<i class="fas fa-unlink"></i> Desconectar';
        connectionStatus.innerHTML = '<i class="fas fa-circle"></i> Conectado';
        connectionStatus.classList.add('connected');
        
        // Habilita botões
        testBtn.disabled = false;
        resetBtn.disabled = false;
        commandInput.disabled = false;
        document.querySelectorAll('.irrigation-control button').forEach(btn => btn.disabled = false);

        // Log no console
        logConsole('✅ Conectado ao ESP32', 'success');

        // Inicia leitura contínua
        keepReading = true;
        lerDadosSerial();

    } catch (error) {
        console.error('Erro ao conectar:', error);
        logConsole(`❌ Erro ao conectar: ${error.message}`, 'error');
    }
}

// Função para desconectar
async function desconectar() {
    try {
        keepReading = false;
        
        if (reader) {
            await reader.cancel();
            reader.releaseLock();
        }
        
        if (writer) {
            await writer.close();
        }
        
        if (port) {
            await port.close();
            port = null;
        }

        // Atualiza interface
        connectBtn.innerHTML = '<i class="fas fa-plug"></i> Conectar ESP32';
        connectionStatus.innerHTML = '<i class="fas fa-circle"></i> Desconectado';
        connectionStatus.classList.remove('connected');
        
        // Desabilita botões
        testBtn.disabled = true;
        resetBtn.disabled = true;
        commandInput.disabled = true;
        document.querySelectorAll('.irrigation-control button').forEach(btn => btn.disabled = true);

        // Limpa valores dos sensores
        document.getElementById('sensor1Value').textContent = '--';
        document.getElementById('sensor2Value').textContent = '--';
        document.getElementById('sensor1Status').textContent = 'Desconectado';
        document.getElementById('sensor2Status').textContent = 'Desconectado';
        document.getElementById('sensor1Progress').style.width = '0%';
        document.getElementById('sensor2Progress').style.width = '0%';

        logConsole('🔌 Desconectado do ESP32', 'info');

    } catch (error) {
        console.error('Erro ao desconectar:', error);
        logConsole(`❌ Erro ao desconectar: ${error.message}`, 'error');
    }
}

// Função para ler dados da porta serial
async function lerDadosSerial() {
    while (port && keepReading) {
        try {
            const { value, done } = await reader.read();
            
            if (done) {
                reader.releaseLock();
                break;
            }
            
            if (value) {
                processarDados(value);
            }
        } catch (error) {
            console.error('Erro na leitura:', error);
            logConsole(`❌ Erro na leitura: ${error.message}`, 'error');
            break;
        }
    }
}

// Função para processar dados recebidos
function processarDados(data) {
    const lines = data.trim().split('\n');
    
    lines.forEach(line => {
        line = line.trim();
        
        // Log de dados brutos (comentado para produção)
        // console.log('Dados recebidos:', line);
        
        // Tenta processar como JSON
        if (line.startsWith('{') && line.endsWith('}')) {
            try {
                const json = JSON.parse(line);
                
                if (json.type === 'data') {
                    atualizarSensores(json);
                } else if (json.type === 'log') {
                    logConsole(`ESP32: ${json.message}`, json.level || 'info');
                } else if (json.type === 'config') {
                    atualizarConfiguracao(json);
                }
            } catch (e) {
                // Se não for JSON válido, exibe como log simples
                if (line.length > 0) {
                    logConsole(`ESP32: ${line}`, 'raw');
                }
            }
        } else if (line.length > 0) {
            // Dados não-JSON
            logConsole(`ESP32: ${line}`, 'raw');
        }
    });
}

// Função para atualizar valores dos sensores
function atualizarSensores(data) {
    // Atualiza Sensor 1
    if (data.sensor1 !== undefined) {
        const valor1 = data.sensor1;
        const porcentagem1 = data.humidity1 || 0;
        
        document.getElementById('sensor1Value').textContent = valor1;
        document.getElementById('sensor1Status').textContent = data.status1 || 'OK';
        document.getElementById('sensor1Progress').style.width = `${porcentagem1}%`;
        
        // Muda cor baseado no status
        const card1 = document.querySelector('.sensor-card:nth-child(1)');
        if (data.status1 === 'Muito Seco') {
            card1.style.borderLeft = '5px solid var(--danger-color)';
        } else if (data.status1 === 'Muito Úmido') {
            card1.style.borderLeft = '5px solid var(--secondary-color)';
        } else {
            card1.style.borderLeft = '5px solid var(--primary-color)';
        }
    }
    
    // Atualiza Sensor 2
    if (data.sensor2 !== undefined) {
        const valor2 = data.sensor2;
        const porcentagem2 = data.humidity2 || 0;
        
        document.getElementById('sensor2Value').textContent = valor2;
        document.getElementById('sensor2Status').textContent = data.status2 || 'OK';
        document.getElementById('sensor2Progress').style.width = `${porcentagem2}%`;
        
        // Muda cor baseado no status
        const card2 = document.querySelector('.sensor-card:nth-child(2)');
        if (data.status2 === 'Muito Seco') {
            card2.style.borderLeft = '5px solid var(--danger-color)';
        } else if (data.status2 === 'Muito Úmido') {
            card2.style.borderLeft = '5px solid var(--secondary-color)';
        } else {
            card2.style.borderLeft = '5px solid var(--primary-color)';
        }
    }
}

// Função para atualizar configuração
function atualizarConfiguracao(config) {
    if (config.limiteSecoAtual !== undefined) {
        document.getElementById('dryLimit').value = config.limiteSecoAtual;
    }
    if (config.limiteUmidoAtual !== undefined) {
        document.getElementById('wetLimit').value = config.limiteUmidoAtual;
    }
    if (config.intervaloAtual !== undefined) {
        document.getElementById('readInterval').value = config.intervaloAtual;
    }
}

// Função para enviar comandos
async function enviarComando(comando) {
    if (!writer) {
        logConsole('❌ Não conectado ao ESP32', 'error');
        return;
    }
    
    try {
        await writer.write(comando + '\n');
        logConsole(`📤 Comando enviado: ${comando}`, 'command');
    } catch (error) {
        console.error('Erro ao enviar comando:', error);
        logConsole(`❌ Erro ao enviar comando: ${error.message}`, 'error');
    }
}

// Função para irrigar planta
function irrigarPlanta(numeroPlanta, duracao) {
    const comando = `IRRIGAR:${numeroPlanta}:${duracao}`;
    enviarComando(comando);
    
    // Feedback visual
    const btn = event.target.closest('button');
    const originalText = btn.innerHTML;
    btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Irrigando...';
    btn.disabled = true;
    
    // Restaura botão após duração
    setTimeout(() => {
        btn.innerHTML = originalText;
        btn.disabled = false;
    }, duracao * 1000);
}

// Função para atualizar limites
function atualizarLimite(tipo) {
    let comando;
    if (tipo === 'seco') {
        const valor = document.getElementById('dryLimit').value;
        comando = `CALIBRAR:SECO:${valor}`;
    } else {
        const valor = document.getElementById('wetLimit').value;
        comando = `CALIBRAR:UMIDO:${valor}`;
    }
    enviarComando(comando);
}

// Função para atualizar intervalo
function atualizarIntervalo() {
    const valor = document.getElementById('readInterval').value;
    const comando = `INTERVALO:${valor}`;
    enviarComando(comando);
}

// Função para enviar comando customizado
function enviarComandoCustom() {
    const comando = commandInput.value.trim();
    if (comando) {
        enviarComando(comando);
        commandInput.value = '';
    }
}

// Função para log no console
function logConsole(mensagem, tipo = 'info') {
    const timestamp = new Date().toLocaleTimeString('pt-BR');
    const logEntry = document.createElement('div');
    
    // Define cor baseado no tipo
    let cor = '#bdc3c7';
    let prefixo = '';
    
    switch(tipo) {
        case 'success':
            cor = '#2ecc71';
            break;
        case 'error':
            cor = '#e74c3c';
            break;
        case 'command':
            cor = '#3498db';
            break;
        case 'warning':
            cor = '#f39c12';
            break;
        case 'raw':
            cor = '#95a5a6';
            break;
    }
    
    logEntry.style.color = cor;
    logEntry.textContent = `[${timestamp}] ${mensagem}`;
    
    consoleOutput.appendChild(logEntry);
    
    // Auto-scroll para última mensagem
    consoleOutput.scrollTop = consoleOutput.scrollHeight;
    
    // Limita número de mensagens no console (mantém últimas 100)
    while (consoleOutput.children.length > 100) {
        consoleOutput.removeChild(consoleOutput.firstChild);
    }
}

// Listener para Enter no input de comando
commandInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        enviarComandoCustom();
    }
});

// Verifica suporte ao Web Serial API ao carregar
window.addEventListener('load', () => {
    if (!('serial' in navigator)) {
        logConsole('⚠️ Web Serial API não suportada neste navegador', 'warning');
        logConsole('Use Chrome 89+ ou Edge 89+ para funcionalidade completa', 'warning');
        connectBtn.disabled = true;
    } else {
        logConsole('✅ Sistema iniciado - Pronto para conexão', 'success');
    }
});

// Desconecta ao fechar/recarregar página
window.addEventListener('beforeunload', async () => {
    if (port) {
        await desconectar();
    }
});