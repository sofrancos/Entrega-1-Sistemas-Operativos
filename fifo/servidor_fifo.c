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
    (void)sig; // Evita la advertencia de parámetro no usado
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
    char resultados[MAX_RESULTADOS][TAM_REGISTRO];

    while (read(fd_req, &sol, sizeof(solicitud_t)) > 0) {
        if (sol.tipo == MSG_SALIR) {
            printf("[Servidor FIFO] Recibida orden de salida por parte del cliente.\n");
            break;
        }

        if (sol.tipo == MSG_CONSULTA) {
            printf("[Servidor FIFO] Consulta recibida -> Criterio: %d, Valor de búsqueda: '%s'\n", sol.criterio, sol.valor);

            // Búsqueda secuencial en el CSV utilizando el módulo comun/csv.c
            int encontrados = csv_buscar("data/dataset.csv", sol.criterio, sol.valor, resultados, MAX_RESULTADOS);

            if (encontrados < 0) {
                respuesta_t res;
                res.tipo = RESP_ERROR;
                snprintf(res.registro, sizeof(res.registro), "Error al leer el archivo CSV.");
                write(fd_res, &res, sizeof(respuesta_t));
            } else if (encontrados == 0) {
                respuesta_t res;
                res.tipo = RESP_SIN_RESULTADOS;
                write(fd_res, &res, sizeof(respuesta_t));
            } else {
                // Enviar cada uno de los registros encontrados
                for (int i = 0; i < encontrados; i++) {
                    respuesta_t res;
                    res.tipo = RESP_REGISTRO;
                    strncpy(res.registro, resultados[i], TAM_REGISTRO - 1);
                    res.registro[TAM_REGISTRO - 1] = '\0';
                    write(fd_res, &res, sizeof(respuesta_t));
                }
                // Enviar mensaje de fin de transmisión
                respuesta_t res;
                res.tipo = RESP_FIN;
                write(fd_res, &res, sizeof(respuesta_t));
            }
        }
    }

    close(fd_req);
    close(fd_res);
    unlink(FIFO_REQ);
    unlink(FIFO_RES);
    printf("[Servidor FIFO] Servidor finalizado y recursos liberados.\n");
    return 0;
}
