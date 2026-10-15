#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <string.h>
#include <errno.h>

#include "protocolo.h"
#include "menu.h"

/* Escribe exactamente n bytes. 0 = ok, -1 = error (EPIPE si el servidor cerró) */
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

/* Lee exactamente n bytes. 1 = ok, 0 = EOF, -1 = error */
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

/* Abre la FIFO de solicitudes sin quedar bloqueado si no hay servidor */
static int conectar_solicitudes(void)
{
    /*
     * O_NONBLOCK: si no hay ningún proceso con el extremo de lectura abierto
     * (servidor apagado, o una FIFO que quedó de una ejecución terminada con
     * SIGKILL), open() falla con ENXIO en lugar de bloquearse para siempre.
     */
    int fd = open(FIFO_SOLICITUD, O_WRONLY | O_NONBLOCK);
    if (fd == -1) {
        if (errno == ENOENT || errno == ENXIO) {
            fprintf(stderr, "[Cliente FIFO] El servidor no está activo (%s).\n", strerror(errno));
            fprintf(stderr, "Ejecuta primero ./bin/servidor_fifo en otra terminal.\n");
        } else {
            perror("[Cliente FIFO] No se pudo abrir " FIFO_SOLICITUD);
        }
        return -1;
    }

    /* Ya conectados: se vuelve al modo bloqueante para que write() espere si la FIFO se llena */
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        perror("[Cliente FIFO] fcntl");
        close(fd);
        return -1;
    }

    /*
     * El servidor atiende a un solo cliente. Un candado exclusivo sobre la FIFO
     * impide que un segundo cliente mezcle sus mensajes con los del primero.
     * El kernel lo libera solo cuando este proceso cierra el descriptor o muere.
     */
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK)
            fprintf(stderr, "[Cliente FIFO] Ya hay otro cliente conectado al servidor.\n");
        else
            perror("[Cliente FIFO] flock");
        close(fd);
        return -1;
    }
    return fd;
}

int main(void)
{
    /* Si el servidor muere, write() devuelve EPIPE en vez de matar al cliente */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("[Cliente FIFO] sigaction");
        return EXIT_FAILURE;
    }

    printf("[Cliente FIFO] PID %d. Conectando con el servidor...\n", (int)getpid());

    int fd_req = conectar_solicitudes();
    if (fd_req == -1)
        return EXIT_FAILURE;

    /* Bloquea hasta que el servidor abra el extremo de escritura */
    int fd_res = open(FIFO_RESPUESTA, O_RDONLY);
    if (fd_res == -1) {
        perror("[Cliente FIFO] No se pudo abrir " FIFO_RESPUESTA);
        close(fd_req);
        return EXIT_FAILURE;
    }

    printf("[Cliente FIFO] Conexión establecida.\n");

    int servidor_caido = 0;

    while (!servidor_caido) {
        solicitud_t sol;
        menu_leer_solicitud(&sol);

        if (escribir_todo(fd_req, &sol, sizeof sol) == -1) {
            if (errno == EPIPE)
                fprintf(stderr, "[Cliente FIFO] El servidor ya no está escuchando.\n");
            else
                perror("[Cliente FIFO] Error enviando la solicitud");
            servidor_caido = 1;
            break;
        }

        if (sol.tipo == MSG_SALIR) {
            printf("[Cliente FIFO] Finalizando sesión...\n");
            break;
        }

        respuesta_t res;
        int mostrados = 0;
        int fin = 0;

        while (!fin) {
            int r = leer_todo(fd_res, &res, sizeof res);
            if (r <= 0) {
                if (r < 0)
                    perror("[Cliente FIFO] Error leyendo la respuesta");
                fprintf(stderr, "[Cliente FIFO] Se perdió la conexión con el servidor.\n");
                servidor_caido = 1;
                break;
            }

            switch (res.tipo) {
            case RESP_REGISTRO:
                res.registro[sizeof res.registro - 1] = '\0';
                mostrados++;
                menu_mostrar_registro(res.registro);
                break;
            case RESP_FIN:
                menu_mostrar_cierre(RESP_FIN, NULL, mostrados, res.total);
                fin = 1;
                break;
            case RESP_SIN_RESULTADOS:
                menu_mostrar_cierre(RESP_SIN_RESULTADOS, NULL, 0, 0);
                fin = 1;
                break;
            default:
                res.registro[sizeof res.registro - 1] = '\0';
                menu_mostrar_cierre(res.tipo, res.registro, 0, 0);
                fin = 1;
                break;
            }
        }
    }

    if (close(fd_req) == -1) perror("[Cliente FIFO] close fd_req");
    if (close(fd_res) == -1) perror("[Cliente FIFO] close fd_res");
    return servidor_caido ? EXIT_FAILURE : EXIT_SUCCESS;
}
