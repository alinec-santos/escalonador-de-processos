#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "gerenciador.h"

int tipo_escalonamento = 0; // 0 - Prioridade, 1 - Round Robin

void modo_interativo(int write_fd);
void modo_arquivo(int write_fd, const char *arquivo);

int main(int argc, char *argv[]) {
    int pipe_fd[2];
    iniciar_monitorCPU();
    printf("\n> Escolha o algoritmo de escalonamento\n< 1-Prioridade 2-Round Robin >: ");
    int escolha_algo;
    scanf("%d", &escolha_algo);
    getchar(); // consome \n
    tipo_escalonamento = (escolha_algo == 2) ? 1 : 0;

    if (pipe(pipe_fd) == -1) {
        perror("Erro ao criar pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Erro no fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Processo gerenciador
        close(pipe_fd[1]); // fecha escrita
        dup2(pipe_fd[0], STDIN_FILENO); // redireciona leitura do pipe para STDIN
        close(pipe_fd[0]);
        gerenciador();
        exit(EXIT_SUCCESS);
    } else {
        // Processo controlador
        close(pipe_fd[0]); // fecha leitura

        int escolha;
        usleep(100000); // pequena espera para garantir sincronização
        printf("\n> Escolha modo de entrada\n< 1-Interativo 2-Arquivo >: ");
        scanf("%d", &escolha);
        getchar(); // consome \n

        if (escolha == 1) {
            modo_interativo(pipe_fd[1]);
        } else if (escolha == 2) {
            char nome_arquivo[100];
            printf("> Digite o nome do arquivo de entrada: ");
            fgets(nome_arquivo, sizeof(nome_arquivo), stdin);
            nome_arquivo[strcspn(nome_arquivo, "\n")] = 0; // remove \n
            modo_arquivo(pipe_fd[1], nome_arquivo);
        } else {
            printf("Opção inválida.\n");
        }

        close(pipe_fd[1]);
        wait(NULL); // aguarda processo filho
    }

    return 0;
}

void modo_interativo(int write_fd) {
    char buffer[128];
    char espera;

    while (1) {
        printf("\n> Digite um comando (U, I, M): ");
        fflush(stdout);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
            break;

        write(write_fd, buffer, strlen(buffer));
        
        if (buffer[0] == 'M')
            break;

         usleep(300000); 
        // Espera ENTER antes de mostrar o próximo prompt
        printf("\n[Pressione ENTER para continuar]");
        fflush(stdout);
        espera = getchar(); // Espera o usuário apertar ENTER
        while (espera != '\n') {
            espera = getchar(); // Limpa buffer caso usuário digite qualquer coisa
        }

        // Limpa a tela do terminal (funciona em Unix/WSL)
        system("clear");
    }
}


void modo_arquivo(int write_fd, const char *arquivo) {
    FILE *fp = fopen(arquivo, "r");
    if (!fp) {
        perror("Erro ao abrir arquivo");
        return;
    }

    char linha[128];
    while (fgets(linha, sizeof(linha), fp)) {
        printf("\n[Debug] Enviando comando: %s", linha); // Feedback visual
        write(write_fd, linha, strlen(linha));
        usleep(50000); // Delay entre comandos (50ms)
        if (linha[0] == 'M') break;
    }

    fclose(fp);
}
