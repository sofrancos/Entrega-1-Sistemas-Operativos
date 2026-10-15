#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#include "protocolo.h"
#include "menu.h"
#include "shm_comun.h"

/* Abre un semáforo que ya debe haber creado el servidor (sin O_CREAT) */
static sem_t *abrir_semaforo(const char *nombre)
{
    sem_t *s = sem_open(nombre, 0);
    if (s == SEM_FAILED)
        fprintf(stderr, "[Cliente SHM] sem_open %s: %s\n", nombre, strerror(errno));
    return s;
}

static void cerrar_semaforo(sem_t *s, const char *nombre)
{
    if (s != SEM_FAILED && sem_close(s) == -1)
        fprintf(stderr, "[Cliente SHM] sem_close %s: %s\n", nombre, strerror(errno));
}

/*
 * Espera las respuestas de una consulta hasta recibir la de cierre.
 * Devuelve 0 si la consulta terminó bien, -1 si se perdió el servidor.
 */
static int recibir_respuestas(shm_data_t *shm, sem_t *sem_res, sem_t *sem_ack)
{
    int mostrados = 0;

    for (;;) {
        /* Espera a que el servidor termine de escribir la respuesta */
        int r = esperar_semaforo(sem_res, &shm->pid_servidor, NULL);
        if (r == ESPERA_OTRO_MURIO) {
            fprintf(stderr, "[Cliente SHM] El servidor terminó inesperadamente.\n");
            return -1;
        }
        if (r != ESPERA_OK) {
            perror("[Cliente SHM] sem_timedwait");
            return -1;
        }

        /* Copia local de la respuesta y ACK: el servidor ya puede reutilizar la ranura */
        respuesta_t res = shm->respuesta;
        res.registro[sizeof res.registro - 1] = '\0';
        shm->estado = ESTADO_LEIDA;
        if (sem_post(sem_ack) == -1) {
            perror("[Cliente SHM] sem_post " SEM_ACK);
            return -1;
        }

        switch (res.tipo) {
        case RESP_REGISTRO:
            mostrados++;
            menu_mostrar_registro(res.registro);
            break;
        case RESP_FIN:
            menu_mostrar_cierre(RESP_FIN, NULL, mostrados, res.total);
            return 0;
        case RESP_SIN_RESULTADOS:
            menu_mostrar_cierre(RESP_SIN_RESULTADOS, NULL, 0, 0);
            return 0;
        default:
            menu_mostrar_cierre(res.tipo, res.registro, 0, 0);
            return 0;
        }
    }
}

int main(void)
{
    int estado = EXIT_FAILURE;
    int con_mutex = 0;
    sem_t *sem_sol = SEM_FAILED, *sem_res = SEM_FAILED;
    sem_t *sem_ack = SEM_FAILED, *sem_mutex = SEM_FAILED;
    shm_data_t *shm = MAP_FAILED;

    printf("[Cliente SHM] PID %d. Conectando con el servidor...\n", (int)getpid());

    /* Sin O_CREAT: si el servidor no creó la región, el cliente no la inventa */
    int shm_fd = shm_open(SHM_NOMBRE, O_RDWR, 0);
    if (shm_fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "[Cliente SHM] El servidor no está activo (no existe %s).\n", SHM_NOMBRE);
            fprintf(stderr, "Ejecuta primero ./bin/servidor_shm en otra terminal.\n");
        } else {
            perror("[Cliente SHM] shm_open " SHM_NOMBRE);
        }
        return EXIT_FAILURE;
    }

    /* Si el servidor aún no la dimensionó, acceder a ella produciría SIGBUS */
    struct stat st;
    if (fstat(shm_fd, &st) == -1) {
        perror("[Cliente SHM] fstat");
        goto salir;
    }
    if ((size_t)st.st_size < sizeof(shm_data_t)) {
        fprintf(stderr, "[Cliente SHM] El servidor aún se está iniciando; intenta de nuevo.\n");
        goto salir;
    }

    shm = mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("[Cliente SHM] mmap");
        goto salir;
    }

    /* La región puede ser un resto de un servidor terminado con SIGKILL */
    if (shm->pid_servidor <= 0 || shm->estado == ESTADO_CERRADO ||
        !proceso_vivo(shm->pid_servidor)) {
        fprintf(stderr, "[Cliente SHM] %s es de un servidor que ya no existe (PID %d).\n",
                SHM_NOMBRE, (int)shm->pid_servidor);
        fprintf(stderr, "Reinicia ./bin/servidor_shm o ejecuta 'make limpiar-ipc'.\n");
        goto salir;
    }

    sem_sol   = abrir_semaforo(SEM_SOLICITUD);
    sem_res   = abrir_semaforo(SEM_RESPUESTA);
    sem_ack   = abrir_semaforo(SEM_ACK);
    sem_mutex = abrir_semaforo(SEM_MUTEX);
    if (sem_sol == SEM_FAILED || sem_res == SEM_FAILED ||
        sem_ack == SEM_FAILED || sem_mutex == SEM_FAILED)
        goto salir;

    /* Exclusión mutua entre clientes: la región tiene una sola ranura */
    if (sem_trywait(sem_mutex) == -1) {
        if (errno == EAGAIN)
            fprintf(stderr, "[Cliente SHM] Ya hay otro cliente conectado al servidor.\n");
        else
            perror("[Cliente SHM] sem_trywait " SEM_MUTEX);
        goto salir;
    }
    con_mutex = 1;
    shm->pid_cliente = getpid();

    printf("[Cliente SHM] Conexión establecida (servidor PID %d).\n", (int)shm->pid_servidor);

    estado = EXIT_SUCCESS;
    for (;;) {
        solicitud_t sol;
        menu_leer_solicitud(&sol);

        /* Mientras el usuario escribía, el servidor pudo haber terminado */
        if (shm->estado == ESTADO_CERRADO || !proceso_vivo(shm->pid_servidor)) {
            fprintf(stderr, "[Cliente SHM] El servidor ya no está activo.\n");
            estado = EXIT_FAILURE;
            break;
        }

        shm->solicitud = sol;
        shm->estado = ESTADO_SOLICITUD;

        /* Avisar al servidor que la solicitud está completa */
        if (sem_post(sem_sol) == -1) {
            perror("[Cliente SHM] sem_post " SEM_SOLICITUD);
            estado = EXIT_FAILURE;
            break;
        }

        if (sol.tipo == MSG_SALIR) {
            printf("[Cliente SHM] Finalizando sesión...\n");
            break;
        }

        if (recibir_respuestas(shm, sem_res, sem_ack) == -1) {
            estado = EXIT_FAILURE;
            break;
        }
    }

salir:
    /* El cliente solo cierra sus referencias; el servidor hace shm_unlink y sem_unlink */
    if (con_mutex && sem_post(sem_mutex) == -1)
        perror("[Cliente SHM] sem_post " SEM_MUTEX);
    if (shm != MAP_FAILED && munmap(shm, sizeof(shm_data_t)) == -1)
        perror("[Cliente SHM] munmap");
    if (close(shm_fd) == -1)
        perror("[Cliente SHM] close");
    cerrar_semaforo(sem_sol, SEM_SOLICITUD);
    cerrar_semaforo(sem_res, SEM_RESPUESTA);
    cerrar_semaforo(sem_ack, SEM_ACK);
    cerrar_semaforo(sem_mutex, SEM_MUTEX);

    return estado;
}
