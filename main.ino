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
String cenasSalvas = "";

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

String limparNome(String nome) {
  nome.trim();
  nome.replace(" ", "_");
  nome.replace("|", "_");
  nome.replace(":", "_");
  nome.replace("/", "_");
  nome.replace("\\", "_");
  nome.replace("?", "_");
  nome.replace("&", "_");
  nome.replace("=", "_");
  return nome;
}

bool itemExisteNaLista(String lista, String nome) {
  String busca = "|" + nome + "|";
  return lista.indexOf(busca) >= 0;
}

bool teclaJaExiste(String nome) {
  teclasSalvas = prefs.getString("teclas", "");
  return itemExisteNaLista(teclasSalvas, nome);
}

bool cenaJaExiste(String nome) {
  cenasSalvas = prefs.getString("cenas", "");
  return itemExisteNaLista(cenasSalvas, nome);
}

void adicionarTeclaNaLista(String nome) {
  teclasSalvas = prefs.getString("teclas", "");

  if (!teclaJaExiste(nome)) {
    teclasSalvas += "|" + nome + "|";
    prefs.putString("teclas", teclasSalvas);
  }
}

void adicionarCenaNaLista(String nome) {
  cenasSalvas = prefs.getString("cenas", "");

  if (!cenaJaExiste(nome)) {
    cenasSalvas += "|" + nome + "|";
    prefs.putString("cenas", cenasSalvas);
  }
}

String removerItemDaLista(String lista, String nome) {
  String novaLista = "";

  int start = 0;

  while (true) {
    int ini = lista.indexOf('|', start);

    if (ini == -1) break;

    int fim = lista.indexOf('|', ini + 1);

    if (fim == -1) break;

    String item = lista.substring(ini + 1, fim);
    item.trim();

    if (item != nome && item.length() > 0) {
      novaLista += "|" + item + "|";
    }

    start = fim + 1;
  }

  return novaLista;
}

String getCodigoTecla(String nome) {
  return prefs.getString(("key_" + nome).c_str(), "");
}

