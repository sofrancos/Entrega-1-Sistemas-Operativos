#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#include "protocolo.h"
#include "csv.h"
#include "shm_comun.h"

/*
 * La pone en 1 el manejador de SIGINT/SIGTERM. El ciclo principal la revisa
 * y es quien libera la memoria compartida y los semáforos.
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
    /* Sin SA_RESTART: un sem_timedwait bloqueado vuelve con EINTR */
    sa.sa_handler = manejar_senal;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1)
        return -1;
    return 0;
}

/* Borra los nombres de los recursos IPC; que no existan (ENOENT) no es un error */
static void desvincular_recursos(void)
{
    const char *semaforos[] = { SEM_SOLICITUD, SEM_RESPUESTA, SEM_ACK, SEM_MUTEX };

    if (shm_unlink(SHM_NOMBRE) == -1 && errno != ENOENT)
        perror("[Servidor SHM] shm_unlink " SHM_NOMBRE);
    for (size_t i = 0; i < sizeof semaforos / sizeof semaforos[0]; i++)
        if (sem_unlink(semaforos[i]) == -1 && errno != ENOENT)
            fprintf(stderr, "[Servidor SHM] sem_unlink %s: %s\n", semaforos[i], strerror(errno));
}

/* O_EXCL: falla si el semáforo ya existe, así nunca se hereda un valor viejo */
static sem_t *crear_semaforo(const char *nombre, unsigned int valor)
{
    sem_t *s = sem_open(nombre, O_CREAT | O_EXCL, 0666, valor);
    if (s == SEM_FAILED)
        fprintf(stderr, "[Servidor SHM] sem_open %s: %s\n", nombre, strerror(errno));
    return s;
}

static void cerrar_semaforo(sem_t *s, const char *nombre)
{
    if (s != SEM_FAILED && sem_close(s) == -1)
        fprintf(stderr, "[Servidor SHM] sem_close %s: %s\n", nombre, strerror(errno));
}

/*
 * Escribe una respuesta en la región compartida, despierta al cliente y espera
 * su confirmación (ACK). La región tiene una sola ranura de respuesta: sin el
 * ACK, la siguiente respuesta podría sobrescribir esta antes de ser leída.
 * Devuelve un código ESPERA_...
 */
static int publicar_respuesta(shm_data_t *shm, sem_t *sem_res, sem_t *sem_ack,
                              int tipo, int total, const char *texto)
{
    memset(&shm->respuesta, 0, sizeof shm->respuesta);
    shm->respuesta.tipo = tipo;
    shm->respuesta.total = total;
    if (texto != NULL)
        snprintf(shm->respuesta.registro, sizeof shm->respuesta.registro, "%s", texto);
    shm->estado = ESTADO_RESPUESTA;

    if (sem_post(sem_res) == -1) {
        perror("[Servidor SHM] sem_post " SEM_RESPUESTA);
        return ESPERA_ERROR;
    }
    return esperar_semaforo(sem_ack, &shm->pid_cliente, &terminar);
}

/* Atiende una solicitud ya copiada de la región compartida */
static int atender(shm_data_t *shm, sem_t *sem_res, sem_t *sem_ack,
                   const solicitud_t *sol, const char *ruta_csv)
{
    static char resultados[MAX_RESULTADOS][TAM_REGISTRO];

    if (sol->tipo != MSG_CONSULTA) {
        /* Se responde igual, para que el cliente no quede esperando */
        fprintf(stderr, "[Servidor SHM] Tipo de solicitud desconocido: %d\n", sol->tipo);
        return publicar_respuesta(shm, sem_res, sem_ack, RESP_ERROR, 0, "Solicitud no válida.");
    }

    printf("[Servidor SHM] Consulta -> criterio %d, valor '%s'\n", sol->criterio, sol->valor);

    int total = 0;
    int encontrados = csv_buscar(ruta_csv, sol->criterio, sol->valor,
                                 resultados, MAX_RESULTADOS, &total);

    if (encontrados < 0)
        return publicar_respuesta(shm, sem_res, sem_ack, RESP_ERROR, 0, "Error al leer el archivo CSV.");
    if (encontrados == 0)
        return publicar_respuesta(shm, sem_res, sem_ack, RESP_SIN_RESULTADOS, 0, NULL);

    /* Enviar resultados uno a uno; cada uno espera el ACK del cliente */
    for (int i = 0; i < encontrados; i++) {
        int r = publicar_respuesta(shm, sem_res, sem_ack, RESP_REGISTRO, 0, resultados[i]);
        if (r != ESPERA_OK)
            return r;
    }
    return publicar_respuesta(shm, sem_res, sem_ack, RESP_FIN, total, NULL);
}

