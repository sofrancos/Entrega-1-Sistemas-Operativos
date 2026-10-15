#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "protocolo.h"
#include "csv.h"

/*
 * La pone en 1 el manejador de SIGINT/SIGTERM. El manejador no hace nada más:
 * el ciclo principal la revisa y es quien cierra descriptores y borra las FIFO.
 */
static volatile sig_atomic_t terminar = 0;

static void manejar_senal(int sig)
{
    (void)sig;
    terminar = 1;
}

static int instalar_senales(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);

    /*
     * Sin SA_RESTART: si llega la señal mientras open() o read() están
     * bloqueados, la llamada vuelve con EINTR y el ciclo puede terminar.
     */
    sa.sa_handler = manejar_senal;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1)
        return -1;

    /* Si el cliente muere, write() devuelve EPIPE en vez de matar al servidor */
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        return -1;
    return 0;
}

/* Escribe exactamente n bytes. 0 = ok, -1 = error (EPIPE si el cliente cerró) */
static int escribir_todo(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR && !terminar) continue;
            return -1;
        }
        p += w;
        n -= (size_t)w;
    }
    return 0;
}

/* Lee exactamente n bytes. 1 = ok, 0 = EOF antes de leer nada, -1 = error */
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
        if (r == 0) return (total == 0) ? 0 : -1;   /* EOF a mitad de mensaje = error */
        total += (size_t)r;
    }
    return 1;
}

/* Arma una respuesta_t y la envía completa. 0 = ok, -1 = error */
static int enviar_respuesta(int fd, int tipo, int total, const char *texto)
{
    respuesta_t res;
    memset(&res, 0, sizeof res);
    res.tipo = tipo;
    res.total = total;
    if (texto != NULL)
        snprintf(res.registro, sizeof res.registro, "%s", texto);
    return escribir_todo(fd, &res, sizeof res);
}

/* Borra las FIFO; que no existan (ENOENT) no es un error */
static void limpiar(void)
{
    if (unlink(FIFO_SOLICITUD) == -1 && errno != ENOENT)
        perror("[Servidor FIFO] unlink " FIFO_SOLICITUD);
    if (unlink(FIFO_RESPUESTA) == -1 && errno != ENOENT)
        perror("[Servidor FIFO] unlink " FIFO_RESPUESTA);
}

/* Informa por qué falló un open() bloqueante: señal recibida o error real */
static int fallo_open(const char *que)
{
    if (errno == EINTR && terminar) {
        printf("[Servidor FIFO] Señal recibida mientras esperaba al cliente.\n");
        return EXIT_SUCCESS;
    }
    perror(que);
    return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
    const char *ruta_csv = (argc > 1) ? argv[1] : RUTA_CSV_DEFECTO;
    int estado = EXIT_SUCCESS;

    if (instalar_senales() == -1) {
        perror("[Servidor FIFO] sigaction");
        return EXIT_FAILURE;
    }

    if (access(ruta_csv, R_OK) == -1)
        fprintf(stderr, "[Servidor FIFO] Aviso: no se puede leer %s (%s). "
                "Las consultas responderán con error.\n", ruta_csv, strerror(errno));

    /* Eliminar restos de ejecuciones anteriores */
    limpiar();

    /* Crear las tuberías nombradas (FIFO) */
    if (mkfifo(FIFO_SOLICITUD, 0666) == -1) {
        perror("[Servidor FIFO] mkfifo " FIFO_SOLICITUD);
        return EXIT_FAILURE;
    }
    if (mkfifo(FIFO_RESPUESTA, 0666) == -1) {
        perror("[Servidor FIFO] mkfifo " FIFO_RESPUESTA);
        limpiar();
        return EXIT_FAILURE;
    }

    printf("[Servidor FIFO] PID %d. CSV: %s\n", (int)getpid(), ruta_csv);
    printf("[Servidor FIFO] Esperando al cliente...\n");

    /* open() se bloquea hasta que el cliente abra el otro extremo */
    int fd_req = open(FIFO_SOLICITUD, O_RDONLY);
    if (fd_req == -1) {
        estado = fallo_open("[Servidor FIFO] open " FIFO_SOLICITUD);
        limpiar();
        return estado;
    }

    int fd_res = open(FIFO_RESPUESTA, O_WRONLY);
    if (fd_res == -1) {
        estado = fallo_open("[Servidor FIFO] open " FIFO_RESPUESTA);
        close(fd_req);
        limpiar();
        return estado;
    }

    printf("[Servidor FIFO] Cliente conectado. Listo para consultas.\n");

    solicitud_t sol;
    static char resultados[MAX_RESULTADOS][TAM_REGISTRO];

    while (!terminar) {
        int r = leer_todo(fd_req, &sol, sizeof sol);
        if (r == 0) {
            printf("[Servidor FIFO] El cliente cerró la FIFO (EOF).\n");
            break;
        }
        if (r < 0) {
            if (!terminar) {
                perror("[Servidor FIFO] Error leyendo la solicitud");
                estado = EXIT_FAILURE;
            }
            break;
        }

        /* Si el cliente envía la orden de salir, rompe el ciclo */
        if (sol.tipo == MSG_SALIR) {
            printf("[Servidor FIFO] Orden de salida recibida.\n");
            break;
        }

        int ok;
        if (sol.tipo != MSG_CONSULTA) {
            /* Se responde igual, para que el cliente no quede esperando */
            fprintf(stderr, "[Servidor FIFO] Tipo de solicitud desconocido: %d\n", sol.tipo);
            ok = enviar_respuesta(fd_res, RESP_ERROR, 0, "Solicitud no válida.");
        } else {
            sol.valor[sizeof sol.valor - 1] = '\0';
            printf("[Servidor FIFO] Consulta -> criterio %d, valor '%s'\n", sol.criterio, sol.valor);

            int total = 0;
            int encontrados = csv_buscar(ruta_csv, sol.criterio, sol.valor,
                                         resultados, MAX_RESULTADOS, &total);

            if (encontrados < 0) {
                ok = enviar_respuesta(fd_res, RESP_ERROR, 0, "Error al leer el archivo CSV.");
            } else if (encontrados == 0) {
                ok = enviar_respuesta(fd_res, RESP_SIN_RESULTADOS, 0, NULL);
            } else {
                ok = 0;
                for (int i = 0; i < encontrados && ok == 0; i++)
                    ok = enviar_respuesta(fd_res, RESP_REGISTRO, 0, resultados[i]);
                if (ok == 0)
                    ok = enviar_respuesta(fd_res, RESP_FIN, total, NULL);
            }
        }

        if (ok == -1) {
            if (!terminar) {
                perror("[Servidor FIFO] Error enviando la respuesta (¿cliente desconectado?)");
                estado = EXIT_FAILURE;
            }
            break;
        }
    }

    if (terminar)
        printf("[Servidor FIFO] Señal de terminación recibida.\n");

    /* Limpieza de descriptores y FIFO, también cuando se sale por señal o error */
    if (close(fd_req) == -1) perror("[Servidor FIFO] close fd_req");
    if (close(fd_res) == -1) perror("[Servidor FIFO] close fd_res");
    limpiar();
    printf("[Servidor FIFO] Finalizado y FIFO eliminadas.\n");

    return estado;
}
