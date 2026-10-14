# Practica 1  Procesos, llamadas al sistema e IPC en Linux

Sistemas Operativos �Universidad Nacional de Colombia
Sofia Franco Segura
Mariana Garcia Dussan
Julian Foglia Wilches
Carlos Romero Sicacha
---

## 1. Dataset
Ocurrencias documentadas de fosiles de dinosaurios.

El archivo se obtuvo con la API pública de la PBDB:

```
https://paleobiodb.org/data1.2/occs/list.csv?base_name=Dinosauria&show=full
```

### 1.1 Como obtener el CSV

El CSV **no esta en el repositorio** por su tamaño (GitHub no admite archivos de más de 100 MB) El archivo `.gitignore` excluye `data/*.csv` para que no se suba por accidente.

Después de clonar el repositorio:

1. Descargar el archivo desde la carpeta compartida de Google Drive: ENLACE_DEL_DRIVE
2. Guardarlo **exactamente** con esta ruta y nombre dentro del repositorio:
```
   data/dataset.csv
```
   Por ejemplo, si quedó en Descargas:
```bash
   cp ~/Downloads/pbdb_data.csv data/dataset.csv
```
3. Verificar que quedó en su lugar y que Git lo ignora:
```bash
   ls -lh data/dataset.csv
   git check-ignore data/dataset.csv   # debe responder: data/dataset.csv
```
Los programas leen el archivo directamente del disco local; no necesitan conexión a Internet ni a GitHub.

### 1.2 Columnas utilizadas

El programa localiza las columnas **por su nombre** en la primera línea (encabezado), no por su posición. Si la PBDB cambia el orden de las columnas, el programa sigue funcionando.

| Columna en el CSV | Uso |
|---|---|
| `accepted_name` | Nombre del taxón |
| `family` | Familia |
| `cc` | Código de país de 2 letras |
| `early_interval` | Intervalo geológico (se muestra en el resultado) |
| `max_ma`, `min_ma` | Edad máxima y mínima en millones de años |
| `formation` | Formación geológica |
| `diet` | Dieta |

Si falta alguna de estas columnas, la búsqueda falla con el mensaje `[csv] Al CSV le falta alguna columna necesaria`.

---

## 2. Estructura del repositorio

```
comun/      Código compartido por las versiones FIFO y memoria compartida
  protocolo.h   Formato de los mensajes entre cliente y servidor
  csv.h/.c      Búsqueda en el dataset
  menu.h/.c     Interfaz de línea de comandos (común a ambos clientes)
fifo/       Cliente y servidor de la Parte A (tuberías nombradas)
shm/        Cliente y servidor de la Parte B (memoria compartida)
data/       Ubicación del CSV (no se versiona)
docs/       Notas del grupo (observaciones con herramientas de Linux)
```

---

## 3. Criterios de búsqueda

Los criterios están definidos en `comun/protocolo.h`. El número es el que se envía en el campo `criterio` de la solicitud.

| Índice | Constante | Columna | Tipo de coincidencia | Ejemplo |
|---|---|---|---|---|
| 1 | `CRITERIO_NOMBRE` | `accepted_name` | Contiene el texto, sin distinguir mayúsculas | `rex` encuentra *Tyrannosaurus rex* |
| 2 | `CRITERIO_FAMILIA` | `family` | Contiene el texto, sin distinguir mayúsculas | `Ceratopsidae` |
| 3 | `CRITERIO_PAIS` | `cc` | Igualdad exacta, sin distinguir mayúsculas | `AR` (no encuentra `ARG`) |
| 4 | `CRITERIO_EDAD` | `min_ma`, `max_ma` | El número está dentro del rango `min_ma <= X <= max_ma` | `66` |
| 5 | `CRITERIO_FORMAGEO` | `formation` | Contiene el texto, sin distinguir mayúsculas | `Hell Creek` |
| 6 | `CRITERIO_DIETA` | `diet` | Contiene el texto, sin distinguir mayúsculas | `herbivore` |

---

## 4. Lectura del CSV (`comun/csv.c`)

### 4.1 Funcionamiento

Los campos del CSV de la PBDB vienen entre comillas dobles y algunos contienen comas (por ejemplo `"Neuquén, Patagonia"`). La función `extraer_campo` recorre la línea carácter por carácter y no trata como separador una coma que esté dentro de comillas. La secuencia `""` dentro de un campo se interpreta como una comilla literal.
Cada campo se lee con un máximo de 128 caracteres (`TAM_CAMPO`); si un campo es más largo, se recorta.
Cada consulta devuelve **como máximo 20 registros** (`MAX_RESULTADOS` en `protocolo.h`). Son las primeras 20 coincidencias en el orden en que aparecen en el archivo. Al llegar a 20, la lectura se detiene y no se recorre el resto del archivo. Para cambiar el límite basta con modificar `MAX_RESULTADOS`.

### 4.2 Probar el módulo por separado

Se puede probar la búsqueda sin cliente ni servidor, con un programa pequeño que llame a `csv_buscar` (por ejemplo `prueba_csv.c`) compilado en `bin/`, que está excluido de Git:

```bash
mkdir -p bin
gcc -Wall -Icomun prueba_csv.c comun/csv.c -o bin/prueba_csv
./bin/prueba_csv 1 Tyrannosaurus
```

Debe ejecutarse desde la raíz del repositorio, porque busca `data/dataset.csv`.

---

## 5. Protocolo de comunicación

Definido en `comun/protocolo.h`. Todos los mensajes son estructuras de **tamaño fijo**, para que quien lee sepa siempre cuántos bytes esperar y no haya mensajes ambiguos o cortados.

**Solicitud (cliente → servidor):** `solicitud_t`

| Campo | Contenido |
|---|---|
| `tipo` | `MSG_CONSULTA` (1) o `MSG_SALIR` (2) |
| `criterio` | Índice del criterio (1 a 6, ver sección 3) |
| `valor` | Texto buscado, máximo 127 caracteres |

**Respuesta (servidor → cliente):** `respuesta_t`

| Campo | Contenido |
|---|---|
| `tipo` | `RESP_REGISTRO` (1), `RESP_FIN` (2), `RESP_SIN_RESULTADOS` (3) o `RESP_ERROR` (4) |
| `registro` | Un registro formateado, o el mensaje de error |

Una consulta produce de 0 a 20 mensajes `RESP_REGISTRO` seguidos de **exactamente un** mensaje de cierre (`RESP_FIN`, `RESP_SIN_RESULTADOS` o `RESP_ERROR`).
