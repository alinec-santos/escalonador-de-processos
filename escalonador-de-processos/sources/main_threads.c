#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "sistema.h"

// Estruturas compartilhadas entre threads
typedef struct {
    char comando[128];
    int novo_comando;
    int finalizar;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} CanalComandos;

typedef struct {
    Sistema *sistema;
    CanalComandos *canal;
} DadosCompartilhados;

// Variáveis globais
CanalComandos canal_comandos;
Sistema sistema_global;
pthread_mutex_t mutex_impressao = PTHREAD_MUTEX_INITIALIZER;
int tipo_escalonamento = 0;

// Protótipos das threads
void* thread_controle(void* arg);
void* thread_gerenciador(void* arg);
void* thread_impressao(void* arg);

// Funções auxiliares
void inicializar_canal(CanalComandos *canal);
void enviar_comando(CanalComandos *canal, const char *cmd);
int receber_comando(CanalComandos *canal, char *buffer);

int main(int argc, char *argv[]) {
    pthread_t tid_controle, tid_gerenciador;
    DadosCompartilhados dados;
    printf("\n====================================== MODO THREADS =========================================\n");
    printf("\n> Escolha o algoritmo de escalonamento\n< 1-Prioridade 2-Round Robin >: ");
    int escolha_algo;
    scanf("%d", &escolha_algo);
    getchar(); // consome \n
    tipo_escalonamento = (escolha_algo == 2) ? 1 : 0;
    
    // Inicializa estruturas compartilhadas
    inicializar_canal(&canal_comandos);
    inicializa_sistema(&sistema_global);
    
    dados.sistema = &sistema_global;
    dados.canal = &canal_comandos;
    
    // Cria as threads
    if (pthread_create(&tid_gerenciador, NULL, thread_gerenciador, &dados) != 0) {
        perror("Erro ao criar thread gerenciador");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&tid_controle, NULL, thread_controle, &dados) != 0) {
        perror("Erro ao criar thread controle");
        exit(EXIT_FAILURE);
    }
    
    // Aguarda as threads terminarem
    pthread_join(tid_controle, NULL);
    pthread_join(tid_gerenciador, NULL);
    
    // Cleanup
    pthread_mutex_destroy(&canal_comandos.mutex);
    pthread_cond_destroy(&canal_comandos.cond);
    pthread_mutex_destroy(&mutex_impressao);
    
    return 0;
}

void inicializar_canal(CanalComandos *canal) {
    canal->novo_comando = 0;
    canal->finalizar = 0;
    pthread_mutex_init(&canal->mutex, NULL);
    pthread_cond_init(&canal->cond, NULL);
}

void enviar_comando(CanalComandos *canal, const char *cmd) {
    pthread_mutex_lock(&canal->mutex);
    strcpy(canal->comando, cmd);
    canal->novo_comando = 1;
    pthread_cond_signal(&canal->cond);
    pthread_mutex_unlock(&canal->mutex);
}

int receber_comando(CanalComandos *canal, char *buffer) {
    pthread_mutex_lock(&canal->mutex);
    
    while (!canal->novo_comando && !canal->finalizar) {
        pthread_cond_wait(&canal->cond, &canal->mutex);
    }
    
    if (canal->finalizar) {
        pthread_mutex_unlock(&canal->mutex);
        return 0;
    }
    
    strcpy(buffer, canal->comando);
    canal->novo_comando = 0;
    
    pthread_mutex_unlock(&canal->mutex);
    return 1;
}

