#include "menu.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Lee una línea del teclado y le quita el '\n' final.
 * Devuelve 0 si leyó algo, -1 si el usuario presionó Ctrl+D (fin de entrada).
 * Si la línea no cabe en 'buf', descarta el resto y deja 'buf' vacío.
 */
static int leer_linea(char *buf, size_t tam)
{
    if (fgets(buf, (int)tam, stdin) == NULL)
        return -1;

    size_t n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';   /* elimina el salto de línea */
    } else if (n == tam - 1) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;   /* descartar lo que sobró */
        printf("Texto demasiado largo (máximo %zu caracteres).\n", tam - 2);
        buf[0] = '\0';
    }
    return 0;
}

static void mostrar_opciones(void)
{
    printf("\n========== Buscador de dinosaurios ==========\n");
    printf("  1. Buscar por nombre          (ej. Tyrannosaurus)\n");
    printf("  2. Buscar por familia         (ej. Ceratopsidae)\n");
    printf("  3. Buscar por país            (código de 2 letras, ej. AR)\n");
    printf("  4. ¿Quién vivía hace X millones de años? (ej. 66)\n");
    printf("  5. Buscar por formación geológica (ej. Hell Creek)\n");
    printf("  6. Buscar por dieta           (ej. herbivore)\n");
    printf("  0. Salir\n");
    printf("Opción: ");
    fflush(stdout);
}

/* Revisa que el valor tenga sentido para el criterio. Devuelve 1 si es válido. */
static int valor_valido(int criterio, const char *valor)
{
    if (valor[0] == '\0') {
        printf("El valor no puede estar vacío.\n");
        return 0;
    }
    if (criterio == CRITERIO_PAIS &&
        (strlen(valor) != 2 || !isalpha((unsigned char)valor[0]) ||
         !isalpha((unsigned char)valor[1]))) {
        printf("El código de país debe tener exactamente 2 letras.\n");
        return 0;
    }
    if (criterio == CRITERIO_EDAD) {
        char *fin;
        double x = strtod(valor, &fin);
        /* isfinite descarta "nan", "inf" y desbordes como "1e999" */
        if (fin == valor || *fin != '\0' || !isfinite(x) || x < 0) {
            printf("La edad debe ser un número mayor o igual que cero.\n");
            return 0;
        }
    }
    return 1;
}

void menu_leer_solicitud(solicitud_t *sol)
{
    /* El índice de cada pregunta coincide con el CRITERIO_... del protocolo */
    const char *preguntas[] = {
        "", "Nombre", "Familia", "Código de país",
        "Millones de años", "Formación geológica", "Dieta"
    };
    char opcion[16];

    memset(sol, 0, sizeof *sol);

    for (;;) {
        mostrar_opciones();

        if (leer_linea(opcion, sizeof opcion) == -1) {   /* Ctrl+D */
            printf("\n");
            sol->tipo = MSG_SALIR;
            return;
        }

        if (strcmp(opcion, "0") == 0) {
            sol->tipo = MSG_SALIR;
            return;   /* el usuario quiere salir */
        }

        int criterio = atoi(opcion);
        if (strlen(opcion) != 1 || criterio < CRITERIO_NOMBRE || criterio > CRITERIO_DIETA) {
            printf("Opción no válida.\n");
            continue;
        }

        printf("%s: ", preguntas[criterio]);
        fflush(stdout);

        if (leer_linea(sol->valor, sizeof sol->valor) == -1) {   /* Ctrl+D */
            printf("\n");
            sol->tipo = MSG_SALIR;
            return;
        }

        if (!valor_valido(criterio, sol->valor))
            continue;   /* valor inválido: se vuelve a mostrar el menú */

        sol->tipo = MSG_CONSULTA;
        sol->criterio = criterio;
        return;
    }
}

void menu_mostrar_registro(const char *registro)
{
    printf("  - %s\n", registro);
}

void menu_mostrar_cierre(int tipo_respuesta, const char *detalle, int mostrados, int total)
{
    switch (tipo_respuesta) {
    case RESP_FIN:
        printf("Se encontraron %d resultado(s)", total);
        if (total > mostrados)
            printf(" (se muestran los primeros %d)", mostrados);
        printf(".\n");
        break;
    case RESP_SIN_RESULTADOS:
        printf("No se encontraron resultados.\n");
        break;
    case RESP_ERROR:
        printf("Error del servidor: %s\n", detalle != NULL ? detalle : "desconocido");
        break;
    default:
        printf("Respuesta desconocida del servidor (tipo %d).\n", tipo_respuesta);
        break;
    }
}
