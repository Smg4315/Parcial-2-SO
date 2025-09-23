

---
# RETO 1
# Reto 1: Sistema de Chat con Colas de Mensajes

## Descripción del Reto

En el mundo actual, las aplicaciones de mensajería instantánea como WhatsApp son fundamentales para la comunicación diaria. En este reto, los estudiantes deberán implementar un sistema de chat simple entre procesos utilizando colas de mensajes (message queues) como mecanismo de comunicación entre procesos (IPC). El sistema debe soportar múltiples salas de chat, donde los usuarios pueden unirse a diferentes salas y enviar mensajes que serán recibidos por todos los miembros de la sala.

### Objetivos del Reto

1. Implementar un sistema de chat que permita la comunicación entre múltiples procesos.
2. Utilizar colas de mensajes como mecanismo de IPC.
3. Soportar múltiples salas de chat, donde cada sala tiene su propia cola de mensajes.
4. Implementar un servidor central que gestione las salas y los mensajes.
5. Permitir que los procesos clientes se unan a salas, envíen mensajes y reciban mensajes de la sala.

### Requisitos Técnicos

- **Lenguaje**: C
- **Mecanismo IPC**: Colas de mensajes (System V o POSIX)
- **Sistema Operativo**: Linux
- **Trabajo en equipo**: 2-5 personas por equipo
- **Fecha de entrega**: 10 días a partir de la publicación del reto

## Ejemplo Práctico: Funcionamiento del Sistema

### Arquitectura del Sistema

El sistema estará compuesto por:

1. **Servidor Central**: Un proceso que se encarga de:
   - Crear y gestionar las colas de mensajes para cada sala.
   - Recibir mensajes de los clientes y reenviarlos a todos los miembros de la sala.
   - Gestionar la lista de salas y los usuarios en cada sala.

2. **Clientes**: Procesos que representan a los usuarios. Cada cliente:
   - Se une a una sala específica.
   - Puede enviar mensajes a la sala.
   - Recibe todos los mensajes enviados a la sala.

### Flujo de Comunicación

1. **Inicialización**:
   - El servidor crea una cola de mensajes global para solicitudes de clientes.
   - Los clientes se conectan al servidor enviando un mensaje a la cola global.

2. **Unirse a una sala**:
   - Un cliente envía un mensaje de tipo "JOIN" a la cola global, indicando la sala a la que quiere unirse.
   - El servidor crea una cola para la sala (si no existe) y añade al cliente a la lista de esa sala.
   - El servidor responde al cliente con el identificador de la cola de la sala.

3. **Enviar un mensaje**:
   - Un cliente envía un mensaje a la cola de su sala.
   - El servidor recibe el mensaje y lo reenvía a todos los clientes de la sala (excepto al remitente).

4. **Recibir mensajes**:
   - Cada cliente está escuchando en la cola de su sala.
   - Cuando llega un mensaje, el cliente lo muestra en pantalla.

### Ejemplo de Ejecución con Nombres Colombianos

Supongamos que tenemos dos salas: "General" y "Deportes". Tres clientes: María, Juan y Camila.

1. **Servidor**:
   ```bash
   $ ./servidor
   Servidor de chat iniciado. Esperando clientes...
   ```

2. **Cliente María** (se une a "General"):
   ```bash
   $ ./cliente María
   Bienvenido, María. Salas disponibles: General, Deportes
   > join General
   Te has unido a la sala: General
   > Hola a todos en General!
   ```

3. **Cliente Juan** (se une a "General"):
   ```bash
   $ ./cliente Juan
   Bienvenido, Juan. Salas disponibles: General, Deportes
   > join General
   Te has unido a la sala: General
   > Hola María!
   ```

4. **Cliente Camila** (se une a "Deportes"):
   ```bash
   $ ./cliente Camila
   Bienvenido, Camila. Salas disponibles: General, Deportes
   > join Deportes
   Te has unido a la sala: Deportes
   > ¿Alguien vio el partido del América?
   ```

5. **Salida en María**:
   ```
   Hola a todos en General!
   Juan: Hola María!
   ```

6. **Salida en Juan**:
   ```
   María: Hola a todos en General!
   ```

7. **Salida en Camila**:
   ```
   ¿Alguien vio el partido del América?
   ```

## Código Base para Cliente/Servidor

Para aquellos estudiantes que no han visto redes de datos, aquí se proporciona un código base que utiliza colas de mensajes de System V. Este código base incluye la estructura básica del servidor y del cliente, pero está incompleto y debe ser extendido para cumplir con todos los requisitos.

