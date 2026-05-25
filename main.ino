#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <Ticker.h>

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#define IR_RECV_PIN 15
#define IR_SEND_PIN 4
#define BUZZER_PIN 27
#define LED_PLACA 2

IRrecv irrecv(IR_RECV_PIN);
IRsend irsend(IR_SEND_PIN);

decode_results results;

WebServer server(80);
Preferences prefs;
WiFiManager wm;
Ticker piscaLed;

bool modoCaptura = false;
bool estadoLed = false;

String ultimoCodigoRaw = "";
String teclasSalvas = "";
String scene = "";

void alternarLed() {
  estadoLed = !estadoLed;
  digitalWrite(LED_PLACA, estadoLed);
}

void beepSucesso() {
  ledcAttach(BUZZER_PIN, 2000, 8);

  ledcWriteTone(BUZZER_PIN, 2000);

  delay(150);

  ledcWriteTone(BUZZER_PIN, 0);
}

void beepExecucaoCena() {
  ledcAttach(BUZZER_PIN, 2000, 8);

  ledcWriteTone(BUZZER_PIN, 2600);

  delay(80);

  ledcWriteTone(BUZZER_PIN, 0);
}

void beepOkWifi() {
  ledcAttach(BUZZER_PIN, 2000, 8);

  ledcWriteTone(BUZZER_PIN, 1800);
  delay(120);

  ledcWriteTone(BUZZER_PIN, 0);
  delay(80);

  ledcWriteTone(BUZZER_PIN, 2500);
  delay(180);

  ledcWriteTone(BUZZER_PIN, 0);
}

bool teclaJaExiste(String nome) {
  teclasSalvas = prefs.getString("teclas", "");

  String busca = "|" + nome + "|";

  return teclasSalvas.indexOf(busca) >= 0;
}

void adicionarTeclaNaLista(String nome) {
  teclasSalvas = prefs.getString("teclas", "");

  if (!teclaJaExiste(nome)) {
    teclasSalvas += "|" + nome + "|";

    prefs.putString("teclas", teclasSalvas);
  }
}

String getCodigoTecla(String nome) {
  return prefs.getString(("key_" + nome).c_str(), "");
}

