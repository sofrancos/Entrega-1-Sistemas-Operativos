#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "protocolo.h"
#include "csv.h"

#define FIFO_REQ "/tmp/fifo_dino_req"
#define FIFO_RES "/tmp/fifo_dino_res"

static volatile sig_atomic_t terminar = 0;

/* Solo marca la bandera: es lo único seguro dentro de un manejador */
static void manejar_senal(int sig)
{
    (void)sig;
    terminar = 1;
}

static int escribir_todo(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += w;
        n -= (size_t)w;
    }
    return 0;
}

/* 1 = ok, 0 = EOF (el cliente cerró), -1 = error/interrumpido por señal */
static int leer_todo(int fd, void *buf, size_t n)
{
    char *p = buf;
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, p + total, n - total);
        if (r < 0) {
            if (errno == EINTR && !terminar) continue;
            return -1;
        }
        if (r == 0) return 0;
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
    /* sigaction SIN SA_RESTART: open()/read() bloqueados salen con EINTR al llegar Ctrl+C */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = manejar_senal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);   /* si el cliente muere, write() devuelve EPIPE */

    /* Eliminar restos de una ejecución anterior y crear las FIFO limpias */
    limpiar();
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

    /* Bloquea hasta que el cliente abra el extremo de escritura */
    int fd_req = open(FIFO_REQ, O_RDONLY);
    if (fd_req == -1) {
        if (errno != EINTR) perror("[Servidor FIFO] open FIFO_REQ");
        limpiar();
        return terminar ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    int fd_res = open(FIFO_RES, O_WRONLY);
    if (fd_res == -1) {
        if (errno != EINTR) perror("[Servidor FIFO] open FIFO_RES");
        close(fd_req);
        limpiar();
        return terminar ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    printf("[Servidor FIFO] Cliente conectado. Listo para consultas.\n");

    solicitud_t sol;
    static char resultados[MAX_RESULTADOS][TAM_REGISTRO];
    int cliente_activo = 1;

    while (!terminar && cliente_activo) {
        int r = leer_todo(fd_req, &sol, sizeof sol);
        if (r == 0) {
            printf("[Servidor FIFO] El cliente cerró la conexión sin avisar.\n");
            break;
        }
        if (r < 0) {
            if (!terminar) perror("[Servidor FIFO] read");
            break;
        }

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
            if (escribir_todo(fd_res, &res, sizeof res) == -1) cliente_activo = 0;
        } else if (encontrados == 0) {
            res.tipo = RESP_SIN_RESULTADOS;
            if (escribir_todo(fd_res, &res, sizeof res) == -1) cliente_activo = 0;
        } else {
            for (int i = 0; i < encontrados && cliente_activo; i++) {
                memset(&res, 0, sizeof res);
                res.tipo = RESP_REGISTRO;
                snprintf(res.registro, sizeof res.registro, "%s", resultados[i]);
                if (escribir_todo(fd_res, &res, sizeof res) == -1) cliente_activo = 0;
            }
            if (cliente_activo) {
                memset(&res, 0, sizeof res);
                res.tipo = RESP_FIN;
                if (escribir_todo(fd_res, &res, sizeof res) == -1) cliente_activo = 0;
            }
        }

        if (!cliente_activo)
            fprintf(stderr, "[Servidor FIFO] No se pudo escribir al cliente (¿terminó?).\n");
    }

    close(fd_req);
    close(fd_res);
    limpiar();
    printf("[Servidor FIFO] Finalizado y FIFO eliminadas.\n");
    return EXIT_SUCCESS;
}
