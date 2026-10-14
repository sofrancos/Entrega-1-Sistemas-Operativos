#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include "../comun/protocolo.h"

#define SHM_NAME "/dinos_shm"
#define SEM_SRV_NAME "/dinos_sem_srv"
#define SEM_CLI_NAME "/dinos_sem_cli"

typedef struct {
    solicitud_t solicitud;
    respuesta_t respuesta;
} shm_data_t;

void mostrar_menu() {
    printf("\n--- Busqueda en Dataset (SHM) ---\n");
    printf("1. Nombre\n");
    printf("2. Familia\n");
    printf("3. Pais\n");
    printf("4. Edad (Rango Ej: 65.5)\n");
    printf("5. Formacion Geologica\n");
    printf("6. Dieta\n");
    printf("7. Salir\n");
    printf("Seleccione un criterio: ");
}

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

    int opcion;
    char buffer[TAM_VALORUSUARIO];

    while (1) {
        mostrar_menu();
        if (scanf("%d", &opcion) != 1) {
            while(getchar() != '\n'); // Limpiar buffer de basura
            continue;
        }
        while(getchar() != '\n'); // Limpiar salto de línea

        if (opcion == 7) {
            shm_ptr->solicitud.tipo = MSG_SALIR;
            sem_post(sem_srv); // Despierta al servidor para que limpie y termine
            break;
        }

        if (opcion < 1 || opcion > 6) {
            printf("Opcion invalida.\n");
            continue;
        }

        printf("Ingrese el valor a buscar: ");
        fgets(buffer, TAM_VALORUSUARIO, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Quitar el salto de línea

        // 1. Preparar la estructura en memoria compartida
        shm_ptr->solicitud.tipo = MSG_CONSULTA;
        shm_ptr->solicitud.criterio = opcion;
        strncpy(shm_ptr->solicitud.valor, buffer, TAM_VALORUSUARIO);

        // 2. Avisar al servidor que la consulta está lista
        sem_post(sem_srv);

        // 3. Recibir e imprimir respuestas
        int num_resultados = 0;
        while (1) {
            sem_wait(sem_cli); // Espera a que el servidor escriba la respuesta

            if (shm_ptr->respuesta.tipo == RESP_REGISTRO) {
                num_resultados++;
                printf("[%d] %s\n", num_resultados, shm_ptr->respuesta.registro);
                
                // IMPORTANTE: Enviar ACK al servidor para que sepa que ya extrajimos los datos 
                // y puede sobrescribir con el siguiente registro.
                sem_post(sem_srv); 
                
            } else if (shm_ptr->respuesta.tipo == RESP_SIN_RESULTADOS) {
                printf("\nNo se encontraron resultados para la busqueda.\n");
                break;
            } else if (shm_ptr->respuesta.tipo == RESP_FIN) {
                printf("\n--- Fin de los resultados (%d encontrados) ---\n", num_resultados);
                break;
            } else if (shm_ptr->respuesta.tipo == RESP_ERROR) {
                printf("\n[Error del Servidor] %s\n", shm_ptr->respuesta.registro);
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