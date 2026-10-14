#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "protocolo.h"
#include "menu.h"

#define FIFO_REQ "/tmp/fifo_dino_req"
#define FIFO_RES "/tmp/fifo_dino_res"

int main() {
    printf("[Cliente FIFO] Conectando con el servidor FIFO...\n");

    // 1. Abrir la FIFO de solicitudes en modo escritura
    int fd_req = open(FIFO_REQ, O_WRONLY);
    if (fd_req == -1) {
        perror("[Cliente FIFO Error] El servidor no está activo (no se encontró /tmp/fifo_dino_req)");
        printf("Asegúrate de ejecutar primero './bin/servidor_fifo'\n");
        exit(EXIT_FAILURE);
    }

    // 2. Abrir la FIFO de respuestas en modo lectura
    int fd_res = open(FIFO_RES, O_RDONLY);
    if (fd_res == -1) {
        perror("[Cliente FIFO Error] No se pudo abrir la FIFO de respuestas");
        close(fd_req);
        exit(EXIT_FAILURE);
    }

    printf("[Cliente FIFO] Conexión establecida con éxito.\n");

    while (1) {
        solicitud_t sol;
        if (!mostrar_menu_y_obtener_solicitud(&sol)) {
            // Usuario seleccionó la opción de salir
            sol.tipo = MSG_SALIR;
            write(fd_req, &sol, sizeof(solicitud_t));
            printf("[Cliente FIFO] Finalizando sesión.\n");
            break;
        }

        sol.tipo = MSG_CONSULTA;

        // Enviar la solicitud de consulta al servidor
        if (write(fd_req, &sol, sizeof(solicitud_t)) <= 0) {
            perror("[Cliente FIFO Error] Falló el envío de la consulta al servidor");
            break;
        }

        // Leer los mensajes de respuesta secuencialmente desde la FIFO
        respuesta_t res;
        int total_registros = 0;
        printf("\n*** RESULTADOS DE LA BÚSQUEDA ***\n");

        while (read(fd_res, &res, sizeof(respuesta_t)) > 0) {
            if (res.tipo == RESP_REGISTRO) {
                total_registros++;
                printf("[%d] %s\n", total_registros, res.registro);
            } else if (res.tipo == RESP_FIN) {
                printf("--------------------------------------------\n");
                printf("Búsqueda completada (%d registro(s) encontrado(s)).\n\n", total_registros);
                break;
            } else if (res.tipo == RESP_SIN_RESULTADOS) {
                printf("No se encontraron coincidencias para la consulta solicitada.\n");
                printf("--------------------------------------------\n\n");
                break;
            } else if (res.tipo == RESP_ERROR) {
                printf("Error interno en el servidor al procesar la consulta en el CSV.\n");
                printf("--------------------------------------------\n\n");
                break;
            }
        }
    }

    close(fd_req);
    close(fd_res);
    return 0;
}