void* thread_controle(void* arg) {
    DadosCompartilhados *dados = (DadosCompartilhados*)arg;
    
    int escolha;
    usleep(300000); 
    printf("\n> Escolha modo de entrada\n< 1-Interativo 2-Arquivo >: ");
    scanf("%d", &escolha);
    getchar(); // consome \n
    
    if (escolha == 1) {
        // Modo interativo
        char buffer[128];
        while (1) {
            usleep(300000); 
            printf("\n> Digite um comando (U, I, M): ");
            fflush(stdout);
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
            
            // Remove \n do final
            buffer[strcspn(buffer, "\n")] = 0;
            
            enviar_comando(dados->canal, buffer);
            
            if (buffer[0] == 'M') break;
            usleep(300000); 
            // Espera ENTER para continuar (antes de limpar a tela)
            printf("\n[Pressione ENTER para continuar]");
            fflush(stdout);
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // Limpa buffer

            // Limpa a tela
            system("clear");
        }
    } else if (escolha == 2) {
        // Modo arquivo
        char nome_arquivo[100];
        printf("> Digite o nome do arquivo de entrada: ");
        fgets(nome_arquivo, sizeof(nome_arquivo), stdin);
        nome_arquivo[strcspn(nome_arquivo, "\n")] = 0;
        
        FILE *fp = fopen(nome_arquivo, "r");
        if (!fp) {
            perror("Erro ao abrir arquivo");
            pthread_exit(NULL);
        }
        
        char linha[128];
        while (fgets(linha, sizeof(linha), fp)) {
            linha[strcspn(linha, "\r\n")] = 0;
            printf("[Debug] Enviando comando: %s\n", linha);
            
            enviar_comando(dados->canal, linha);
            usleep(50000);

            if (linha[0] == 'M') break;
        }
        fclose(fp);
    }
    
    // Sinaliza finalização
    pthread_mutex_lock(&dados->canal->mutex);
    dados->canal->finalizar = 1;
    pthread_cond_signal(&dados->canal->cond);
    pthread_mutex_unlock(&dados->canal->mutex);
    
    pthread_exit(NULL);
}


void* thread_gerenciador(void* arg) {
    DadosCompartilhados *dados = (DadosCompartilhados*)arg;
    Sistema *sistema = dados->sistema;
    
    // Cria processo inicial
    criar_processo_inicial(sistema, "programas/init.txt");
    
    char comando[128];
    
    while (receber_comando(dados->canal, comando)) {
        if (strcmp(comando, "U") == 0) {
            sistema->tempo++;
            executar_proxima_instrucao(sistema);
            printf("\n[G] Tempo avançado para %d\n", sistema->tempo);
        } 
        else if (strcmp(comando, "I") == 0) {
            // Cria thread para impressão
            pthread_t tid_impressao;
            if (pthread_create(&tid_impressao, NULL, thread_impressao, sistema) == 0) {
                pthread_join(tid_impressao, NULL); // Aguarda impressão terminar
            }
        } 
        else if (strcmp(comando, "M") == 0) {
            printf("\n[G] Finalizando sistema.\n");

            // Última impressão ANTES de sair do laço
            pthread_t tid_impressao;
            if (pthread_create(&tid_impressao, NULL, thread_impressao, sistema) == 0) {
                pthread_join(tid_impressao, NULL);  // Garante que a impressão final conclua
            }
            usleep(300000); 
             break;  // Sai do loop principal da thread_gerenciador
        

        } 
        else {
            printf("\n[G] Comando inválido: %s\n", comando);
        }
        
        fflush(stdout);
    }
    
    pthread_exit(NULL);
}

void* thread_impressao(void* arg) {
    Sistema *sistema = (Sistema*)arg;
    
    // Protege a impressão com mutex para evitar saídas sobrepostas
    pthread_mutex_lock(&mutex_impressao);
    
    printf("\n[INFO] Estado atual do sistema\n");
    printf("--------------------------------------------------\n");
    printf("Tempo atual: %d\n", sistema->tempo);
    printf("Total de processos ativos: %d\n\n", sistema->total_processos);
    printf("ID  | Pai | Estado     | Prioridade | PC  | CPU\n");
    printf("----|-----|------------|------------|-----|-----\n");
    
    for (int i = 0; i < sistema->total_processos; i++) {
        const ProcessoSimulado *p = &sistema->tabela[i];
        
        const char* estado_str;
        switch (p->estado) {
            case PRONTO: estado_str = "PRONTO"; break;
            case EXECUTANDO: estado_str = "EXECUTANDO"; break;
            case BLOQUEADO: estado_str = "BLOQUEADO"; break;
            case TERMINADO: estado_str = "TERMINADO"; break;
            default: estado_str = "DESCONHECIDO"; break;
        }
        
        printf("P%-3d| %-4d| %-11s| %-10d| %-3d | %-3d\n",
               p->id,
               p->id_pai,
               estado_str,
               p->prioridade,
               p->pc,
               p->tempo_cpu);
    }
    
    printf("--------------------------------------------------\n");
    fflush(stdout);
    
    pthread_mutex_unlock(&mutex_impressao);
    
    pthread_exit(NULL);
}