### servidor.c

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define MAX_SALAS 10
#define MAX_USUARIOS_POR_SALA 20
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Estructura para los mensajes
struct mensaje {
    long mtype;         // Tipo de mensaje
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

// Estructura para una sala de chat
struct sala {
    char nombre[MAX_NOMBRE];
    int cola_id;        // ID de la cola de mensajes de la sala
    int num_usuarios;
    char usuarios[MAX_USUARIOS_POR_SALA][MAX_NOMBRE];
};

struct sala salas[MAX_SALAS];
int num_salas = 0;

// Función para crear una nueva sala
int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) {
        return -1; // No se pueden crear más salas
    }

    // Crear una cola de mensajes para la sala
    key_t key = ftok("/tmp", num_salas + 1); // Generar una clave única
    int cola_id = msgget(key, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear la cola de la sala");
        return -1;
    }

    // Inicializar la sala
    strcpy(salas[num_salas].nombre, nombre);
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;

    num_salas++;
    return num_salas - 1; // Retornar el índice de la sala creada
}

// Función para buscar una sala por nombre
int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) {
            return i;
        }
    }
    return -1; // No encontrada
}

// Función para agregar un usuario a una sala
int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario) {
    if (indice_sala < 0 || indice_sala >= num_salas) {
        return -1;
    }

    struct sala *s = &salas[indice_sala];
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) {
        return -1; // Sala llena
    }

    // Verificar si el usuario ya está en la sala
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i], nombre_usuario) == 0) {
            return -1; // Usuario ya está en la sala
        }
    }

    // Agregar el usuario
    strcpy(s->usuarios[s->num_usuarios], nombre_usuario);
    s->num_usuarios++;
    return 0;
}

// Función para enviar un mensaje a todos los usuarios de una sala
void enviar_a_todos_en_sala(int indice_sala, struct mensaje *msg) {
    if (indice_sala < 0 || indice_sala >= num_salas) {
        return;
    }

    struct sala *s = &salas[indice_sala];
    for (int i = 0; i < s->num_usuarios; i++) {
        // Enviar el mensaje a la cola de la sala
        if (msgsnd(s->cola_id, msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
            perror("Error al enviar mensaje a la sala");
        }
    }
}

int main() {
    // Crear la cola global para solicitudes de clientes
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado. Esperando clientes...\n");

    struct mensaje msg;

    while (1) {
        // Recibir mensajes de la cola global
        if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            continue;
        }

        // Procesar el mensaje según su tipo
        if (msg.mtype == 1) { // JOIN
            printf("Solicitud de unirse a la sala: %s por %s\n", msg.sala, msg.remitente);

            // Buscar o crear la sala
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala == -1) {
                indice_sala = crear_sala(msg.sala);
                if (indice_sala == -1) {
                    printf("No se pudo crear la sala %s\n", msg.sala);
                    continue;
                }
                printf("Nueva sala creada: %s\n", msg.sala);
            }

            // Agregar el usuario a la sala
            if (agregar_usuario_a_sala(indice_sala, msg.remitente) == 0) {
                printf("Usuario %s agregado a la sala %s\n", msg.remitente, msg.sala);

                // Enviar confirmación al cliente (usando el mismo tipo de mensaje)
                msg.mtype = 2; // Tipo de respuesta
                sprintf(msg.texto, "Te has unido a la sala: %s", msg.sala);
                if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                    perror("Error al enviar confirmación");
                }
            } else {
                printf("No se pudo agregar al usuario %s a la sala %s\n", msg.remitente, msg.sala);
            }
        } else if (msg.mtype == 3) { // MSG
            printf("Mensaje en la sala %s de %s: %s\n", msg.sala, msg.remitente, msg.texto);

            // Buscar la sala
            int indice_sala = buscar_sala(msg.sala);
            if (indice_sala != -1) {
                // Reenviar el mensaje a todos en la sala
                enviar_a_todos_en_sala(indice_sala, &msg);
            }
        }
    }

    return 0;
}
```

### cliente.c

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// Estructura para los mensajes
struct mensaje {
    long mtype;         // Tipo de mensaje
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

int cola_global;
int cola_sala = -1;
char nombre_usuario[MAX_NOMBRE];
char sala_actual[MAX_NOMBRE] = "";

// Función para el hilo que recibe mensajes
void *recibir_mensajes(void *arg) {
    struct mensaje msg;

    while (1) {
        if (cola_sala != -1) {
            // Recibir mensajes de la cola de la sala
            if (msgrcv(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0, 0) == -1) {
                perror("Error al recibir mensaje de la sala");
                continue;
            }

            // Mostrar el mensaje si no es del propio usuario
            if (strcmp(msg.remitente, nombre_usuario) != 0) {
                printf("%s: %s\n", msg.remitente, msg.texto);
            }
        }
        usleep(100000); // Pequeña pausa para no consumir demasiado CPU
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }

    strcpy(nombre_usuario, argv[1]);

    // Conectarse a la cola global
    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global");
        exit(1);
    }

    printf("Bienvenido, %s. Salas disponibles: General, Deportes\n", nombre_usuario);

    // Crear un hilo para recibir mensajes
    pthread_t hilo_receptor;
    pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL);

    struct mensaje msg;
    char comando[MAX_TEXTO];

    while (1) {
        printf("> ");
        fgets(comando, MAX_TEXTO, stdin);
        comando[strcspn(comando, "\n")] = '\0'; // Eliminar el salto de línea

        if (strncmp(comando, "join ", 5) == 0) {
            // Comando para unirse a una sala
            char sala[MAX_NOMBRE];
            sscanf(comando, "join %s", sala);

            // Enviar solicitud de JOIN al servidor
            msg.mtype = 1; // JOIN
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala);
            strcpy(msg.texto, "");

            if (msgsnd(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar solicitud de JOIN");
                continue;
            }

            // Esperar confirmación del servidor
            if (msgrcv(cola_global, &msg, sizeof(struct mensaje) - sizeof(long), 2, 0) == -1) {
                perror("Error al recibir confirmación");
                continue;
            }

            printf("%s\n", msg.texto);

            // Obtener la cola de la sala
            key_t key_sala = ftok("/tmp", atoi(sala)); // Esto es un ejemplo, debe ser mejorado
            cola_sala = msgget(key_sala, 0666);
            if (cola_sala == -1) {
                perror("Error al conectar a la cola de la sala");
                continue;
            }

            strcpy(sala_actual, sala);
        } else if (strlen(comando) > 0) {
            // Enviar un mensaje a la sala actual
            if (strlen(sala_actual) == 0) {
                printf("No estás en ninguna sala. Usa 'join <sala>' para unirte a una.\n");
                continue;
            }

            msg.mtype = 3; // MSG
            strcpy(msg.remitente, nombre_usuario);
            strcpy(msg.sala, sala_actual);
            strcpy(msg.texto, comando);

            if (msgsnd(cola_sala, &msg, sizeof(struct mensaje) - sizeof(long), 0) == -1) {
                perror("Error al enviar mensaje");
                continue;
            }
        }
    }

    return 0;
}
```
---
# RETO 2
# Reto 2: Extensión Concurrente de Procesamiento de Imágenes en C

