#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "protocolo.h"

#define FIFO_REQ "/tmp/fifo_dino_req"
#define FIFO_RES "/tmp/fifo_dino_res"

// Función para mostrar el menú y capturar la solicitud del usuario
static int obtener_solicitud_usuario(solicitud_t *sol) {
    int opcion = 0;
    char buffer[TAM_VALORUSUARIO];

    printf("\n================ MENÚ DE BÚSQUEDA (FIFO) ================\n");
    printf("1. Nombre (accepted_name)\n");
    printf("2. Familia (family)\n");
    printf("3. País (cc)\n");
    printf("4. Edad (min_ma / max_ma)\n");
    printf("5. Formación Geológica (formation)\n");
    printf("6. Dieta (diet)\n");
    printf("7. Salir\n");
    printf("----------------------------------------------------------\n");
    printf("Seleccione una opción (1-7): ");

    if (scanf("%d", &opcion) != 1) {
        while (getchar() != '\n'); // Limpiar buffer de entrada
        return 1;
    }
    while (getchar() != '\n'); // Limpiar el salto de línea residual

    if (opcion == 7) {
        return 0; // Señal de salida
    }

    if (opcion < 1 || opcion > 6) {
        printf("Opción inválida. Intente de nuevo.\n");
        return 1;
    }

    printf("Ingrese el valor a buscar: ");
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        buffer[strcspn(buffer, "\r\n")] = '\0'; // Eliminar salto de línea
    } else {
        buffer[0] = '\0';
    }

    sol->tipo = MSG_CONSULTA;
    sol->criterio = opcion;
    strncpy(sol->valor, buffer, TAM_VALORUSUARIO - 1);
    sol->valor[TAM_VALORUSUARIO - 1] = '\0';

    return 1;
}

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
        if (!obtener_solicitud_usuario(&sol)) {
            // El usuario eligió salir
            sol.tipo = MSG_SALIR;
            write(fd_req, &sol, sizeof(solicitud_t));
            printf("[Cliente FIFO] Finalizando sesión...\n");
            break;
        }

        // Enviar la solicitud de consulta al servidor
        if (write(fd_req, &sol, sizeof(solicitud_t)) <= 0) {
            perror("[Cliente FIFO Error] Falló el envío de la consulta al servidor");
            break;
        }

        // Leer los mensajes de respuesta secuencialmente desde la FIFO
        respuesta_t res;
        int total_registros = 0;
        printf("\n================ RESULTADOS DE LA BÚSQUEDA ================\n");

        while (read(fd_res, &res, sizeof(respuesta_t)) > 0) {
            if (res.tipo == RESP_REGISTRO) {
                total_registros++;
                printf("[%d] %s\n", total_registros, res.registro);
            } else if (res.tipo == RESP_FIN) {
                printf("===========================================================\n");
                printf("Búsqueda completada (%d registro(s) encontrado(s)).\n\n", total_registros);
                break;
            } else if (res.tipo == RESP_SIN_RESULTADOS) {
                printf("No se encontraron coincidencias para la consulta solicitada.\n");
                printf("===========================================================\n\n");
                break;
            } else if (res.tipo == RESP_ERROR) {
                printf("Error interno en el servidor al procesar la consulta en el CSV.\n");
                printf("===========================================================\n\n");
                break;
            }
        }
    }

    close(fd_req);
    close(fd_res);
    return 0;
}
