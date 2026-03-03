#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gerenciador.h"
#include <sys/types.h>
#include <unistd.h>
#include "../headers/sistema.h"

void iniciar_monitorCPU(){
    pid_t pid = fork();
    if(pid == 0){
        // Redireciona saída para arquivo
        FILE *log = fopen("monitorCPU.log", "w");
        if (log == NULL) {
            perror("Erro ao abrir arquivo para monitorCPU");
            _exit(1);
        }
        dup2(fileno(log), STDOUT_FILENO);
        dup2(fileno(log), STDERR_FILENO);
        fclose(log);

        execl("./monitorCPU", "monitor_CPU", NULL);
        perror("Erro ao executar monitorCPU");
        _exit(1);
    } else if (pid > 0){
        printf("[GERENCIADOR] Monitor de CPU iniciado (PID %d)\n", pid);
    } else {
        perror("Erro ao criar processo do monitor de CPU");
    }
}

void gerenciador() {
    Sistema sistema;
    inicializa_sistema(&sistema);
    criar_processo_inicial(&sistema, "programas/init.txt");

    char comando[128];

    while (fgets(comando, sizeof(comando), stdin)) {
        // Remove qualquer \n ou \r do final da string
        size_t len = strlen(comando);
        while (len > 0 && (comando[len - 1] == '\n' || comando[len - 1] == '\r')) {
            comando[--len] = 0;
        }

        if (strcmp(comando, "U") == 0) {
            sistema.tempo++;
            executar_proxima_instrucao(&sistema);
            printf("\n[G] Tempo avançado para %d\n", sistema.tempo);
        } else if (strcmp(comando, "I") == 0) {
            imprimir_estado(&sistema);
        } else if (strcmp(comando, "M") == 0) {
            printf("\n[G] Finalizando sistema.\n");
            imprimir_estado(&sistema);
            break;
        } else {
            printf("\n[G] Comando inválido: %s\n", comando);
        }

        fflush(stdout);
    }
}
