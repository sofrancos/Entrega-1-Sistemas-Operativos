CC = gcc
CFLAGS = -Wall -Wextra -Icomun -pthread
LDFLAGS = -lrt -pthread

BIN_DIR = bin

# Ejecutables
FIFO_SRV = $(BIN_DIR)/servidor_fifo
FIFO_CLI = $(BIN_DIR)/cliente_fifo
SHM_SRV  = $(BIN_DIR)/servidor_shm
SHM_CLI  = $(BIN_DIR)/cliente_shm

# Cabeceras: si cambian (p. ej. MAX_RESULTADOS en protocolo.h) se recompila
COMUN_H = comun/protocolo.h comun/csv.h comun/menu.h
SHM_H   = shm/shm_comun.h

.PHONY: all fifo shm clean limpiar-ipc

all: fifo shm

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# --- Compilación versión FIFO ---
fifo: $(FIFO_SRV) $(FIFO_CLI)

$(FIFO_SRV): fifo/servidor_fifo.c comun/csv.c $(COMUN_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(FIFO_CLI): fifo/cliente_fifo.c comun/menu.c $(COMUN_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

# --- Compilación versión SHM ---
shm: $(SHM_SRV) $(SHM_CLI)

$(SHM_SRV): shm/servidor_shm.c shm/shm_comun.c comun/csv.c $(COMUN_H) $(SHM_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(SHM_CLI): shm/cliente_shm.c shm/shm_comun.c comun/menu.c $(COMUN_H) $(SHM_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

# --- Recursos IPC que pueda dejar una ejecución interrumpida ---
# Los semáforos POSIX con nombre aparecen en /dev/shm con el prefijo "sem."
limpiar-ipc:
	rm -f /tmp/fifo_dino_req /tmp/fifo_dino_res
	rm -f /dev/shm/dinos_shm /dev/shm/sem.dinos_sem_*
	@echo "Recursos IPC eliminados."

# --- Limpieza de binarios y recursos temporales ---
clean: limpiar-ipc
	rm -rf $(BIN_DIR)
	@echo "Limpieza completada correctamente."
