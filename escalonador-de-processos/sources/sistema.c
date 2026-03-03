#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>
#include "sistema.h"

// Cores ANSI
#define ANSI_RESET       "\033[0m"
#define ANSI_RED         "\033[1;31m"
#define ANSI_GREEN       "\033[1;32m"
#define ANSI_YELLOW      "\033[1;33m"
#define ANSI_BLUE        "\033[1;34m"
#define ANSI_MAGENTA     "\033[1;35m"
#define ANSI_CYAN        "\033[1;36m"

// Variável global para tipo de escalonamento
extern int tipo_escalonamento;

void escalonar_proximo_processo(Sistema *sistema);
void escalonar_round_robin(Sistema *sistema);

void inicializa_fila(FilaDeProcessos *fila) {
    fila->inicio = 0;
    fila->fim = 0;
}

int esta_vazia(FilaDeProcessos *fila) {
    return fila->inicio == fila->fim;
}

void enqueue(FilaDeProcessos *fila, int id) {
    fila->fila[fila->fim++] = id;
}

int dequeue(FilaDeProcessos *fila) {
    if (esta_vazia(fila)) return -1;
    return fila->fila[fila->inicio++];
}

void inicializa_sistema(Sistema *sistema) {
    sistema->tempo = 0;
    sistema->total_processos = 0;
    sistema->cpu.programa = NULL;
    sistema->cpu.pc = 0;
    sistema->cpu.memoria = NULL;
    sistema->cpu.tamanho_memoria = 0;
    sistema->cpu.quantum_usado = 0;
    sistema->cpu.processo_id = -1;
    sistema->cpu.prioridade = 0;

    inicializa_fila(&sistema->fila_round_robin);

    for (int i = 0; i < MAX_PRIORIDADE; i++)
        inicializa_fila(&sistema->estado_pronto[i]);

    inicializa_fila(&sistema->estado_bloqueado);
}

void criar_processo_inicial(Sistema *sistema, const char *nome_arquivo) {
    ProcessoSimulado *p = &sistema->tabela[0];
    p->id = 0;
    p->id_pai = -1;
    p->estado = PRONTO;
    p->pc = 0;
    p->prioridade = 0;
    p->tempo_inicio = sistema->tempo;
    p->tempo_cpu = 0;

    if (!carregar_programa(nome_arquivo, &p->programa, &p->tamanho_memoria)) {
        fprintf(stderr, ANSI_RED "Erro ao carregar o programa inicial.\n" ANSI_RESET);
        exit(EXIT_FAILURE);
    }

    p->memoria = calloc(100, sizeof(int));

    enqueue(&sistema->estado_pronto[0], 0);
    enqueue(&sistema->fila_round_robin, p->id);

    sistema->total_processos = 1;

    printf(ANSI_GREEN "\n[G] Processo init (P0) criado com sucesso.\n" ANSI_RESET);
    fflush(stdout);
}

const char* estado_str(Estado estado) {
    switch (estado) {
        case PRONTO: return "PRONTO";
        case EXECUTANDO: return "EXECUTANDO";
        case BLOQUEADO: return "BLOQUEADO";
        case TERMINADO: return "TERMINADO";
        default: return "DESCONHECIDO";
    }
}

void imprimir_estado(const Sistema *sistema) {
    sem_t *sem = sem_open("/sem_impressao", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho: impressão protegida
        sem_wait(sem);

        printf(ANSI_CYAN "\n[INFO] Estado atual do sistema\n" ANSI_RESET);
        printf("--------------------------------------------------\n");
        printf("Tempo atual: %d\n", sistema->tempo);
        printf("Total de processos ativos: %d\n\n", sistema->total_processos);
        printf("ID  | Pai | Estado     | Prioridade | PC  | CPU\n");
        printf("----|-----|------------|------------|-----|-----\n");

        for (int i = 0; i < sistema->total_processos; i++) {
            const ProcessoSimulado *p = &sistema->tabela[i];

            printf("P%-3d| %-4d| %-11s| %-10d| %-3d | %-3d\n",
                   p->id,
                   p->id_pai,
                   estado_str(p->estado),
                   p->prioridade,
                   p->pc,
                   p->tempo_cpu);
        }

        printf("--------------------------------------------------\n");
        fflush(stdout);

        sem_post(sem);
        sem_close(sem);
        _exit(0);
    } else if (pid > 0) {
        // Processo pai: espera o filho terminar
        waitpid(pid, NULL, 0);
        sem_close(sem);
    } else {
        perror("fork");
        sem_close(sem);
    }
}

