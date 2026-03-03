CC = gcc
CFLAGS = -Wall -g -pthread

SRC = sources
INC = headers

# Objetos da versão com processos
OBJ_ORIG = $(SRC)/main.o $(SRC)/sistema.o $(SRC)/gerenciador.o
TARGET_ORIG = simulador

# Objetos da versão com threads
OBJ_THREADS = $(SRC)/main_threads.o $(SRC)/sistema_threads.o
TARGET_THREADS = simulador_threads

# Monitor de CPU
MONITOR_SRC = $(SRC)/monitorCPU.c
MONITOR_TARGET = monitorCPU

all: $(TARGET_ORIG) $(TARGET_THREADS) $(MONITOR_TARGET)

# =========================
# Compilação versão original (processos)
$(SRC)/main.o: $(SRC)/main.c $(INC)/sistema.h $(INC)/gerenciador.h
	$(CC) $(CFLAGS) -I$(INC) -c $< -o $@

$(SRC)/gerenciador.o: $(SRC)/gerenciador.c $(INC)/gerenciador.h
	$(CC) $(CFLAGS) -I$(INC) -c $< -o $@

$(SRC)/sistema.o: $(SRC)/sistema.c $(INC)/sistema.h
	$(CC) $(CFLAGS) -I$(INC) -c $< -o $@

$(TARGET_ORIG): $(OBJ_ORIG)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

# =========================
# Compilação versão com threads
$(SRC)/main_threads.o: $(SRC)/main_threads.c $(INC)/sistema.h
	$(CC) $(CFLAGS) -I$(INC) -c $< -o $@

# Evita conflito com sistema.o da versão de processos
$(SRC)/sistema_threads.o: $(SRC)/sistema.c $(INC)/sistema.h
	$(CC) $(CFLAGS) -I$(INC) -c $< -o $@

$(TARGET_THREADS): $(OBJ_THREADS)
	$(CC) $(CFLAGS) -o $@ $^

# =========================
# Compilação do monitorCPU
$(MONITOR_TARGET): $(MONITOR_SRC)
	$(CC) $(CFLAGS) -o $@ $^

# =========================
# Limpeza
clean:
	rm -f $(SRC)/*.o $(TARGET_ORIG) $(TARGET_THREADS) $(MONITOR_TARGET)

# =========================
# Execução
test-original: $(TARGET_ORIG)
	@echo "=== Executando simulador (processos) ==="
	./$(TARGET_ORIG)

test-threads: $(TARGET_THREADS)
	@echo "=== Executando simulador_threads (threads) ==="
	./$(TARGET_THREADS)

test-monitor: $(MONITOR_TARGET)
	@echo "=== Executando monitorCPU ==="
	./$(MONITOR_TARGET)

# =========================
# Ajuda
help:
	@echo "Comandos disponíveis:"
	@echo "  make            - Compila os dois simuladores"
	@echo "  make clean      - Limpa os executáveis e objetos"
	@echo "  make test-original - Roda ./simulador"
	@echo "  make test-threads  - Roda ./simulador_threads"
	@echo "  make test-monitor   - Roda ./monitorCPU"

.PHONY: all clean test-original test-threads test-monitor help