#ifndef SHM_COMUN_H
#define SHM_COMUN_H

#include <signal.h>
#include <semaphore.h>
#include <sys/types.h>

#include "protocolo.h"

/* Estado de la comunicación (campo 'estado' de la región compartida) */
#define ESTADO_LIBRE      0   /* no hay ninguna operación en curso      */
#define ESTADO_SOLICITUD  1   /* el cliente escribió una solicitud      */
#define ESTADO_PROCESANDO 2   /* el servidor está buscando en el CSV    */
#define ESTADO_RESPUESTA  3   /* el servidor escribió una respuesta     */
#define ESTADO_LEIDA      4   /* el cliente ya copió la respuesta       */
#define ESTADO_CERRADO    5   /* el servidor terminó                    */

/* Estructura que reside en la memoria compartida */
typedef struct {
    int         estado;         /* ESTADO_...                                  */
    pid_t       pid_servidor;   /* lo escribe el servidor al crear la región   */
    pid_t       pid_cliente;    /* lo escribe el cliente al conectarse         */
    solicitud_t solicitud;      /* tipo, criterio y valor de la consulta       */
    respuesta_t respuesta;      /* una sola ranura: se usa con confirmación    */
} shm_data_t;

/* Segundos entre comprobaciones de que el otro proceso sigue vivo */
#define INTERVALO_VIDA 1

/* Resultados de esperar_semaforo */
#define ESPERA_OK          0
#define ESPERA_ERROR      -1
#define ESPERA_OTRO_MURIO  1
#define ESPERA_SENAL       2

/* 1 si el proceso existe (o si pid <= 0, porque aún no se conoce), 0 si no */
int proceso_vivo(pid_t pid);

/*
 * Hace sem_wait sobre 'sem' sin espera activa: el proceso queda dormido en el
 * kernel. Cada INTERVALO_VIDA segundos despierta para comprobar que el proceso
 * '*pid_otro' siga vivo, de modo que la muerte del otro no lo deja bloqueado
 * para siempre. Si 'terminar' no es NULL y se pone en 1 por una señal, vuelve.
 */
int esperar_semaforo(sem_t *sem, const pid_t *pid_otro, volatile sig_atomic_t *terminar);

#endif