void conectarWiFi() {
  WiFi.mode(WIFI_STA);

  Serial.println("");
  Serial.println("================================");
  Serial.println("Iniciando WiFiManager...");
  Serial.println("Rede AP: ESP32-Controle-IR");
  Serial.println("Acesse: 192.168.4.1");
  Serial.println("================================");

  piscaLed.attach(0.5, alternarLed);

  bool conectado = wm.autoConnect("ESP32-Controle-IR");

  piscaLed.detach();

  if (!conectado) {
    Serial.println("Falha WiFi. Reiniciando...");

    digitalWrite(LED_PLACA, LOW);

    delay(2000);

    ESP.restart();
  }

  digitalWrite(LED_PLACA, HIGH);

  Serial.println("");
  Serial.println("================================");
  Serial.println("WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("================================");

  beepOkWifi();
}

String rawToString(decode_results *results) {
  String data = "";

  for (uint16_t i = 1; i < results->rawlen; i++) {
    uint32_t duration = results->rawbuf[i] * kRawTick;

    data += String(duration);

    if (i < results->rawlen - 1) {
      data += ",";
    }
  }

  return data;
}

std::vector<uint16_t> stringToRaw(String data) {
  std::vector<uint16_t> raw;

  int start = 0;

  while (true) {
    int comma = data.indexOf(',', start);

    if (comma == -1) {
      String value = data.substring(start);

      if (value.length() > 0) {
        raw.push_back(value.toInt());
      }

      break;
    }

    String value = data.substring(start, comma);

    raw.push_back(value.toInt());

    start = comma + 1;
  }

  return raw;
}

void enviarCodigo(String nome) {
  String codigo = getCodigoTecla(nome);

  if (codigo == "") {
    Serial.println("Código não encontrado: " + nome);
    return;
  }

  std::vector<uint16_t> raw = stringToRaw(codigo);

  if (raw.size() > 0) {
    irsend.sendRaw(raw.data(), raw.size(), 38);

    Serial.println("Enviado: " + nome);

    beepExecucaoCena();
  }
}

void executarCena() {
  scene = prefs.getString("scene", "");

  int start = 0;

  while (true) {
    int sep = scene.indexOf('|', start);

    String nome;

    if (sep == -1) {
      nome = scene.substring(start);
    } else {
      nome = scene.substring(start, sep);
    }

    nome.trim();

    if (nome.length() > 0) {
      Serial.println("Executando tecla: " + nome);

      enviarCodigo(nome);

      delay(1000);
    }

    if (sep == -1) break;

    start = sep + 1;
  }
}

String botoesTeclasHtml() {
  teclasSalvas = prefs.getString("teclas", "");

  String html = "";

  int start = 0;

  while (true) {
    int ini = teclasSalvas.indexOf('|', start);

    if (ini == -1) break;

    int fim = teclasSalvas.indexOf('|', ini + 1);

    if (fim == -1) break;

    String nome = teclasSalvas.substring(ini + 1, fim);

    nome.trim();

    if (nome.length() > 0) {
      html += "<button class='teclaBtn' onclick=\"mostrarCodigo('";
      html += nome;
      html += "')\">";
      html += nome;
      html += "</button> ";

      html += "<button onclick=\"enviarTecla('";
      html += nome;
      html += "')\">Enviar</button><br><br>";
    }

    start = fim + 1;
  }

  if (html == "") {
    html = "<p>Nenhuma tecla salva ainda.</p>";
  }

  return html;
}

String opcoesTeclasHtml() {
  teclasSalvas = prefs.getString("teclas", "");

  String html = "";

  int start = 0;

  while (true) {
    int ini = teclasSalvas.indexOf('|', start);

    if (ini == -1) break;

    int fim = teclasSalvas.indexOf('|', ini + 1);

    if (fim == -1) break;

    String nome = teclasSalvas.substring(ini + 1, fim);

    nome.trim();

    if (nome.length() > 0) {
      html += "<button onclick=\"adicionarNaCena('";
      html += nome;
      html += "')\">";
      html += nome;
      html += "</button> ";
    }

    start = fim + 1;
  }

  return html;
}

String htmlPage() {
  String html = "";

  scene = prefs.getString("scene", "");

  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Controle IR ESP32</title>";

  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#f4f4f4;}";
  html += ".card{background:white;padding:15px;margin-bottom:15px;border-radius:8px;box-shadow:0 2px 5px #ccc;}";
  html += "button{padding:10px 14px;margin:4px;border:0;border-radius:6px;background:#1976d2;color:white;font-size:15px;}";
  html += "button:hover{background:#0d47a1;cursor:pointer;}";
  html += ".danger{background:#c62828;}";
  html += ".success{background:#2e7d32;}";
  html += ".teclaBtn{background:#6a1b9a;}";
  html += ".itemCena{padding:10px;margin:6px;background:#e3f2fd;border:1px solid #90caf9;border-radius:6px;cursor:move;}";
  html += "textarea{width:100%;box-sizing:border-box;}";
  html += "input{padding:10px;width:70%;max-width:300px;}";
  html += "</style>";

  html += "</head>";
  html += "<body>";

  html += "<h2>Controle IR ESP32</h2>";

  html += "<div class='card'>";
  html += "<p><b>IP do ESP32:</b> ";
  html += WiFi.localIP().toString();
  html += "</p>";
  html += "</div>";

  html += "<div class='card'>";

  html += "<h3>1. Capturar tecla</h3>";

  if (modoCaptura) {
    html += "<p><b>Status:</b> Aguardando tecla do controle...</p>";
  } else {
    html += "<p><b>Status:</b> Parado</p>";
  }

  html += "<form action='/capturar'>";
  html += "<button type='submit'>Iniciar captura da tecla</button>";
  html += "</form>";

  html += "<p><b>Último código capturado:</b></p>";

  html += "<textarea rows='5'>";
  html += ultimoCodigoRaw;
  html += "</textarea>";

  html += "<h3>Salvar tecla capturada</h3>";

  html += "<form action='/salvar'>";
  html += "<input name='nome' placeholder='Ex: power, canal_5'>";
  html += "<button class='success' type='submit'>Salvar</button>";
  html += "</form>";

  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>2. Teclas salvas</h3>";
  html += "<p>Clique no botão para ver o código da tecla.</p>";

  html += botoesTeclasHtml();

  html += "<p><b>Código da tecla selecionada:</b></p>";

  html += "<textarea id='codigoSelecionado' rows='6'></textarea>";

  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>3. Configurar cena</h3>";

  html += "<p>Clique nas teclas para adicionar na cena. Arraste para ordenar.</p>";

  html += opcoesTeclasHtml();

  html += "<h4>Ordem da cena</h4>";

  html += "<div id='listaCena'></div>";

  html += "<br>";

  html += "<button class='success' onclick='salvarCena()'>Salvar cena</button>";

  html += "<button class='danger' onclick='limparCena()'>Limpar cena</button>";

  html += "<p><b>Cena salva:</b></p>";

  html += "<pre id='sceneAtual'>";
  html += scene;
  html += "</pre>";

  html += "</div>";

  html += "<div class='card'>";

  html += "<h3>4. Executar</h3>";

  html += "<form action='/ligar-tv'>";

  html += "<button class='success' type='submit'>";
  html += "Executar cena / Ligar TV";
  html += "</button>";

  html += "</form>";

  html += "</div>";

  html += "<div class='card'>";
  html += "<a href='/reset-wifi'>Resetar WiFi</a>";
  html += "</div>";

  html += "<script>";

  html += "let cena = [];";

  html += "function carregarCenaSalva(){";
  html += "let salva='";
  html += scene;
  html += "';";
  html += "if(salva.length>0){";
  html += "cena=salva.split('|').filter(x=>x.trim().length>0);";
  html += "renderizarCena();";
  html += "}";
  html += "}";

  html += "function mostrarCodigo(nome){";
  html += "fetch('/codigo?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>{";
  html += "document.getElementById('codigoSelecionado').value=t;";
  html += "});";
  html += "}";

  html += "function enviarTecla(nome){";
  html += "fetch('/enviar?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>alert(t));";
  html += "}";

  html += "function adicionarNaCena(nome){";
  html += "cena.push(nome);";
  html += "renderizarCena();";
  html += "}";

  html += "function renderizarCena(){";
  html += "let div=document.getElementById('listaCena');";
  html += "div.innerHTML='';";

  html += "cena.forEach((nome,index)=>{";

  html += "let item=document.createElement('div');";

  html += "item.className='itemCena';";

  html += "item.draggable=true;";

  html += "item.dataset.index=index;";

  html += "item.innerHTML=(index+1)+' - '+nome+";
  html += "\" <button onclick='removerItem(\"+index+\")'>Remover</button>\";";

  html += "item.addEventListener('dragstart',dragStart);";
  html += "item.addEventListener('dragover',dragOver);";
  html += "item.addEventListener('drop',dropItem);";

  html += "div.appendChild(item);";

  html += "});";
  html += "}";

  html += "let dragIndex=null;";

  html += "function dragStart(e){";
  html += "dragIndex=Number(e.currentTarget.dataset.index);";
  html += "}";

  html += "function dragOver(e){";
  html += "e.preventDefault();";
  html += "}";

  html += "function dropItem(e){";
  html += "e.preventDefault();";

  html += "let dropIndex=Number(e.currentTarget.dataset.index);";

  html += "let item=cena.splice(dragIndex,1)[0];";

  html += "cena.splice(dropIndex,0,item);";

  html += "renderizarCena();";
  html += "}";

  html += "function removerItem(index){";
  html += "cena.splice(index,1);";
  html += "renderizarCena();";
  html += "}";

  html += "function limparCena(){";
  html += "cena=[];";
  html += "renderizarCena();";
  html += "}";

  html += "function salvarCena(){";

  html += "let valor=cena.join('|');";

  html += "fetch('/salvar-cena?scene='+encodeURIComponent(valor))";

  html += ".then(r=>r.text())";

  html += ".then(t=>{";
  html += "alert(t);";
  html += "location.reload();";
  html += "});";
  html += "}";

  html += "carregarCenaSalva();";

  html += "</script>";

  html += "</body>";
  html += "</html>";

  return html;
}

void setupRotas() {
  server.on("/", []() {
    teclasSalvas = prefs.getString("teclas", "");
    scene = prefs.getString("scene", "");

    server.send(200, "text/html", htmlPage());
  });

  server.on("/capturar", []() {
    modoCaptura = true;

    Serial.println("Modo captura ativado");

    server.sendHeader("Location", "/");

    server.send(302, "text/plain", "");
  });

  server.on("/salvar", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome da tecla");
      return;
    }

    String nome = server.arg("nome");

    nome.trim();

    nome.replace(" ", "_");

    if (nome == "") {
      server.send(400, "text/plain", "Nome inválido");
      return;
    }

    if (ultimoCodigoRaw == "") {
      server.send(400, "text/plain", "Nenhum código capturado ainda");
      return;
    }

    prefs.putString(("key_" + nome).c_str(), ultimoCodigoRaw);

    adicionarTeclaNaLista(nome);

    Serial.println("Tecla salva: " + nome);

    beepSucesso();

    server.sendHeader("Location", "/");

    server.send(302, "text/plain", "");
  });

  server.on("/codigo", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome");
      return;
    }

    String nome = server.arg("nome");

    String codigo = getCodigoTecla(nome);

    server.send(200, "text/plain", codigo);
  });

  server.on("/enviar", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome");
      return;
    }

    String nome = server.arg("nome");

    enviarCodigo(nome);

    server.send(200, "text/plain", "Enviado: " + nome);
  });

  server.on("/salvar-cena", []() {
    if (!server.hasArg("scene")) {
      server.send(400, "text/plain", "Cena inválida");
      return;
    }

    scene = server.arg("scene");

    prefs.putString("scene", scene);

    Serial.println("Cena salva:");
    Serial.println(scene);

    beepSucesso();

    server.send(200, "text/plain", "Cena salva com sucesso");
  });

  server.on("/ligar-tv", []() {
    Serial.println("Executando cena...");

    executarCena();

    server.send(200, "text/plain", "Cena executada");
  });

  server.on("/reset-wifi", []() {
    wm.resetSettings();

    digitalWrite(LED_PLACA, LOW);

    server.send(200, "text/plain", "WiFi apagado. Reinicie o ESP32.");
  });

  server.begin();

  Serial.println("Servidor WEB iniciado");
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PLACA, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PLACA, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  prefs.begin("controle-ir", false);

  teclasSalvas = prefs.getString("teclas", "");
  scene = prefs.getString("scene", "");

  irrecv.enableIRIn();

  irsend.begin();

  conectarWiFi();

  setupRotas();
}

void loop() {
  server.handleClient();

  if (modoCaptura && irrecv.decode(&results)) {
    ultimoCodigoRaw = rawToString(&results);

    Serial.println("");
    Serial.println("========== IR RECEBIDO ==========");
    Serial.println(ultimoCodigoRaw);
    Serial.println("================================");

    beepSucesso();

    modoCaptura = false;

    irrecv.resume();
  }
}
