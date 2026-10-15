#include "shm_comun.h"

#include <errno.h>
#include <time.h>

int proceso_vivo(pid_t pid)
{
    if (pid <= 0)
        return 1;
    /* La señal 0 no se entrega: kill solo comprueba si el proceso existe */
    return kill(pid, 0) == 0 || errno != ESRCH;
}

int esperar_semaforo(sem_t *sem, const pid_t *pid_otro, volatile sig_atomic_t *terminar)
{
    for (;;) {
        struct timespec limite;
        if (clock_gettime(CLOCK_REALTIME, &limite) == -1)
            return ESPERA_ERROR;
        limite.tv_sec += INTERVALO_VIDA;

        if (sem_timedwait(sem, &limite) == 0)
            return ESPERA_OK;

        if (errno == EINTR) {
            if (terminar != NULL && *terminar)
                return ESPERA_SENAL;
            continue;
        }
        if (errno != ETIMEDOUT)
            return ESPERA_ERROR;

        /* Pasó el intervalo sin que nadie publicara el semáforo */
        if (pid_otro != NULL && !proceso_vivo(*pid_otro))
            return ESPERA_OTRO_MURIO;
    }
}