## Objetivo
Basado en el código base proporcionado (un programa en C para procesamiento de imágenes PNG usando matrices 3D y concurrencia con pthreads), tu tarea es extender el programa para evaluar habilidades en concurrencia aplicada a operaciones de matrices en imágenes. Debes implementar nuevas funciones de procesamiento de imágenes que involucren operaciones matriciales complejas, asegurando que utilicen hilos POSIX (pthreads) para paralelizar el trabajo y mejorar el rendimiento. El enfoque principal es en la gestión eficiente de la concurrencia, el manejo de matrices y la robustez del código.

El código base ya incluye carga/guardado de imágenes (grayscale o RGB), visualización de matrices y ajuste de brillo concurrente. Debes integrar las nuevas funciones al menú interactivo y mantener la estructura existente (usando la estructura `ImagenInfo` y las bibliotecas stb).

## Requisitos de Implementación
1. **Mantener la Estructura Base**:
   - No modifiques las funciones existentes (`cargarImagen`, `liberarImagen`, `mostrarMatriz`, `guardarPNG`, `ajustarBrilloConcurrente`, etc.) a menos que sea necesario para integración (por ejemplo, agregar opciones al menú).
   - Asegúrate de que el programa compile con `gcc -o img img_base.c -pthread -lm` y se ejecute de manera similar.
   - Maneja tanto imágenes en escala de grises (1 canal) como RGB (3 canales) en todas las nuevas funciones.
   - Usa matrices 3D para los píxeles (`unsigned char*** pixeles`) y gestiona la memoria dinámicamente para evitar fugas.

