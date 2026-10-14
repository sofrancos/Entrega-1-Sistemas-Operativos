#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include "../comun/protocolo.h"
#include "../comun/csv.h"

// Nombres para los recursos IPC POSIX
#define SHM_NAME "/dinos_shm"
#define SEM_SRV_NAME "/dinos_sem_srv"
#define SEM_CLI_NAME "/dinos_sem_cli"

// Estructura que residirá en la memoria compartida
typedef struct {
    solicitud_t solicitud;
    respuesta_t respuesta;
} shm_data_t;

int main(int argc, char *argv[]) {
    // Por defecto asume que los datos están en data/pbdb_data.csv
    const char *ruta_csv = "data/dataset.csv"; 
    
    printf("[Servidor SHM] Iniciando servidor...\n");

    // 1. Crear y dimensionar la memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[Error] shm_open");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(shm_data_t));
    shm_data_t *shm_ptr = mmap(0, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("[Error] mmap");
        exit(EXIT_FAILURE);
    }

    // 2. Crear los semáforos inicializados en 0
    sem_t *sem_srv = sem_open(SEM_SRV_NAME, O_CREAT, 0666, 0);
    sem_t *sem_cli = sem_open(SEM_CLI_NAME, O_CREAT, 0666, 0);
    if (sem_srv == SEM_FAILED || sem_cli == SEM_FAILED) {
        perror("[Error] sem_open");
        exit(EXIT_FAILURE);
    }

    printf("[Servidor SHM] Esperando solicitudes...\n");
    char resultados[MAX_RESULTADOS][TAM_REGISTRO];

    while (1) {
        // Esperar a que el cliente escriba una solicitud
        sem_wait(sem_srv);

        if (shm_ptr->solicitud.tipo == MSG_SALIR) {
            printf("[Servidor SHM] Recibida senal de salida. Terminando...\n");
            break;
        }

        if (shm_ptr->solicitud.tipo == MSG_CONSULTA) {
            printf("[Servidor SHM] Consulta recibida: criterio %d, valor '%s'\n",
                   shm_ptr->solicitud.criterio, shm_ptr->solicitud.valor);

            // Realizar la búsqueda usando la función base
            int encontrados = csv_buscar(ruta_csv, shm_ptr->solicitud.criterio, shm_ptr->solicitud.valor, resultados, MAX_RESULTADOS);

            if (encontrados < 0) {
                shm_ptr->respuesta.tipo = RESP_ERROR;
                strncpy(shm_ptr->respuesta.registro, "Error leyendo el archivo CSV", TAM_REGISTRO);
                sem_post(sem_cli); // Despierta al cliente
            } else if (encontrados == 0) {
                shm_ptr->respuesta.tipo = RESP_SIN_RESULTADOS;
                sem_post(sem_cli); // Despierta al cliente
            } else {
                // Enviar resultados uno a uno asegurando sincronización
                for (int i = 0; i < encontrados; i++) {
                    shm_ptr->respuesta.tipo = RESP_REGISTRO;
                    strncpy(shm_ptr->respuesta.registro, resultados[i], TAM_REGISTRO);
                    
                    sem_post(sem_cli); // Avisa al cliente que hay un nuevo registro listo
                    sem_wait(sem_srv); // Espera confirmación (ACK) del cliente antes de sobrescribir
                }
                
                // Indicar al cliente que no hay más registros
                shm_ptr->respuesta.tipo = RESP_FIN;
                sem_post(sem_cli); // Último aviso, el cliente ya no responderá con ACK tras esto
            }
        }
    }

    // 3. Liberación de recursos del sistema operativo
    munmap(shm_ptr, sizeof(shm_data_t));
    close(shm_fd);
    shm_unlink(SHM_NAME); // Desvincula la memoria compartida
    
    sem_close(sem_srv);
    sem_close(sem_cli);
    sem_unlink(SEM_SRV_NAME); // Desvincula semáforo del servidor
    sem_unlink(SEM_CLI_NAME); // Desvincula semáforo del cliente

    printf("[Servidor SHM] Apagado correcto.\n");
    return 0;
}
