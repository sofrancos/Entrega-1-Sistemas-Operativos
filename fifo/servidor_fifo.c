#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "protocolo.h"
#include "csv.h"

#define FIFO_REQ "/tmp/fifo_dino_req"
#define FIFO_RES "/tmp/fifo_dino_res"

static int escribir_todo(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) return -1;
        p += w;
        n -= (size_t)w;
    }
    return 0;
}

static int leer_todo(int fd, void *buf, size_t n)
{
    char *p = buf;
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, p + total, n - total);
        if (r <= 0) return (total == 0) ? 0 : -1;
        total += (size_t)r;
    }
    return 1;
}

static void limpiar(void)
{
    unlink(FIFO_REQ);
    unlink(FIFO_RES);
}

int main(void)
{
    /* Eliminar restos de ejecuciones anteriores */
    limpiar();

    /* Crear las tuberías nombradas (FIFOs) */
    if (mkfifo(FIFO_REQ, 0666) == -1) {
        perror("[Servidor FIFO] mkfifo FIFO_REQ");
        return EXIT_FAILURE;
    }
    if (mkfifo(FIFO_RES, 0666) == -1) {
        perror("[Servidor FIFO] mkfifo FIFO_RES");
        limpiar();
        return EXIT_FAILURE;
    }

    printf("[Servidor FIFO] Esperando al cliente...\n");

    /* Abrir los extremos de las FIFOs */
    int fd_req = open(FIFO_REQ, O_RDONLY);
    if (fd_req == -1) {
        perror("[Servidor FIFO] open FIFO_REQ");
        limpiar();
        return EXIT_FAILURE;
    }

    int fd_res = open(FIFO_RES, O_WRONLY);
    if (fd_res == -1) {
        perror("[Servidor FIFO] open FIFO_RES");
        close(fd_req);
        limpiar();
        return EXIT_FAILURE;
    }

    printf("[Servidor FIFO] Cliente conectado. Listo para consultas.\n");

    solicitud_t sol;
    static char resultados[MAX_RESULTADOS][TAM_REGISTRO];

    while (1) {
        int r = leer_todo(fd_req, &sol, sizeof sol);
        if (r <= 0) {
            printf("[Servidor FIFO] Cliente desconectado o error de lectura.\n");
            break;
        }

        /* Si el cliente envía la orden de salir, rompe el ciclo */
        if (sol.tipo == MSG_SALIR) {
            printf("[Servidor FIFO] Orden de salida recibida.\n");
            break;
        }

        if (sol.tipo != MSG_CONSULTA) continue;

        sol.valor[sizeof sol.valor - 1] = '\0';
        printf("[Servidor FIFO] Consulta -> criterio %d, valor '%s'\n", sol.criterio, sol.valor);

        int encontrados = csv_buscar("data/dataset.csv", sol.criterio, sol.valor,
                                     resultados, MAX_RESULTADOS);

        respuesta_t res;
        memset(&res, 0, sizeof res);

        if (encontrados < 0) {
            res.tipo = RESP_ERROR;
            snprintf(res.registro, sizeof res.registro, "Error al leer el archivo CSV.");
            escribir_todo(fd_res, &res, sizeof res);
        } else if (encontrados == 0) {
            res.tipo = RESP_SIN_RESULTADOS;
            escribir_todo(fd_res, &res, sizeof res);
        } else {
            for (int i = 0; i < encontrados; i++) {
                memset(&res, 0, sizeof res);
                res.tipo = RESP_REGISTRO;
                snprintf(res.registro, sizeof res.registro, "%s", resultados[i]);
                if (escribir_todo(fd_res, &res, sizeof res) == -1) break;
            }
            memset(&res, 0, sizeof res);
            res.tipo = RESP_FIN;
            escribir_todo(fd_res, &res, sizeof res);
        }
    }

    /* Limpieza de descriptores y FIFOs al terminar normalmente */
    close(fd_req);
    close(fd_res);
    limpiar();
    printf("[Servidor FIFO] Finalizado y FIFOs eliminadas.\n");

    return EXIT_SUCCESS;
}
