#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include "../comun/protocolo.h"
#include "../comun/menu.h"

#define SHM_NAME "/dinos_shm"
#define SEM_SRV_NAME "/dinos_sem_srv"
#define SEM_CLI_NAME "/dinos_sem_cli"

typedef struct {
    solicitud_t solicitud;
    respuesta_t respuesta;
} shm_data_t;

int main() {
    // Abrir memoria compartida (O_CREAT incluido por seguridad si el cliente arranca milisegundos antes)
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[Cliente] Error en shm_open");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(shm_data_t));
    shm_data_t *shm_ptr = mmap(0, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("[Cliente] Error en mmap");
        exit(EXIT_FAILURE);
    }

    // Abrir semáforos
    sem_t *sem_srv = sem_open(SEM_SRV_NAME, O_CREAT, 0666, 0);
    sem_t *sem_cli = sem_open(SEM_CLI_NAME, O_CREAT, 0666, 0);
    if (sem_srv == SEM_FAILED || sem_cli == SEM_FAILED) {
        perror("[Cliente] Error abriendo semaforos");
        exit(EXIT_FAILURE);
    }

    while (1) {
        solicitud_t sol;
        menu_leer_solicitud(&sol);
        shm_ptr->solicitud = sol;

        if (sol.tipo == MSG_SALIR) {
            sem_post(sem_srv); // Despierta al servidor para que limpie y termine
            break;
        }

        // Avisar al servidor que la consulta está lista.
        sem_post(sem_srv);

        int num_resultados = 0;
        while (1) {
            sem_wait(sem_cli); // Espera a que el servidor escriba la respuesta

            if (shm_ptr->respuesta.tipo == RESP_REGISTRO) {
                num_resultados++;
                menu_mostrar_registro(shm_ptr->respuesta.registro);
                
                // IMPORTANTE: Enviar ACK al servidor para que sepa que ya extrajimos los datos 
                // y puede sobrescribir con el siguiente registro.
                sem_post(sem_srv); 
                
            } else if (shm_ptr->respuesta.tipo == RESP_SIN_RESULTADOS) {
                menu_mostrar_cierre(RESP_SIN_RESULTADOS, NULL, 0);
                break;
            } else if (shm_ptr->respuesta.tipo == RESP_FIN) {
                menu_mostrar_cierre(RESP_FIN, NULL, num_resultados);
                break;
            } else if (shm_ptr->respuesta.tipo == RESP_ERROR) {
                menu_mostrar_cierre(RESP_ERROR, shm_ptr->respuesta.registro, 0);
                break;
            } else {
                menu_mostrar_cierre(shm_ptr->respuesta.tipo, NULL, 0);
                break;
            }
        }
    }

    // El cliente solo cierra sus referencias, dejando que el servidor destruya (unlink) los IPC
    munmap(shm_ptr, sizeof(shm_data_t));
    close(shm_fd);
    sem_close(sem_srv);
    sem_close(sem_cli);

    return 0;
}