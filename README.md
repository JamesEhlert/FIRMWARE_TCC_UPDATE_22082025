# FIRMWARE_TCC_UPDATE_22082025
UPDATE E UPGRADE DO CODIGO DO TCC

Relatório de Melhorias Implementadas no Firmware do ESP32
Projeto: Sistema de Irrigação Inteligente com ESP32 e AWS IoT
Versão Original: FIRMWARE_TCC.ino (análise inicial)
Versão Aprimorada: FIRMWARE_TCC.ino (v3.1 - Compatibilidade)

Introdução
O objetivo deste relatório é documentar e justificar o processo de refatoração do firmware original. A meta principal foi evoluir o código de um protótipo funcional para um sistema embarcado mais robusto, estável, modular e de fácil manutenção, aplicando boas práticas de programação e arquiteturas de software mais avançadas, como o uso de um Sistema Operacional de Tempo Real (FreeRTOS).

A seguir, detalhamos as principais melhorias implementadas em cada área do sistema.

1. Estrutura e Gerenciamento de Tarefas: A Evolução para Multitarefa Real
Esta foi a mudança mais significativa e impactante em toda a arquitetura do firmware.

No Código Original:
O sistema operava em um único fluxo de execução dentro da função loop(). A lógica para executar diferentes ações (ler sensor, verificar conexão, etc.) em tempos diferentes era controlada manualmente usando a função millis(). Embora funcional e eficiente para sistemas simples (uma técnica conhecida como "multitarefa cooperativa"), essa abordagem se torna complexa e suscetível a erros à medida que novas funcionalidades são adicionadas, pois o tempo de execução de uma função afeta diretamente todas as outras.

No Código Aprimorado (v3.1):
Implementamos o FreeRTOS, um Sistema Operacional de Tempo Real que já vem integrado ao framework do ESP32. Isso nos permitiu abandonar o loop() monolítico e dividir as responsabilidades do sistema em tarefas independentes que rodam de forma concorrente:

taskNetworkManager: Uma tarefa dedicada exclusivamente a uma função crítica: garantir que a conexão Wi-Fi e a comunicação com a AWS via MQTT estejam sempre ativas. Ela roda continuamente em segundo plano.

taskSensorPublisher: Uma tarefa otimizada para uma única função: acordar a cada 10 minutos, ler os dados do sensor AHT, formatar a mensagem JSON e publicá-la na nuvem. Após sua execução, ela "dorme" pelo tempo programado, não consumindo recursos da CPU desnecessariamente.

Benefício: Essa separação torna o código extremamente modular e resiliente. Um problema ou um atraso na tarefa do sensor, por exemplo, não interfere na capacidade do sistema de receber comandos da nuvem, pois a tarefa de rede continua operando de forma independente.

2. Conectividade e Gerenciamento de Erros: Sistema Autônomo e Resiliente
A forma como o sistema lida com falhas de rede foi completamente redesenhada para garantir a operação contínua e autônoma.

No Código Original:
A lógica de conexão com o Wi-Fi e a AWS estava localizada na função setup(). Isso representava um ponto crítico de falha: se o roteador estivesse desligado ou a internet indisponível no momento em que o ESP32 fosse ligado, o dispositivo ficaria travado em um laço while infinito, nunca iniciando sua operação principal. Além disso, não havia uma estratégia explícita no loop() para se recuperar de uma queda de Wi-Fi durante a operação.

No Código Aprimorado (v3.1):
Toda a lógica de conexão foi movida para a taskNetworkManager. Agora, a conexão não é mais um passo bloqueante na inicialização.

Reconexão Automática: A tarefa verifica continuamente o status da conexão. Se o Wi-Fi cair ou a conexão MQTT for perdida a qualquer momento, a tarefa automaticamente iniciará o processo de reconexão em segundo plano, sem travar o resto do sistema.

Operação Offline: Enquanto a rede está indisponível, as outras partes do sistema continuam funcionando. Por exemplo, a lógica de desligamento do temporizador da irrigação no loop() principal continua operando normalmente, garantindo que a irrigação não fique ligada indefinidamente por uma falha de rede.

3. Organização, Legibilidade e Manutenibilidade
A estrutura do arquivo de código foi profissionalizada para facilitar o entendimento, a depuração e futuras expansões.

No Código Original:
O código era funcional, mas seguia um script linear. Variáveis, funções e lógica estavam misturadas, tornando a navegação e o entendimento mais difíceis para alguém que não escreveu o código.

No Código Aprimorado (v3.1):

Seções Lógicas: O código foi dividido em seções claras e bem definidas (INCLUDES, CONSTANTES, OBJETOS GLOBAIS, PROTÓTIPOS, SETUP, LOOP, TAREFAS, etc.), facilitando a localização de qualquer parte do programa.

Comentários Profissionais (Doxygen): Foram adicionados comentários em um formato padrão (Doxygen) no cabeçalho do arquivo e antes de cada função. Eles explicam o propósito (@brief), os parâmetros e a versão do código, tornando-o autodescritivo.

Nomes Descritivos: Variáveis como ledOn foram renomeadas para isIrrigationOn, tornando a intenção do código imediatamente clara, o que reduz a chance de erros lógicos.

Loop Limpo: A função loop() se tornou mínima e de fácil entendimento. Suas responsabilidades são agora muito claras: cuidar do timer da irrigação e nada mais. Todo o trabalho pesado foi abstraído para as tarefas do FreeRTOS.

4. Boas Práticas de Programação em C++
Pequenos, mas importantes, ajustes foram feitos para alinhar o código com práticas modernas de programação para sistemas embarcados.

No Código Original:
O ajuste de fuso horário era feito com timeinfo.tm_hour -= 3;, um "número mágico" que pode ser confuso e não lida bem com regras mais complexas.

No Código Aprimorado (v3.1):

Gerenciamento de Tempo: O ajuste de fuso horário foi substituído pela configuração padrão via configTime(-3 * 3600, 0, ...), que é a forma correta e mais robusta de definir o fuso horário para o sistema.

Uso de const: Valores fixos, como os pinos e os tópicos MQTT, foram declarados como constantes (const). Isso previne modificações acidentais durante a execução e permite que o compilador otimize o código de forma mais eficiente.

Tipos de Dados: Foi adicionado o sufixo UL (Unsigned Long) em cálculos de tempo (duration_min * 60000UL) para evitar potenciais estouros de memória (overflow) com números grandes.

Tabela Comparativa Resumida
Característica	Código Original	Código Aprimorado (v3.1)
Arquitetura	loop() único (multitarefa cooperativa)	FreeRTOS com tarefas concorrentes e independentes.
Conectividade	Bloqueante no setup(), sem reconexão explícita.	Não-bloqueante, com reconexão automática em segundo plano.
Modularidade	Baixa. Funções interdependentes no mesmo fluxo.	Alta. Lógica de rede e de sensores totalmente desacoplada.
Legibilidade	Funcional, mas pouco estruturada.	Alta, com seções lógicas, comentários e nomes descritivos.
Robustez	Vulnerável a falhas de rede na inicialização.	Resiliente a falhas de rede a qualquer momento.
Manutenibilidade	Difícil de expandir sem introduzir bugs.	Fácil de adicionar novas funcionalidades criando novas tarefas.

Exportar para as Planilhas
Conclusão
A refatoração transformou o firmware de um protótipo funcional em um sistema embarcado robusto, escalável e profissional. A adoção do FreeRTOS e de práticas de programação defensiva (como a reconexão automática) garante que o sistema de irrigação inteligente seja confiável para operação de longo prazo e esteja preparado para futuras expansões com o mínimo de esforço.
