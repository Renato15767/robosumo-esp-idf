# RoboSumo autônomo com ESP-IDF

Firmware em C para ESP32 clássico, ponte H TB6612FNG, dois ultrassons HC-SR04 e dois sensores infravermelhos de borda. O ESP32 é a única placa de controle. O projeto usa componentes nativos do ESP-IDF e inicia sozinho, sem Wi-Fi.

[Repositório do projeto no GitHub](https://github.com/Renato15767/robosumo-esp-idf)

## Como funciona

1. A ponte H fica em espera durante os primeiros 5 segundos. O robô começa a busca com os dois sensores de borda sobre o preto.
2. Os ultrassons medem alternadamente, com alcance configurado entre 2 e 60 cm. O frontal confirma com dois acertos nas últimas três leituras, incluindo a mais recente. Durante o ataque, os dois motores recebem PWM de 100% e o robô tolera até 260 ms sem um eco válido antes de voltar a procurar. O traseiro exige duas leituras consecutivas.
3. Quando só o sonar traseiro confirma, o robô freia, executa uma meia-volta e mede novamente. Sem alvo, procura em giros curtos com pausas para medir.
4. A faixa branca tem prioridade. O robô freia e recua se a faixa está à frente, ou avança se ela está atrás. De volta ao preto, percorre uma margem curta, freia, gira e retoma a busca. A saída da borda e os giros usam PWM de aproximadamente 50%.
5. Branco nos dois sensores, branco no caminho da fuga ou fuga sem sucesso fazem o robô parar até reiniciar.

O tempo de giro inicial é de 480 ms. Ajuste `HALF_TURN_MS` em [main/controle.h](main/controle.h) com a alimentação e a superfície da arena. O ajuste depende da tração e da velocidade das rodas. O firmware foi compilado para ESP32 e a lógica foi testada em computador. A meia-volta, a detecção da faixa e a capacidade de empurrar a garrafa exigem teste físico.

## Ferramentas e bibliotecas

| Item | Versão nesta entrega | Referência |
| --- | --- | --- |
| ESP-IDF | 5.5.0 | [Release v5.5](https://github.com/espressif/esp-idf/releases/tag/v5.5) |
| GPIO e LEDC | Componentes do ESP-IDF 5.5.0 | [GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/peripherals/gpio.html), [LEDC](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/peripherals/ledc.html) |
| FreeRTOS, esp_timer, esp_rom e esp_log | Componentes integrados ao ESP-IDF 5.5.0 | [FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/freertos_idf.html), [ESP Timer](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/esp_timer.html), [API ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/index.html) |
| PlatformIO Core | 6.2.0 | [PyPI](https://pypi.org/project/platformio/6.2.0/) |
| Plataforma Espressif32 do PlatformIO | 6.12.0 | [Release v6.12.0](https://github.com/platformio/platform-espressif32/releases/tag/v6.12.0) |

O projeto não tem bibliotecas de terceiros, `lib_deps` ou dependência de Arduino. O sonar usa GPIO, interrupção e fila do FreeRTOS.

## Compilar e gravar com ESP-IDF

Instale o [ESP-IDF 5.5 para ESP32](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/index.html) e abra o terminal com o ambiente do IDF ativado. Na raiz do projeto:

```text
idf.py set-target esp32
idf.py build
idf.py -p COM7 flash monitor
```

Troque `COM7` pela porta da placa. No Linux, ela costuma ser `/dev/ttyUSB0`. Saia do monitor do IDF com Ctrl+]. Para testar apenas os sensores, habilite **RoboSumo → Diagnostico com motores bloqueados** em `idf.py menuconfig` antes de compilar. Desabilite essa opção para gravar o modo autônomo.

## Compilar e gravar com PlatformIO no Windows

Esta opção também compila com ESP-IDF. Abra o PowerShell na raiz do projeto:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.2.0
.\.venv\Scripts\python.exe -m platformio run -e autonomo
.\.venv\Scripts\python.exe -m platformio device list
```

Para testar os sensores antes de mover as rodas:

```powershell
.\.venv\Scripts\python.exe -m platformio run -e diagnostico -t upload --upload-port COM7
.\.venv\Scripts\python.exe -m platformio device monitor --port COM7 --baud 115200
```

O diagnóstico mostra os sensores na ordem **frente, trás**. Verifique se cada sensor de borda indica 1 no preto e 0 no branco. Aproxime a garrafa de um sonar por vez e confira se a distância muda no canal correspondente. `-1` indica ausência de eco válido dentro da janela de medição.

Feche o monitor e grave a versão autônoma:

```powershell
.\.venv\Scripts\python.exe -m platformio run -e autonomo -t upload --upload-port COM7
```

Durante a gravação, mantenha o suporte de baterias desligado e retire o cabo do buck do ESP32. A placa pode reiniciar ao final. Para testar na arena, desligue o robô, retire o USB do computador, reconecte a alimentação do robô e coloque os dois sensores de borda sobre o preto. Ligue a chave e espere os cinco segundos.

## Arquivos principais

- [PINAGEM.md](PINAGEM.md) contém o diagrama e as ligações fio a fio.
- `main/config.h` define GPIOs, polaridade e PWM.
- `main/hardware.c` controla a ponte, lê os sensores de borda e mede o ECHO.
- `main/controle.c` define a busca, a perseguição e a fuga da borda.
- `main/main.c` aplica as decisões a cada 5 ms.
- `tests/teste_controle.c` testa a lógica fora da placa.

Para executar o teste localmente com um compilador C:

```sh
cc -std=c11 -Wall -Wextra -Werror -I main tests/teste_controle.c main/controle.c -o teste-robosumo
./teste-robosumo
```
