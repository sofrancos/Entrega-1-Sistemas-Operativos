#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "protocolo.h"
#include "csv.h"

#define FIFO_REQ "/tmp/fifo_dino_req"
#define FIFO_RES "/tmp/fifo_dino_res"

// Manejador para limpiar las FIFO si el servidor finaliza con CTRL+C
void manejar_sigint(int sig) {
    unlink(FIFO_REQ);
    unlink(FIFO_RES);
    printf("\n[Servidor FIFO] Tuberías nombradas eliminadas y servidor cerrado correctamente.\n");
    exit(0);
}

int main() {
    // Registrar el manejador de la señal SIGINT (CTRL+C)
    signal(SIGINT, manejar_sigint);

    // 1. Crear las tuberías nombradas si no existen
    if (mkfifo(FIFO_REQ, 0666) == -1) {
        // Ignorar si ya existe
    }
    if (mkfifo(FIFO_RES, 0666) == -1) {
        // Ignorar si ya existe
    }

    printf("[Servidor FIFO] Servidor iniciado. Esperando conexión del cliente...\n");

    // 2. Abrir la FIFO de solicitudes en modo lectura
    int fd_req = open(FIFO_REQ, O_RDONLY);
    if (fd_req == -1) {
        perror("[Servidor FIFO Error] No se pudo abrir FIFO_REQ para lectura");
        exit(EXIT_FAILURE);
    }

    // 3. Abrir la FIFO de respuestas en modo escritura
    int fd_res = open(FIFO_RES, O_WRONLY);
    if (fd_res == -1) {
        perror("[Servidor FIFO Error] No se pudo abrir FIFO_RES para escritura");
        close(fd_req);
        exit(EXIT_FAILURE);
    }

    printf("[Servidor FIFO] Cliente conectado exitosamente. Listo para procesar consultas.\n");

    solicitud_t sol;
    while (read(fd_req, &sol, sizeof(solicitud_t)) > 0) {
        if (sol.tipo == MSG_SALIR) {
            printf("[Servidor FIFO] Recibida orden de salida por parte del cliente.\n");
            break;
        }

        if (sol.tipo == MSG_CONSULTA) {
            printf("[Servidor FIFO] Consulta recibida -> Criterio: %d, Valor de búsqueda: '%s'\n", sol.criterio, sol.valor);

            // Búsqueda secuencial en el CSV y envío directo de respuestas por la FIFO
            csv_buscar_y_responder(sol.criterio, sol.valor, fd_res);
        }
    }

    close(fd_req);
    close(fd_res);
    unlink(FIFO_REQ);
    unlink(FIFO_RES);
    printf("[Servidor FIFO] Servidor finalizado y recursos liberados.\n");
    return 0;
}