2. **Nuevas Funciones de Procesamiento de Imágenes**:
   Debes declarar e implementar al menos **4 funciones nuevas** de tratamiento de imágenes, cada una utilizando concurrencia con pthreads (similar al ajuste de brillo). Cada función debe dividir el trabajo en hilos (mínimo 2, pero idealmente configurable o basado en el tamaño de la imagen). Las operaciones deben ser matriciales (es decir, involucrar accesos y modificaciones a la matriz de píxeles).

   - **Función 1: Convolución (e.g., Filtro de Desenfoque Gaussiano)**:
     - QUÉ: Aplica un kernel de convolución (matriz 3x3 o 5x5) a cada píxel para suavizar la imagen (blurring).
     - CÓMO: Para cada píxel, multiplica los valores vecinos por el kernel, suma y normaliza. Usa hilos para procesar rangos de filas en paralelo. Maneja bordes con padding (e.g., replicar píxeles de borde).
     - POR QUÉ: Evalúa operaciones matriciales locales y concurrencia en accesos a memoria compartida.
     - Parámetros: `ImagenInfo* info`, `int tamKernel` (tamaño del kernel, e.g., 3), `float sigma` (para generar kernel Gaussiano usando fórmula matemática).
     - Concurrencia: Divide las filas entre hilos, sincronizando con `pthread_join`.

   - **Función 2: Rotación de Imagen**:
     - QUÉ: Rota la imagen en un ángulo dado (e.g., 90°, 180°, 270° o arbitrario).
     - CÓMO: Calcula nuevas coordenadas usando matrices de transformación (e.g., x' = x*cosθ - y*sinθ, y' = x*sinθ + y*cosθ). Usa interpolación bilineal para píxeles no enteros. Crea una nueva matriz para la imagen rotada y libera la antigua.
     - POR QUÉ: Involucra transformaciones geométricas matriciales y manejo de dimensiones cambiantes.
     - Parámetros: `ImagenInfo* info`, `float angulo` (en grados).
     - Concurrencia: Paraleliza el cálculo de píxeles en la nueva matriz dividiendo por filas o bloques.

   - **Función 3: Detección de Bordes (e.g., Operador Sobel)**:
     - QUÉ: Aplica convolución con kernels Sobel para detectar bordes (gradiente horizontal y vertical).
     - CÓMO: Calcula magnitud del gradiente (sqrt(Gx² + Gy²)) y clamp a 0-255. Convierte a grayscale si es RGB (promedio de canales).
     - POR QUÉ: Combina convolución con operaciones vectoriales, evaluando concurrencia en cómputos intensivos.
     - Parámetros: `ImagenInfo* info`.
     - Concurrencia: Hilos procesan rangos de filas, compartiendo la matriz original (lectura) y escribiendo en una nueva.

   - **Función 4: Escalado de Imagen (Resize)**:
     - QUÉ: Redimensiona la imagen a un nuevo tamaño (e.g., bilinear interpolation).
     - CÓMO: Calcula factores de escala, interpola valores de píxeles desde la matriz original a una nueva.
     - POR QUÉ: Involucra accesos no secuenciales a la matriz y gestión de memoria para nueva imagen.
     - Parámetros: `ImagenInfo* info`, `int nuevoAncho`, `int nuevoAlto`.
     - Concurrencia: Paraleliza por filas de la nueva imagen.

   - **Integración al Menú**:
     Agrega opciones al menú interactivo (e.g., 5. Aplicar convolución, 6. Rotar imagen, etc.). Solicita parámetros del usuario cuando sea necesario (e.g., ángulo para rotación).

3. **Requisitos de Concurrencia**:
   - Usa al menos 2-4 hilos por función, dividiendo el trabajo por filas o bloques para evitar race conditions (la matriz es compartida en lectura/escritura).
   - Incluye estructuras como `BrilloArgs` para pasar datos a hilos.
   - Maneja errores en `pthread_create` y `pthread_join`.
   - Opcional: Haz el número de hilos configurable (e.g., vía entrada del usuario) para demostrar escalabilidad.

4. **Consideraciones Generales**:
   - Maneja errores (e.g., memoria insuficiente, imágenes no cargadas).
   - Asegura que las funciones trabajen con ambos formatos (grayscale/RGB).
   - Usa `<math.h>` para cálculos (e.g., sin, cos, sqrt).
   - No agregues dependencias externas más allá de stb y pthreads.