int carregar_programa(const char *nome_arquivo, char ***programa, int *tamanho) {
    FILE *arquivo = fopen(nome_arquivo, "r");
    if (!arquivo) {
        perror(ANSI_RED "Erro ao abrir o arquivo do programa" ANSI_RESET);
        return 0;
    }

    *programa = malloc(MAX_INSTRUCOES * sizeof(char *));
    char linha[128];
    int count = 0;

    while (fgets(linha, sizeof(linha), arquivo) && count < MAX_INSTRUCOES) {
        linha[strcspn(linha, "\n")] = 0;
        (*programa)[count] = strdup(linha);
        count++;
    }

    *tamanho = count;
    fclose(arquivo);
    return 1;
}

void escalonar_proximo_processo(Sistema *sistema) {
    for (int i = 0; i < MAX_PRIORIDADE; i++) {
        if (!esta_vazia(&sistema->estado_pronto[i])) {
            int pid = dequeue(&sistema->estado_pronto[i]);
            ProcessoSimulado *p = &sistema->tabela[pid];

            if (p->estado == TERMINADO) continue;

            sistema->cpu.processo_id = p->id;
            sistema->cpu.programa = p->programa;
            sistema->cpu.pc = p->pc;
            sistema->cpu.memoria = p->memoria;
            sistema->cpu.tamanho_memoria = 100;
            sistema->cpu.quantum_usado = 0;
            sistema->cpu.prioridade = p->prioridade;

            p->estado = EXECUTANDO;
            printf(ANSI_BLUE "\n[G] Processo P%d escalonado para execução\n" ANSI_RESET, pid);
            return;
        }
    }

    sistema->cpu.processo_id = -1;
}

void escalonar_round_robin(Sistema *sistema) {
    int tentativas = 0;
    int total = sistema->total_processos;
    while (!esta_vazia(&sistema->fila_round_robin) && tentativas < total) {
        int pid = dequeue(&sistema->fila_round_robin);
        ProcessoSimulado *p = &sistema->tabela[pid];

        if (p->estado == TERMINADO) {
            tentativas++;
            continue;
        }

        sistema->cpu.processo_id = p->id;
        sistema->cpu.programa = p->programa;
        sistema->cpu.pc = p->pc;
        sistema->cpu.memoria = p->memoria;
        sistema->cpu.tamanho_memoria = 100;
        sistema->cpu.quantum_usado = 0;
        sistema->cpu.prioridade = p->prioridade;

        p->estado = EXECUTANDO;
        printf(ANSI_BLUE "\n[G] (RR) Processo P%d escalonado para execução\n" ANSI_RESET, pid);
        return;
    }
    sistema->cpu.processo_id = -1;
}