String getCena(String nome) {
  return prefs.getString(("scene_" + nome).c_str(), "");
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

void executarCena(String nomeCena) {
  String scene = getCena(nomeCena);

  if (scene == "") {
    Serial.println("Cena não encontrada: " + nomeCena);
    return;
  }

  Serial.println("Executando cena: " + nomeCena);

  int start = 0;

  while (true) {
    int sep = scene.indexOf('|', start);

    String item;

    if (sep == -1) {
      item = scene.substring(start);
    } else {
      item = scene.substring(start, sep);
    }

    item.trim();

    if (item.length() > 0) {
      if (item.startsWith("delay:")) {
        String valorDelay = item.substring(6);
        int segundos = valorDelay.toInt();

        if (segundos > 0) {
          Serial.print("Aguardando delay de ");
          Serial.print(segundos);
          Serial.println(" segundos");

          delay(segundos * 1000);
        }

      } else {
        Serial.println("Executando tecla: " + item);
        enviarCodigo(item);
        delay(1000);
      }
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
      html += "<div style='display:flex;align-items:center;margin-bottom:10px;'>";

      html += "<button class='success' onclick=\"enviarTecla('";
      html += nome;
      html += "')\">Enviar</button>";

      html += "<button class='teclaBtn' style='margin-left:6px;' onclick=\"mostrarCodigo('";
      html += nome;
      html += "')\">";
      html += nome;
      html += "</button>";

      html += "<button class='danger' style='margin-left:6px;' onclick=\"excluirTecla('";
      html += nome;
      html += "')\">Excluir</button>";

      html += "</div>";
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

String cenasHtml() {
  cenasSalvas = prefs.getString("cenas", "");

  String html = "";

  int start = 0;

  while (true) {
    int ini = cenasSalvas.indexOf('|', start);

    if (ini == -1) break;

    int fim = cenasSalvas.indexOf('|', ini + 1);

    if (fim == -1) break;

    String nome = cenasSalvas.substring(ini + 1, fim);
    nome.trim();

    if (nome.length() > 0) {
      html += "<div style='display:flex;align-items:center;margin-bottom:10px;'>";

      html += "<button class='sceneBtn' onclick=\"carregarCenaDoServidor('";
      html += nome;
      html += "')\">";
      html += nome;
      html += "</button>";

      html += "<button class='success' style='margin-left:6px;' onclick=\"acionarCena('";
      html += nome;
      html += "')\">Acionar</button>";

      html += "<button class='danger' style='margin-left:6px;' onclick=\"excluirCena('";
      html += nome;
      html += "')\">Excluir</button>";

      html += "<span style='margin-left:8px;color:#555;'>Rota: /acionar?cena=";
      html += nome;
      html += "</span>";

      html += "</div>";
    }

    start = fim + 1;
  }

  if (html == "") {
    html = "<p>Nenhuma cena salva ainda.</p>";
  }

  return html;
}

String htmlPage() {
  String html = "";

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
  html += ".delayBtn{background:#ef6c00;}";
  html += ".sceneBtn{background:#455a64;}";
  html += ".itemCena{padding:10px;margin:6px;background:#e3f2fd;border:1px solid #90caf9;border-radius:6px;cursor:move;}";
  html += ".itemDelay{background:#fff3e0;border:1px solid #ffb74d;}";
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
  html += "<p>Clique no botão da tecla para ver o código.</p>";
  html += botoesTeclasHtml();
  html += "<p><b>Código da tecla selecionada:</b></p>";
  html += "<textarea id='codigoSelecionado' rows='6'></textarea>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>3. Cenas salvas</h3>";
  html += "<p>Clique no nome da cena para carregar e editar.</p>";
  html += cenasHtml();
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>4. Configurar cena</h3>";

  html += "<p><b>Nome da cena:</b></p>";
  html += "<input id='nomeCena' placeholder='Ex: tv' value='tv'>";

  html += "<p>Clique nas teclas para adicionar na cena. Arraste para ordenar.</p>";
  html += opcoesTeclasHtml();

  html += "<hr>";

  html += "<h4>Adicionar delay</h4>";
  html += "<p>Digite quantos segundos a cena deve esperar quando chegar nesse item.</p>";
  html += "<input id='delaySegundos' type='number' min='1' value='10' placeholder='Segundos'>";
  html += "<button class='delayBtn' onclick='adicionarDelayNaCena()'>Adicionar delay</button>";

  html += "<h4>Ordem da cena</h4>";
  html += "<div id='listaCena'></div>";

  html += "<br>";
  html += "<button class='success' onclick='salvarCena()'>Salvar cena</button>";
  html += "<button class='danger' onclick='limparCena()'>Limpar montagem</button>";

  html += "<p><b>Rota da cena atual:</b></p>";
  html += "<pre id='rotaCenaAtual'>/acionar?cena=tv</pre>";

  html += "<p><b>Dados da cena carregada:</b></p>";
  html += "<pre id='sceneAtual'></pre>";

  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>5. Acionar cena por rota</h3>";
  html += "<p>Agora a rota é:</p>";
  html += "<pre>/acionar?cena=tv</pre>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<a href='/reset-wifi'>Resetar WiFi</a>";
  html += "</div>";

  html += "<script>";

  html += "let cena=[];";

  html += "function nomeCenaAtual(){";
  html += "let nome=document.getElementById('nomeCena').value.trim();";
  html += "nome=nome.replaceAll(' ','_');";
  html += "if(nome.length===0){nome='tv';}";
  html += "document.getElementById('nomeCena').value=nome;";
  html += "document.getElementById('rotaCenaAtual').innerText='/acionar?cena='+nome;";
  html += "return nome;";
  html += "}";

  html += "document.addEventListener('input',function(e){";
  html += "if(e.target && e.target.id==='nomeCena'){nomeCenaAtual();}";
  html += "});";

  html += "function nomeVisual(item){";
  html += "if(item.startsWith('delay:')){";
  html += "let segundos=item.replace('delay:','');";
  html += "return 'Delay de '+segundos+' segundo(s)';";
  html += "}";
  html += "return item;";
  html += "}";

  html += "function mostrarCodigo(nome){";
  html += "fetch('/codigo?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>{document.getElementById('codigoSelecionado').value=t;});";
  html += "}";

  html += "function enviarTecla(nome){";
  html += "fetch('/enviar?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>alert(t));";
  html += "}";

  html += "function excluirTecla(nome){";
  html += "let confirmar=confirm('Deseja excluir essa tecla?');";
  html += "if(!confirmar){return;}";
  html += "fetch('/excluir-tecla?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>{alert(t);location.reload();});";
  html += "}";

  html += "function adicionarNaCena(nome){";
  html += "cena.push(nome);";
  html += "renderizarCena();";
  html += "}";

  html += "function adicionarDelayNaCena(){";
  html += "let segundos=document.getElementById('delaySegundos').value;";
  html += "segundos=parseInt(segundos);";
  html += "if(!segundos || segundos<=0){alert('Informe um delay válido em segundos');return;}";
  html += "cena.push('delay:'+segundos);";
  html += "renderizarCena();";
  html += "}";

  html += "function renderizarCena(){";
  html += "let div=document.getElementById('listaCena');";
  html += "div.innerHTML='';";
  html += "cena.forEach((itemCena,index)=>{";
  html += "let item=document.createElement('div');";
  html += "item.className='itemCena';";
  html += "if(itemCena.startsWith('delay:')){item.className='itemCena itemDelay';}";
  html += "item.draggable=true;";
  html += "item.dataset.index=index;";
  html += "item.innerHTML=(index+1)+' - '+nomeVisual(itemCena)+\" <button onclick='removerItem(\"+index+\")'>Remover</button>\";";
  html += "item.addEventListener('dragstart',dragStart);";
  html += "item.addEventListener('dragover',dragOver);";
  html += "item.addEventListener('drop',dropItem);";
  html += "div.appendChild(item);";
  html += "});";
  html += "document.getElementById('sceneAtual').innerText=cena.join('|');";
  html += "nomeCenaAtual();";
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
  html += "let nome=nomeCenaAtual();";
  html += "let valor=cena.join('|');";
  html += "fetch('/salvar-cena?nome='+encodeURIComponent(nome)+'&scene='+encodeURIComponent(valor))";
  html += ".then(r=>r.text())";
  html += ".then(t=>{alert(t);location.reload();});";
  html += "}";

  html += "function carregarCenaDoServidor(nome){";
  html += "document.getElementById('nomeCena').value=nome;";
  html += "fetch('/ver-cena?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>{";
  html += "cena=t.split('|').filter(x=>x.trim().length>0);";
  html += "renderizarCena();";
  html += "});";
  html += "}";

  html += "function excluirCena(nome){";
  html += "let confirmar=confirm('Deseja excluir a cena '+nome+'?');";
  html += "if(!confirmar){return;}";
  html += "fetch('/excluir-cena?nome='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>{alert(t);location.reload();});";
  html += "}";

  html += "function acionarCena(nome){";
  html += "fetch('/acionar?cena='+encodeURIComponent(nome))";
  html += ".then(r=>r.text())";
  html += ".then(t=>alert(t));";
  html += "}";

  html += "renderizarCena();";

  html += "</script>";

  html += "</body>";
  html += "</html>";

  return html;
}

void setupRotas() {

  server.on("/", []() {
    teclasSalvas = prefs.getString("teclas", "");
    cenasSalvas = prefs.getString("cenas", "");

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

    String nome = limparNome(server.arg("nome"));

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

  server.on("/excluir-tecla", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome");
      return;
    }

    String nome = server.arg("nome");

    prefs.remove(("key_" + nome).c_str());

    teclasSalvas = prefs.getString("teclas", "");

    String novaLista = removerItemDaLista(teclasSalvas, nome);

    prefs.putString("teclas", novaLista);

    Serial.println("Tecla removida: " + nome);

    beepSucesso();

    server.send(200, "text/plain", "Tecla excluída com sucesso");
  });

  server.on("/salvar-cena", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome da cena");
      return;
    }

    if (!server.hasArg("scene")) {
      server.send(400, "text/plain", "Cena inválida");
      return;
    }

    String nome = limparNome(server.arg("nome"));

    if (nome == "") {
      server.send(400, "text/plain", "Nome da cena inválido");
      return;
    }

    String scene = server.arg("scene");

    prefs.putString(("scene_" + nome).c_str(), scene);

    adicionarCenaNaLista(nome);

    Serial.println("Cena salva:");
    Serial.println(nome);
    Serial.println(scene);

    beepSucesso();

    server.send(200, "text/plain", "Cena salva com sucesso: " + nome);
  });

  server.on("/ver-cena", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome da cena");
      return;
    }

    String nome = server.arg("nome");

    String scene = getCena(nome);

    server.send(200, "text/plain", scene);
  });

  server.on("/excluir-cena", []() {
    if (!server.hasArg("nome")) {
      server.send(400, "text/plain", "Informe o nome da cena");
      return;
    }

    String nome = server.arg("nome");

    prefs.remove(("scene_" + nome).c_str());

    cenasSalvas = prefs.getString("cenas", "");

    String novaLista = removerItemDaLista(cenasSalvas, nome);

    prefs.putString("cenas", novaLista);

    Serial.println("Cena removida: " + nome);

    beepSucesso();

    server.send(200, "text/plain", "Cena excluída com sucesso: " + nome);
  });

  server.on("/acionar", []() {
    if (!server.hasArg("cena")) {
      server.send(400, "text/plain", "Informe a cena. Exemplo: /acionar?cena=tv");
      return;
    }

    String nomeCena = server.arg("cena");

    Serial.println("Acionando cena pela rota: " + nomeCena);

    executarCena(nomeCena);

    server.send(200, "text/plain", "Cena acionada: " + nomeCena);
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
  cenasSalvas = prefs.getString("cenas", "");

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
