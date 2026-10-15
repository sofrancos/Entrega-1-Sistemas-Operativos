#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "protocolo.h"
#include "menu.h"

#define FIFO_REQ "/tmp/fifo_dino_req"
#define FIFO_RES "/tmp/fifo_dino_res"

/* Escribe exactamente n bytes (evita escrituras parciales). 0 = ok, -1 = error */
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

/* Lee exactamente n bytes. 1 = ok, 0 = el otro extremo cerro, -1 = error */
static int leer_todo(int fd, void *buf, size_t n)
{
    char *p = buf;
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, p + total, n - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;
        total += (size_t)r;
    }
    return 1;
}

int main(void)
{
    /* Si el servidor muere, write() devuelve EPIPE en vez de matar al cliente */
    signal(SIGPIPE, SIG_IGN);

    printf("[Cliente FIFO] Conectando con el servidor...\n");

    /* Bloquea hasta que el servidor abra el extremo de lectura */
    int fd_req = open(FIFO_REQ, O_WRONLY);
    if (fd_req == -1) {
        perror("[Cliente FIFO] No se pudo abrir la FIFO de solicitudes (¿servidor activo?)");
        fprintf(stderr, "Ejecuta primero ./bin/servidor_fifo en otra terminal.\n");
        return EXIT_FAILURE;
    }

    int fd_res = open(FIFO_RES, O_RDONLY);
    if (fd_res == -1) {
        perror("[Cliente FIFO] No se pudo abrir la FIFO de respuestas");
        close(fd_req);
        return EXIT_FAILURE;
    }

    printf("[Cliente FIFO] Conexión establecida.\n");

    int servidor_caido = 0;

    while (!servidor_caido) {
        solicitud_t sol;
        menu_leer_solicitud(&sol);          /* menu.c: muestra menú, valida y arma sol */

        if (escribir_todo(fd_req, &sol, sizeof sol) == -1) {
            perror("[Cliente FIFO] Error enviando la solicitud");
            break;
        }

        if (sol.tipo == MSG_SALIR) {
            printf("[Cliente FIFO] Finalizando sesión...\n");
            break;
        }

        respuesta_t res;
        int total = 0;
        int fin = 0;

        while (!fin) {
            int r = leer_todo(fd_res, &res, sizeof res);
            if (r <= 0) {
                fprintf(stderr, "[Cliente FIFO] Se perdió la conexión con el servidor.\n");
                servidor_caido = 1;
                break;
            }

            switch (res.tipo) {
            case RESP_REGISTRO:
                total++;
                menu_mostrar_registro(res.registro);
                break;
            case RESP_FIN:
                menu_mostrar_cierre(RESP_FIN, NULL, total);
                fin = 1;
                break;
            case RESP_SIN_RESULTADOS:
                menu_mostrar_cierre(RESP_SIN_RESULTADOS, NULL, 0);
                fin = 1;
                break;
            default: /* RESP_ERROR u otro tipo inesperado */
                menu_mostrar_cierre(res.tipo, res.registro, 0);
                fin = 1;
                break;
            }
        }
    }

    close(fd_req);
    close(fd_res);
    return servidor_caido ? EXIT_FAILURE : EXIT_SUCCESS;
}