## Codigo base:
```c
// Programa de procesamiento de imágenes en C para principiantes en Linux.
// QUÉ: Procesa imágenes PNG (escala de grises o RGB) usando matrices, con soporte
// para carga, visualización, guardado y ajuste de brillo concurrente.
// CÓMO: Usa stb_image.h para cargar PNG y stb_image_write.h para guardar PNG,
// con hilos POSIX (pthread) para el procesamiento paralelo del brillo.
// POR QUÉ: Diseñado para enseñar manejo de matrices, concurrencia y gestión de
// memoria en C, manteniendo simplicidad y robustez para principiantes.
// Dependencias: Descarga stb_image.h y stb_image_write.h desde
// https://github.com/nothings/stb
//   wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
//   wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
//
// Compilar: gcc -o img img_base.c -pthread -lm
// Ejecutar: ./img [ruta_imagen.png]

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

// QUÉ: Incluir bibliotecas stb para cargar y guardar imágenes PNG.
// CÓMO: stb_image.h lee PNG/JPG a memoria; stb_image_write.h escribe PNG.
// POR QUÉ: Son bibliotecas de un solo archivo, simples y sin dependencias externas.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// QUÉ: Estructura para almacenar la imagen (ancho, alto, canales, píxeles).
// CÓMO: Usa matriz 3D para píxeles (alto x ancho x canales), donde canales es
// 1 (grises) o 3 (RGB). Píxeles son unsigned char (0-255).
// POR QUÉ: Permite manejar tanto grises como color, con memoria dinámica para
// flexibilidad y evitar desperdicio.
typedef struct {
    int ancho;           // Ancho de la imagen en píxeles
    int alto;            // Alto de la imagen en píxeles
    int canales;         // 1 (escala de grises) o 3 (RGB)
    unsigned char*** pixeles; // Matriz 3D: [alto][ancho][canales]
} ImagenInfo;

// QUÉ: Liberar memoria asignada para la imagen.
// CÓMO: Libera cada fila y canal de la matriz 3D, luego el arreglo de filas y
// reinicia la estructura.
// POR QUÉ: Evita fugas de memoria, esencial en C para manejar recursos manualmente.
void liberarImagen(ImagenInfo* info) {
    if (info->pixeles) {
        for (int y = 0; y < info->alto; y++) {
            for (int x = 0; x < info->ancho; x++) {
                free(info->pixeles[y][x]); // Liberar canales por píxel
            }
            free(info->pixeles[y]); // Liberar fila
        }
        free(info->pixeles); // Liberar arreglo de filas
        info->pixeles = NULL;
    }
    info->ancho = 0;
    info->alto = 0;
    info->canales = 0;
}

// QUÉ: Cargar una imagen PNG desde un archivo.
// CÓMO: Usa stbi_load para leer el archivo, detecta canales (1 o 3), y convierte
// los datos a una matriz 3D (alto x ancho x canales).
// POR QUÉ: La matriz 3D es intuitiva para principiantes y permite procesar
// píxeles y canales individualmente.
int cargarImagen(const char* ruta, ImagenInfo* info) {
    int canales;
    // QUÉ: Cargar imagen con formato original (0 canales = usar formato nativo).
    // CÓMO: stbi_load lee el archivo y llena ancho, alto y canales.
    // POR QUÉ: Respetar el formato original asegura que grises o RGB se mantengan.
    unsigned char* datos = stbi_load(ruta, &info->ancho, &info->alto, &canales, 0);
    if (!datos) {
        fprintf(stderr, "Error al cargar imagen: %s\n", ruta);
        return 0;
    }
    info->canales = (canales == 1 || canales == 3) ? canales : 1; // Forzar 1 o 3

    // QUÉ: Asignar memoria para matriz 3D.
    // CÓMO: Asignar alto filas, luego ancho columnas por fila, luego canales por píxel.
    // POR QUÉ: Estructura clara y flexible para grises (1 canal) o RGB (3 canales).
    info->pixeles = (unsigned char***)malloc(info->alto * sizeof(unsigned char**));
    if (!info->pixeles) {
        fprintf(stderr, "Error de memoria al asignar filas\n");
        stbi_image_free(datos);
        return 0;
    }
    for (int y = 0; y < info->alto; y++) {
        info->pixeles[y] = (unsigned char**)malloc(info->ancho * sizeof(unsigned char*));
        if (!info->pixeles[y]) {
            fprintf(stderr, "Error de memoria al asignar columnas\n");
            liberarImagen(info);
            stbi_image_free(datos);
            return 0;
        }
        for (int x = 0; x < info->ancho; x++) {
            info->pixeles[y][x] = (unsigned char*)malloc(info->canales * sizeof(unsigned char));
            if (!info->pixeles[y][x]) {
                fprintf(stderr, "Error de memoria al asignar canales\n");
                liberarImagen(info);
                stbi_image_free(datos);
                return 0;
            }
            // Copiar píxeles a matriz 3D
            for (int c = 0; c < info->canales; c++) {
                info->pixeles[y][x][c] = datos[(y * info->ancho + x) * info->canales + c];
            }
        }
    }

    stbi_image_free(datos); // Liberar buffer de stb
    printf("Imagen cargada: %dx%d, %d canales (%s)\n", info->ancho, info->alto,
           info->canales, info->canales == 1 ? "grises" : "RGB");
    return 1;
}

// QUÉ: Mostrar la matriz de píxeles (primeras 10 filas).
// CÓMO: Imprime los valores de los píxeles, agrupando canales por píxel (grises o RGB).
// POR QUÉ: Ayuda a visualizar la matriz para entender la estructura de datos.
void mostrarMatriz(const ImagenInfo* info) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }
    printf("Matriz de la imagen (primeras 10 filas):\n");
    for (int y = 0; y < info->alto && y < 10; y++) {
        for (int x = 0; x < info->ancho; x++) {
            if (info->canales == 1) {
                printf("%3u ", info->pixeles[y][x][0]); // Escala de grises
            } else {
                printf("(%3u,%3u,%3u) ", info->pixeles[y][x][0], info->pixeles[y][x][1],
                       info->pixeles[y][x][2]); // RGB
            }
        }
        printf("\n");
    }
    if (info->alto > 10) {
        printf("... (más filas)\n");
    }
}

// QUÉ: Guardar la matriz como PNG (grises o RGB).
// CÓMO: Aplana la matriz 3D a 1D y usa stbi_write_png con el número de canales correcto.
// POR QUÉ: Respeta el formato original (grises o RGB) para consistencia.
int guardarPNG(const ImagenInfo* info, const char* rutaSalida) {
    if (!info->pixeles) {
        fprintf(stderr, "No hay imagen para guardar.\n");
        return 0;
    }

    // QUÉ: Aplanar matriz 3D a 1D para stb.
    // CÓMO: Copia píxeles en orden [y][x][c] a un arreglo plano.
    // POR QUÉ: stb_write_png requiere datos contiguos.
    unsigned char* datos1D = (unsigned char*)malloc(info->ancho * info->alto * info->canales);
    if (!datos1D) {
        fprintf(stderr, "Error de memoria al aplanar imagen\n");
        return 0;
    }
    for (int y = 0; y < info->alto; y++) {
        for (int x = 0; x < info->ancho; x++) {
            for (int c = 0; c < info->canales; c++) {
                datos1D[(y * info->ancho + x) * info->canales + c] = info->pixeles[y][x][c];
            }
        }
    }

    // QUÉ: Guardar como PNG.
    // CÓMO: Usa stbi_write_png con los canales de la imagen original.
    // POR QUÉ: Mantiene el formato (grises o RGB) de la entrada.
    int resultado = stbi_write_png(rutaSalida, info->ancho, info->alto, info->canales,
                                   datos1D, info->ancho * info->canales);
    free(datos1D);
    if (resultado) {
        printf("Imagen guardada en: %s (%s)\n", rutaSalida,
               info->canales == 1 ? "grises" : "RGB");
        return 1;
    } else {
        fprintf(stderr, "Error al guardar PNG: %s\n", rutaSalida);
        return 0;
    }
}

// QUÉ: Estructura para pasar datos al hilo de ajuste de brillo.
// CÓMO: Contiene matriz, rango de filas, ancho, canales y delta de brillo.
// POR QUÉ: Los hilos necesitan datos específicos para procesar en paralelo.
typedef struct {
    unsigned char*** pixeles;
    int inicio;
    int fin;
    int ancho;
    int canales;
    int delta;
} BrilloArgs;

// QUÉ: Ajustar brillo en un rango de filas (para hilos).
// CÓMO: Suma delta a cada canal de cada píxel, con clamp entre 0-255.
// POR QUÉ: Procesa píxeles en paralelo para demostrar concurrencia.
void* ajustarBrilloHilo(void* args) {
    BrilloArgs* bArgs = (BrilloArgs*)args;
    for (int y = bArgs->inicio; y < bArgs->fin; y++) {
        for (int x = 0; x < bArgs->ancho; x++) {
            for (int c = 0; c < bArgs->canales; c++) {
                int nuevoValor = bArgs->pixeles[y][x][c] + bArgs->delta;
                bArgs->pixeles[y][x][c] = (unsigned char)(nuevoValor < 0 ? 0 :
                                                          (nuevoValor > 255 ? 255 : nuevoValor));
            }
        }
    }
    return NULL;
}

// QUÉ: Ajustar brillo de la imagen usando múltiples hilos.
// CÓMO: Divide las filas entre 2 hilos, pasa argumentos y espera con join.
// POR QUÉ: Usa concurrencia para acelerar el procesamiento y enseñar hilos.
void ajustarBrilloConcurrente(ImagenInfo* info, int delta) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }

    const int numHilos = 2; // QUÉ: Número fijo de hilos para simplicidad.
    pthread_t hilos[numHilos];
    BrilloArgs args[numHilos];
    int filasPorHilo = (int)ceil((double)info->alto / numHilos);

    // QUÉ: Configurar y lanzar hilos.
    // CÓMO: Asigna rangos de filas a cada hilo y pasa datos.
    // POR QUÉ: Divide el trabajo para procesar en paralelo.
    for (int i = 0; i < numHilos; i++) {
        args[i].pixeles = info->pixeles;
        args[i].inicio = i * filasPorHilo;
        args[i].fin = (i + 1) * filasPorHilo < info->alto ? (i + 1) * filasPorHilo : info->alto;
        args[i].ancho = info->ancho;
        args[i].canales = info->canales;
        args[i].delta = delta;
        if (pthread_create(&hilos[i], NULL, ajustarBrilloHilo, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            return;
        }
    }

    // QUÉ: Esperar a que los hilos terminen.
    // CÓMO: Usa pthread_join para sincronizar.
    // POR QUÉ: Garantiza que todos los píxeles se procesen antes de continuar.
    for (int i = 0; i < numHilos; i++) {
        pthread_join(hilos[i], NULL);
    }
    printf("Brillo ajustado concurrentemente con %d hilos (%s).\n", numHilos,
           info->canales == 1 ? "grises" : "RGB");
}

// QUÉ: Mostrar el menú interactivo.
// CÓMO: Imprime opciones y espera entrada del usuario.
// POR QUÉ: Proporciona una interfaz simple para interactuar con el programa.
void mostrarMenu() {
    printf("\n--- Plataforma de Edición de Imágenes ---\n");
    printf("1. Cargar imagen PNG\n");
    printf("2. Mostrar matriz de píxeles\n");
    printf("3. Guardar como PNG\n");
    printf("4. Ajustar brillo (+/- valor) concurrentemente\n");
    printf("5. Salir\n");
    printf("Opción: ");
}

// QUÉ: Función principal que controla el flujo del programa.
// CÓMO: Maneja entrada CLI, ejecuta el menú en bucle y llama funciones según opción.
// POR QUÉ: Centraliza la lógica y asegura limpieza al salir.
int main(int argc, char* argv[]) {
    ImagenInfo imagen = {0, 0, 0, NULL}; // Inicializar estructura
    char ruta[256] = {0}; // Buffer para ruta de archivo

    // QUÉ: Cargar imagen desde CLI si se pasa.
    // CÓMO: Copia argv[1] y llama cargarImagen.
    // POR QUÉ: Permite ejecución directa con ./img imagen.png.
    if (argc > 1) {
        strncpy(ruta, argv[1], sizeof(ruta) - 1);
        if (!cargarImagen(ruta, &imagen)) {
            return EXIT_FAILURE;
        }
    }

    int opcion;
    while (1) {
        mostrarMenu();
        // QUÉ: Leer opción del usuario.
        // CÓMO: Usa scanf y limpia el buffer para evitar bucles infinitos.
        // POR QUÉ: Manejo robusto de entrada evita errores comunes.
        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            printf("Entrada inválida.\n");
            continue;
        }
        while (getchar() != '\n'); // Limpiar buffer

        switch (opcion) {
            case 1: { // Cargar imagen
                printf("Ingresa la ruta del archivo PNG: ");
                if (fgets(ruta, sizeof(ruta), stdin) == NULL) {
                    printf("Error al leer ruta.\n");
                    continue;
                }
                ruta[strcspn(ruta, "\n")] = 0; // Eliminar salto de línea
                liberarImagen(&imagen); // Liberar imagen previa
                if (!cargarImagen(ruta, &imagen)) {
                    continue;
                }
                break;
            }
            case 2: // Mostrar matriz
                mostrarMatriz(&imagen);
                break;
            case 3: { // Guardar PNG
                char salida[256];
                printf("Nombre del archivo PNG de salida: ");
                if (fgets(salida, sizeof(salida), stdin) == NULL) {
                    printf("Error al leer ruta.\n");
                    continue;
                }
                salida[strcspn(salida, "\n")] = 0;
                guardarPNG(&imagen, salida);
                break;
            }
            case 4: { // Ajustar brillo
                int delta;
                printf("Valor de ajuste de brillo (+ para más claro, - para más oscuro): ");
                if (scanf("%d", &delta) != 1) {
                    while (getchar() != '\n');
                    printf("Entrada inválida.\n");
                    continue;
                }
                while (getchar() != '\n');
                ajustarBrilloConcurrente(&imagen, delta);
                break;
            }
            case 5: // Salir
                liberarImagen(&imagen);
                printf("¡Adiós!\n");
                return EXIT_SUCCESS;
            default:
                printf("Opción inválida.\n");
        }
    }
    liberarImagen(&imagen);
    return EXIT_SUCCESS;
}
```


