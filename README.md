# Gerenciamento de Processos e Escalonamento

## Sobre o projeto  

O Trabalho Prático 1 da disciplina de Sistemas Operacionais tem como objetivo aplicar conceitos fundamentais de gerenciamento de processos em Sistemas Operacionais. O projeto consiste na implementação e simulação de políticas de escalonamento de CPU, permitindo a análise do comportamento dos processos durante a execução.

O foco principal está na compreensão do ciclo de vida de um processo e na comparação de algoritmos de escalonamento.

## Objetivos  

- Compreender o ciclo de vida de um processo  
- Implementar algoritmos de escalonamento  
- Calcular métricas como tempo de espera e tempo de retorno  
- Analisar desempenho de diferentes estratégias  

## Funcionalidades Implementadas  

- Leitura de processos com tempo de chegada e tempo de execução  
- Simulação da execução na CPU  
- Implementação de algoritmos de escalonamento  
- Cálculo de métricas de desempenho  
- Exibição da ordem de execução  

## Algoritmos Trabalhados  

- FCFS (First Come, First Served)  
- SJF (Shortest Job First)  
- Round Robin  
- Escalonamento por Prioridade (quando aplicável)  

## Tecnologias Utilizadas  

- Linguagem C  
- GCC  
- Ambiente Linux  

## Como executar  

### Pré-requisitos  

- GCC instalado  
- Linux ou WSL  

### Compilação  

```bash
gcc *.c -o tp1  
./tp1  

---