void executar_proxima_instrucao(Sistema *sistema) {
    FilaDeProcessos temp_fila;
    inicializa_fila(&temp_fila);

    while (!esta_vazia(&sistema->estado_bloqueado)) {
        int pid = dequeue(&sistema->estado_bloqueado);
        ProcessoSimulado *p = &sistema->tabela[pid];

        if (p->tempo_bloqueio <= sistema->tempo) {
            p->estado = PRONTO;
            if (tipo_escalonamento == 1)
                enqueue(&sistema->fila_round_robin, pid);
            else
                enqueue(&sistema->estado_pronto[p->prioridade], pid);
            printf(ANSI_YELLOW "\n[G] Processo P%d desbloqueado (tempo %d)\n" ANSI_RESET, pid, sistema->tempo);
        } else {
            enqueue(&temp_fila, pid);
        }
    }

    while (!esta_vazia(&temp_fila)) {
        enqueue(&sistema->estado_bloqueado, dequeue(&temp_fila));
    }

    // Escalona novo processo se necessário
    if (sistema->cpu.processo_id == -1) {
        if (tipo_escalonamento == 1)
            escalonar_round_robin(sistema);
        else
            escalonar_proximo_processo(sistema);
    }

    int pid = sistema->cpu.processo_id;
    if (pid == -1) {
        printf(ANSI_CYAN "\n[G] Nenhum processo disponível para execução.\n" ANSI_RESET);
        return;
    }

    ProcessoSimulado *proc = &sistema->tabela[pid];
    if (proc->estado == TERMINADO) {
        sistema->cpu.processo_id = -1;
        printf(ANSI_RED "\n[G] Tentativa de executar processo P%d já terminado\n" ANSI_RESET, pid);
        return;
    }

    char *instrucao = proc->programa[sistema->cpu.pc];
    printf(ANSI_CYAN "\n[G] Executando instrução: %s(P%d)\n" ANSI_RESET, instrucao, pid);

    char tipo;
    int x, n;
    if (sscanf(instrucao, "%c %d %d", &tipo, &x, &n) >= 1) {
        switch (tipo) {
            case 'D':
                if (x >= 0 && x < 100) sistema->cpu.memoria[x] = 0;
                break;
            case 'V':
                if (x >= 0 && x < 100) sistema->cpu.memoria[x] = n;
                break;
            case 'A':
                if (x >= 0 && x < 100) sistema->cpu.memoria[x] += n;
                break;
            case 'S':
                if (x >= 0 && x < 100) sistema->cpu.memoria[x] -= n;
                break;
            case 'B': {
                int tempo_bloqueio;
                if (sscanf(instrucao, "B %d", &tempo_bloqueio) == 1) {
                    printf(ANSI_YELLOW "\n[G] Processo P%d bloqueado por %d unidades de tempo\n" ANSI_RESET, pid, tempo_bloqueio);
                    proc->estado = BLOQUEADO;
                    proc->tempo_bloqueio = sistema->tempo + tempo_bloqueio;
                    enqueue(&sistema->estado_bloqueado, pid);
                    proc->pc++;
                    sistema->cpu.processo_id = -1;
                    if (tipo_escalonamento == 1)
                        escalonar_round_robin(sistema);
                    else
                        escalonar_proximo_processo(sistema);
                    return;
                }
                break;
            }
            case 'T': {
                printf(ANSI_MAGENTA "\n[G] Processo P%d terminado.\n" ANSI_RESET, pid);

                for (int i = 0; i < proc->tamanho_memoria; i++) {
                    free(proc->programa[i]);
                }
                free(proc->programa);
                free(proc->memoria);

                proc->estado = TERMINADO;
                sistema->cpu.processo_id = -1;

                if (proc->id_pai != -1) {
                    ProcessoSimulado *pai = &sistema->tabela[proc->id_pai];
                    if (pai->estado != TERMINADO && pai->estado != EXECUTANDO) {
                        pai->estado = PRONTO;
                        if (tipo_escalonamento == 1)
                            enqueue(&sistema->fila_round_robin, proc->id_pai);
                        else
                            enqueue(&sistema->estado_pronto[pai->prioridade], proc->id_pai);
                        printf(ANSI_GREEN "\n[G] Processo pai P%d movido para fila de prontos\n" ANSI_RESET, proc->id_pai);
                    }
                }

                if (tipo_escalonamento == 1)
                    escalonar_round_robin(sistema);
                else
                    escalonar_proximo_processo(sistema);
                return;
            }
            case 'N': {
                char nome_programa[128];
                if (sscanf(instrucao, "N %s", nome_programa) == 1) {
                    char caminho[256];
                    snprintf(caminho, sizeof(caminho), "programas/%s.txt", nome_programa);

                    if (sistema->total_processos >= MAX_PROCESSOS) {
                        printf(ANSI_RED "\n[G] Limite de processos atingido.\n" ANSI_RESET);
                        break;
                    }

                    int novo_pid = sistema->total_processos;
                    ProcessoSimulado *novo = &sistema->tabela[novo_pid];

                    novo->id = novo_pid;
                    novo->id_pai = pid;
                    novo->estado = PRONTO;
                    novo->pc = 0;
                    novo->prioridade = proc->prioridade;
                    novo->tempo_inicio = sistema->tempo;
                    novo->tempo_cpu = 0;
                    novo->tempo_bloqueio = 0;

                    if (!carregar_programa(caminho, &novo->programa, &novo->tamanho_memoria)) {
                        printf(ANSI_RED "\n[G] Falha ao carregar o programa %s\n" ANSI_RESET, caminho);
                        break;
                    }

                    novo->memoria = calloc(100, sizeof(int));
                    if (tipo_escalonamento == 1)
                        enqueue(&sistema->fila_round_robin, novo_pid);
                    else
                        enqueue(&sistema->estado_pronto[novo->prioridade], novo_pid);
                    sistema->total_processos++;

                    printf(ANSI_GREEN "\n[G] Processo P%d criado a partir de %s\n" ANSI_RESET, novo_pid, nome_programa);
                }
                break;
            }
            case 'F': {
                int n;
                if (sscanf(instrucao, "F %d", &n) == 1) {
                    if (sistema->total_processos >= MAX_PROCESSOS) {
                        printf(ANSI_RED "\n[G] Limite de processos atingido.\n" ANSI_RESET);
                        break;
                    }

                    int novo_pid = sistema->total_processos;
                    ProcessoSimulado *novo = &sistema->tabela[novo_pid];

                    *novo = *proc;
                    novo->id = novo_pid;
                    novo->id_pai = pid;
                    novo->estado = PRONTO;
                    novo->pc = sistema->cpu.pc + 1;
                    novo->tempo_inicio = sistema->tempo;
                    novo->tempo_cpu = 0;
                    novo->tempo_bloqueio = 0;

                    novo->programa = malloc(MAX_INSTRUCOES * sizeof(char*));
                    for (int i = 0; i < proc->tamanho_memoria; i++) {
                        novo->programa[i] = strdup(proc->programa[i]);
                    }

                    novo->memoria = malloc(100 * sizeof(int));
                    memcpy(novo->memoria, proc->memoria, 100 * sizeof(int));

                    if (tipo_escalonamento == 1)
                        enqueue(&sistema->fila_round_robin, novo_pid);
                    else
                        enqueue(&sistema->estado_pronto[novo->prioridade], novo_pid);
                    sistema->total_processos++;

                    sistema->cpu.pc += n;
                }
                break;
            }
            case 'R': {
                char nome_arquivo[128];
                if (sscanf(instrucao, "R %s", nome_arquivo) == 1) {
                    char caminho[256];
                    snprintf(caminho, sizeof(caminho), "programas/%s.txt", nome_arquivo);

                    printf(ANSI_CYAN "\n[G] Substituindo imagem do processo P%d com programa de %s\n" ANSI_RESET, pid, caminho);

                    for (int i = 0; i < proc->tamanho_memoria; i++) {
                        if (sistema->cpu.programa[i] != NULL) {
                            free(sistema->cpu.programa[i]);
                        }
                    }
                    free(sistema->cpu.programa);

                    char **novo_programa;
                    int novo_tamanho;

                    if (!carregar_programa(caminho, &novo_programa, &novo_tamanho)) {
                        printf(ANSI_RED "\n[G] Falha ao carregar %s, processo continua com programa atual\n" ANSI_RESET, caminho);
                        break;
                    }

                    sistema->cpu.programa = novo_programa;
                    sistema->cpu.pc = 0;

                    if (sistema->cpu.memoria != NULL) {
                        free(sistema->cpu.memoria);
                    }
                    sistema->cpu.memoria = calloc(100, sizeof(int));
                    sistema->cpu.tamanho_memoria = 100;

                    proc->programa = novo_programa;
                    proc->pc = 0;
                    proc->memoria = sistema->cpu.memoria;
                    proc->tamanho_memoria = novo_tamanho;

                    printf(ANSI_GREEN "\n[G] Imagem do processo P%d substituída com sucesso\n" ANSI_RESET, pid);
                }
                break;
            }
            default:
                printf(ANSI_RED "\n[G] Instrução inválida: %s\n" ANSI_RESET, instrucao);
                break;
        }

        sistema->cpu.pc++;
        proc->pc = sistema->cpu.pc;
        proc->tempo_cpu++;
        sistema->cpu.quantum_usado++;

        int quantum_maximo = (tipo_escalonamento == 1) ? 2 : (1 << proc->prioridade);
        if (sistema->cpu.quantum_usado >= quantum_maximo) {
            proc->estado = PRONTO;
            if (tipo_escalonamento == 1)
                enqueue(&sistema->fila_round_robin, pid);
            else {
                if (proc->prioridade < MAX_PRIORIDADE - 1) {
                    proc->prioridade++;
                }
                enqueue(&sistema->estado_pronto[proc->prioridade], pid);
            }
            sistema->cpu.processo_id = -1;
            if (tipo_escalonamento == 1)
                escalonar_round_robin(sistema);
            else
                escalonar_proximo_processo(sistema);
        }
    }
}