## Rúbricas de Evaluación

El reto será evaluado en base a los siguientes criterios:

### 1. Funcionalidad (40 puntos)

| Criterio | Puntos | Descripción |
|----------|--------|-------------|
| Creación y gestión de colas | 10 | El servidor crea correctamente las colas para las salas y los clientes se conectan adecuadamente. |
| Unirse a salas | 10 | Los clientes pueden unirse a salas existentes y el servidor gestiona correctamente la lista de miembros. |
| Envío de mensajes | 10 | Los mensajes enviados por un cliente son recibidos por todos los demás miembros de la sala. |
| Múltiples salas | 10 | El sistema soporta múltiples salas simultáneamente sin interferencia entre ellas. |

### 2. Diseño y Arquitectura (20 puntos)

| Criterio | Puntos | Descripción |
|----------|--------|-------------|
| Estructura de mensajes | 5 | Los mensajes tienen una estructura clara y bien definida. |
| Modularidad | 5 | El código está organizado en funciones y módulos lógicos. |
| Manejo de errores | 5 | El sistema maneja adecuadamente errores (ej: sala no existe, cola llena, etc.). |
| Documentación | 5 | El código está documentado con comentarios claros y explicativos. |

### 3. Calidad del Código (20 puntos)

| Criterio | Puntos | Descripción |
|----------|--------|-------------|
| Legibilidad | 5 | El código es fácil de leer y entender. |
| Estilo y convenciones | 5 | Sigue convenciones de estilo consistentes (nombres de variables, indentación, etc.). |
| Eficiencia | 5 | El código es eficiente en el uso de recursos (CPU, memoria). |
| Robustez | 5 | El sistema funciona correctamente en diferentes escenarios (múltiples clientes, mensajes rápidos, etc.). |