int main(int argc, char *argv[])
{
    const char *ruta_csv = (argc > 1) ? argv[1] : RUTA_CSV_DEFECTO;
    int estado = EXIT_FAILURE;
    int r = ESPERA_OK;

    sem_t *sem_sol = SEM_FAILED, *sem_res = SEM_FAILED;
    sem_t *sem_ack = SEM_FAILED, *sem_mutex = SEM_FAILED;
    shm_data_t *shm = MAP_FAILED;
    int shm_fd = -1;

    if (instalar_senales() == -1) {
        perror("[Servidor SHM] sigaction");
        return EXIT_FAILURE;
    }

    if (access(ruta_csv, R_OK) == -1)
        fprintf(stderr, "[Servidor SHM] Aviso: no se puede leer %s (%s). "
                "Las consultas responderán con error.\n", ruta_csv, strerror(errno));

    printf("[Servidor SHM] Iniciando servidor...\n");

    /*
     * Si una ejecución anterior terminó de forma abrupta, sus recursos siguen
     * en /dev/shm con valores viejos. Se eliminan antes de crear los nuevos.
     */
    desvincular_recursos();

    /*
     * 1. Semáforos. Se crean antes que la memoria compartida porque el cliente
     *    usa la existencia de la región para saber que el servidor está listo.
     */
    sem_sol   = crear_semaforo(SEM_SOLICITUD, 0);
    sem_res   = crear_semaforo(SEM_RESPUESTA, 0);
    sem_ack   = crear_semaforo(SEM_ACK, 0);
    sem_mutex = crear_semaforo(SEM_MUTEX, 1);
    if (sem_sol == SEM_FAILED || sem_res == SEM_FAILED ||
        sem_ack == SEM_FAILED || sem_mutex == SEM_FAILED)
        goto salir;

    /* 2. Crear y dimensionar la memoria compartida */
    shm_fd = shm_open(SHM_NOMBRE, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[Servidor SHM] shm_open " SHM_NOMBRE);
        goto salir;
    }
    if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) {
        perror("[Servidor SHM] ftruncate");
        goto salir;
    }
    shm = mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("[Servidor SHM] mmap");
        goto salir;
    }

    memset(shm, 0, sizeof *shm);
    shm->estado = ESTADO_LIBRE;
    shm->pid_servidor = getpid();   /* a partir de aquí el cliente puede conectarse */

    printf("[Servidor SHM] PID %d. CSV: %s\n", (int)getpid(), ruta_csv);
    printf("[Servidor SHM] Esperando solicitudes...\n");

    estado = EXIT_SUCCESS;
    while (!terminar) {
        /* Esperar a que el cliente escriba una solicitud (sin espera activa) */
        r = esperar_semaforo(sem_sol, &shm->pid_cliente, &terminar);
        if (r != ESPERA_OK)
            break;

        /* Copia local: el servidor trabaja sobre ella y asegura el '\0' final */
        solicitud_t sol = shm->solicitud;
        sol.valor[sizeof sol.valor - 1] = '\0';

        if (sol.tipo == MSG_SALIR) {
            printf("[Servidor SHM] Orden de salida recibida.\n");
            break;
        }

        shm->estado = ESTADO_PROCESANDO;
        r = atender(shm, sem_res, sem_ack, &sol, ruta_csv);
        if (r != ESPERA_OK)
            break;
        shm->estado = ESTADO_LIBRE;
    }

    if (r == ESPERA_OTRO_MURIO) {
        printf("[Servidor SHM] El cliente (PID %d) terminó inesperadamente.\n",
               (int)shm->pid_cliente);
    } else if (r == ESPERA_SENAL || terminar) {
        printf("[Servidor SHM] Señal de terminación recibida.\n");
    } else if (r == ESPERA_ERROR) {
        perror("[Servidor SHM] sem_timedwait");
        estado = EXIT_FAILURE;
    }

salir:
    /* 3. Liberación de recursos del sistema operativo, también tras un error */
    if (shm != MAP_FAILED) {
        shm->estado = ESTADO_CERRADO;
        if (munmap(shm, sizeof(shm_data_t)) == -1)
            perror("[Servidor SHM] munmap");
    }
    if (shm_fd != -1 && close(shm_fd) == -1)
        perror("[Servidor SHM] close");

    cerrar_semaforo(sem_sol, SEM_SOLICITUD);
    cerrar_semaforo(sem_res, SEM_RESPUESTA);
    cerrar_semaforo(sem_ack, SEM_ACK);
    cerrar_semaforo(sem_mutex, SEM_MUTEX);
    desvincular_recursos();   /* shm_unlink y sem_unlink */

    printf("[Servidor SHM] Recursos IPC liberados. Apagado %s.\n",
           estado == EXIT_SUCCESS ? "correcto" : "con errores");
    return estado;
}
