# 🚀 GUIA DE INÍCIO RÁPIDO - PZEM v2.0

## ✅ VOCÊ RECEBEU:

1. **esp32_pzem_v2.ino** - Código completo e corrigido
2. **app_v2.html** - App web funcional
3. **Este guia** - Instruções simples

---

## 📋 PASSO 1: Preparar Arduino IDE

### Instalar Biblioteca PZEM:

1. Abra Arduino IDE
2. **Ferramentas > Gerenciar Bibliotecas**
3. Procure: `PZEM004T`
4. Clique em **Instalar**
5. Espere terminar

### Instalar Biblioteca JSON:

1. **Ferramentas > Gerenciar Bibliotecas**
2. Procure: `ArduinoJson`
3. Clique em **Instalar**

---

## 🔌 PASSO 2: Conectar Hardware

### Conexão PZEM ↔ ESP32:

```
PZEM Pino 2 (TX)   →   ESP32 GPIO 16 (RX2)
PZEM Pino 3 (RX)   →   ESP32 GPIO 17 (TX2)
PZEM Pino 4 (GND)  →   ESP32 GND
PZEM Pino 1 (VCC)  →   ESP32 5V (ou 3.3V)

Relé:
Relé IN    →   ESP32 GPIO 5
Relé VCC   →   5V
Relé GND   →   GND
```

**IMPORTANTE:** GND DEVE SER COMUM ENTRE TUDO!

---

## 💻 PASSO 3: Carregar o Código

1. Abra Arduino IDE
2. **Arquivo > Abrir**
3. Selecione: `esp32_pzem_v2.ino`
4. **Edite linhas 15-16:**
   ```cpp
   const char* ssid = "SUA_REDE_WIFI";
   const char* password = "SUA_SENHA_WIFI";
   ```
5. **Ferramentas > Placa > ESP32 Dev Module**
6. **Ferramentas > Porta > COM*** (sua porta)
7. **Sketch > Carregar** (ou Ctrl+U)
8. Aguarde até terminar

---

## 🧪 PASSO 4: Verificar no Monitor Serial

1. **Ferramentas > Monitor Serial**
2. Ajuste para **115200 baud**
3. Você deve ver:

```
╔════════════════════════════════════════╗
║   PROTETOR DE CORRENTE COM PZEM v2.0   ║
╚════════════════════════════════════════╝

📡 Conectando WiFi...
✅ WiFi conectado!
🌐 IP: 192.168.15.7

🔍 Testando comunicação com PZEM...
✅ PZEM conectado! Tensão: 220.5V

✅ Servidor web iniciado
✅ Inicialização completa!
```

**IMPORTANTE:** Se vir "❌ PZEM não respondeu", verificar conexão dos fios!

---

## 📱 PASSO 5: Usar o App

### Opção A: Abrir arquivo HTML direto

1. Copie `app_v2.html` para seu celular
2. Abra com o navegador (Chrome, Firefox, etc)
3. No campo de IP, insira o IP do ESP32 (ex: `192.168.15.7`)
4. Clique em **Conectar**

### Opção B: Servir via HTTP (melhor)

Se estiver no PC:

```bash
# Python 3 (na pasta do arquivo)
python -m http.server 8000

# Depois abra no navegador:
http://localhost:8000/app_v2.html
```

No celular conectado à mesma rede:
```
http://[IP_do_PC]:8000/app_v2.html
```

---

## ⚡ PRONTO! Você tem:

✅ Dashboard em tempo real
✅ Leitura de: Tensão, Corrente, Potência, Energia, Frequência
✅ Proteção automática contra:
   - Picos de tensão (>250V)
   - Quedas de tensão (<180V)
   - Sobrecorrente (>25A)
✅ Controle manual do relé
✅ Configurações ajustáveis
✅ Histórico de eventos

---

## 🎯 PRÓXIMAS ETAPAS:

1. **Testar proteção:**
   - Ligar um aparelho que consome corrente
   - Ver a corrente aumentar no app
   - Testar limite (ajustar se necessário)

2. **Ajustar limites:**
   - Clique em ⚙️ **Configurações**
   - Ajuste conforme seus aparelhos
   - Clique em **Salvar**

3. **Monitorar:**
   - Deixe o app aberto
   - Veja qual aparelho consome mais
   - Acompanhe o histórico de eventos

---

## ❌ SE NÃO CONECTAR:

### Verificar:
```
[ ] PZEM tem LED piscando?
[ ] Todos os fios bem conectados?
[ ] GND é comum?
[ ] WiFi está 2.4GHz? (ESP32 não suporta 5GHz)
[ ] IP está correto no app?
[ ] Monitor Serial mostra "✅ PZEM conectado"?
```

### Se ainda não funcionar:
```
[ ] Desligue tudo
[ ] Verifique cada fio
[ ] Religue e teste novamente
[ ] Veja o Monitor Serial para erros
```

---

## 📊 VALORES PADRÃO:

| Parâmetro | Valor | Para quê |
|-----------|-------|----------|
| Tensão Máxima | 250V | Proteção contra picos |
| Tensão Mínima | 180V | Proteção contra quedas |
| Corrente Máxima | 25A | Proteção contra sobrecarga |
| Tempo de Espera | 10s | Aguardar rede normalizar |

**Ajuste conforme sua necessidade!**

---

## 🔧 CONFIGURAÇÕES RECOMENDADAS:

### Para Casa (uso normal):
```
Tensão Máxima: 245V
Tensão Mínima: 190V
Corrente Máxima: 25A
Tempo de Espera: 10s
```

### Para Equipamentos Sensíveis:
```
Tensão Máxima: 240V
Tensão Mínima: 200V
Corrente Máxima: 20A
Tempo de Espera: 5s
```

---

## 📝 CHECKLIST FINAL:

```
Hardware:
[ ] PZEM conectado em GPIO 16/17
[ ] Relé conectado em GPIO 5
[ ] Alimentação 5V ou 3.3V
[ ] GND comum

Software:
[ ] Biblioteca PZEM instalada
[ ] Código carregado
[ ] Monitor Serial mostra "PZEM conectado"

App:
[ ] Acessível no navegador
[ ] Mostra dados em tempo real
[ ] Botões funcionam
```

---

## 🎉 PRONTO!

Você tem um sistema profissional de proteção! 

**Próximo passo:** Conectar à sua rede 220V (com segurança!)

---

**Qualquer dúvida, verifique o Monitor Serial - ele mostra exatamente o que está acontecendo!** 🔍