### 4. Trabajo en Equipo (10 puntos)

| Criterio | Puntos | Descripción |
|----------|--------|-------------|
| Colaboración | 5 | Todos los miembros del equipo participan activamente en el desarrollo. |
| Integración | 5 | El trabajo de los diferentes miembros se integra correctamente. |

### 5. Entrega y Presentación (10 puntos)

| Criterio | Puntos | Descripción |
|----------|--------|-------------|
| Demostración | 10 | La demostración del proyecto es clara y muestra todas las funcionalidades. Se entrega por video corto. |

## Condiciones del Reto

- **Equipos**: 2-5 personas por equipo.
- **Fecha de entrega**: 10 días a partir de la publicación del reto.
- **Plataforma**: Linux.
- **Lenguaje**: C.
- **Mecanismo IPC**: Colas de mensajes (System V o POSIX).
- **Entrega**: 
  - Código fuente completo.
  - Documentación breve (README.md) explicando cómo compilar y ejecutar.
  - Video de demostración.
  - La entrega se hace por interactiva virtual en el buzón

## Bonus (Hasta 10 puntos extra)
- **Comandos adicionales**: Implementar comandos como `/list` (listar salas), `/users` (listar usuarios en la sala), `/leave` (abandonar sala).
- **Persistencia**: Guardar el historial de mensajes en archivos.

## Nota para Estudiantes sin Experiencia en Redes

El código base proporcionado utiliza colas de mensajes de System V, que es un mecanismo de comunicación entre procesos (IPC) en sistemas operativos Unix/Linux. No requiere conocimiento de redes de datos, ya que la comunicación se realiza dentro del mismo sistema a través del kernel. Las colas de mensajes funcionan como buzones donde los procesos pueden enviar y recibir mensajes de forma asíncrona.

Para compilar los programas, utiliza:

```bash
gcc servidor.c -o servidor -lpthread
gcc cliente.c -o cliente -lpthread
```

Para ejecutar:

1. Inicia el servidor en una terminal:
   ```bash
   ./servidor
   ```

2. Inicia los clientes en otras terminales:
   ```bash
   ./cliente María
   ./cliente Juan
   ./cliente Camila
   ```
