CC = gcc
CFLAGS = -Wall -Wextra -Icomun -pthread
LDFLAGS = -lrt -pthread

BIN_DIR = bin

# Objetos/Ejecutables
FIFO_SRV = $(BIN_DIR)/servidor_fifo
FIFO_CLI = $(BIN_DIR)/cliente_fifo
SHM_SRV  = $(BIN_DIR)/servidor_shm
SHM_CLI  = $(BIN_DIR)/cliente_shm

.PHONY: all fifo shm clean setup

all: setup fifo shm

setup:
	@mkdir -p $(BIN_DIR)

# --- Compilación versión FIFO ---
fifo: setup $(FIFO_SRV) $(FIFO_CLI)

$(FIFO_SRV): fifo/servidor_fifo.c comun/csv.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(FIFO_CLI): fifo/cliente_fifo.c comun/menu.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# --- Compilación versión SHM ---
shm: setup $(SHM_SRV) $(SHM_CLI)

$(SHM_SRV): shm/servidor_shm.c comun/csv.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(SHM_CLI): shm/cliente_shm.c comun/menu.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# --- Limpieza de binarios y recursos temporales ---
clean:
	rm -rf $(BIN_DIR) /tmp/fifo_dino_* /dev/shm/dinos_*
	@echo "Limpieza completada correctamente."
