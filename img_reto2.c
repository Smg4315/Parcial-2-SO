// Programa de procesamiento de imágenes en C para principiantes en Linux.
// QUÉ: Procesa imágenes PNG (escala de grises o RGB) usando matrices, con soporte
// para carga, visualización, guardado y múltiples operaciones concurrentes.
// CÓMO: Usa stb_image.h para cargar PNG y stb_image_write.h para guardar PNG,
// con hilos POSIX (pthread) para el procesamiento paralelo.
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

// ============== NUEVAS FUNCIONES PARA RETO 2 ==============

// QUÉ: Estructura para pasar datos a hilos de convolución.
// CÓMO: Contiene matrices origen/destino, rango de filas, dimensiones y kernel.
// POR QUÉ: Los hilos necesitan acceso a kernel y matrices para convolución paralela.
typedef struct {
    unsigned char*** pixelesOrigen;
    unsigned char*** pixelesDestino;
    int inicio;
    int fin;
    int ancho;
    int alto;
    int canales;
    float** kernel;
    int tamKernel;
} ConvolucionArgs;

// QUÉ: Generar kernel Gaussiano para filtro de desenfoque.
// CÓMO: Usa fórmula Gaussiana 2D: e^(-(x²+y²)/(2σ²)), normaliza suma=1.
// POR QUÉ: Kernel Gaussiano produce desenfoque natural y suave.
float** generarKernelGaussiano(int tam, float sigma) {
    float** kernel = (float**)malloc(tam * sizeof(float*));
    for (int i = 0; i < tam; i++) {
        kernel[i] = (float*)malloc(tam * sizeof(float));
    }
    
    float suma = 0.0f;
    int centro = tam / 2;
    
    for (int y = 0; y < tam; y++) {
        for (int x = 0; x < tam; x++) {
            int dx = x - centro;
            int dy = y - centro;
            kernel[y][x] = expf(-(dx*dx + dy*dy) / (2.0f * sigma * sigma));
            suma += kernel[y][x];
        }
    }
    
    // Normalizar para que suma = 1
    for (int y = 0; y < tam; y++) {
        for (int x = 0; x < tam; x++) {
            kernel[y][x] /= suma;
        }
    }
    
    return kernel;
}

// QUÉ: Aplicar convolución en un rango de filas (para hilos).
// CÓMO: Para cada píxel, multiplica vecinos por kernel, suma y clamp.
// POR QUÉ: Procesa convolución en paralelo, maneja bordes con padding.
void* aplicarConvolucionHilo(void* args) {
    ConvolucionArgs* cArgs = (ConvolucionArgs*)args;
    int radio = cArgs->tamKernel / 2;
    
    for (int y = cArgs->inicio; y < cArgs->fin; y++) {
        for (int x = 0; x < cArgs->ancho; x++) {
            for (int c = 0; c < cArgs->canales; c++) {
                float suma = 0.0f;
                
                // Aplicar kernel
                for (int ky = 0; ky < cArgs->tamKernel; ky++) {
                    for (int kx = 0; kx < cArgs->tamKernel; kx++) {
                        int py = y + ky - radio;
                        int px = x + kx - radio;
                        
                        // Padding: replicar píxeles de borde
                        if (py < 0) py = 0;
                        if (py >= cArgs->alto) py = cArgs->alto - 1;
                        if (px < 0) px = 0;
                        if (px >= cArgs->ancho) px = cArgs->ancho - 1;
                        
                        suma += cArgs->pixelesOrigen[py][px][c] * cArgs->kernel[ky][kx];
                    }
                }
                
                // Clamp resultado
                int resultado = (int)(suma + 0.5f);
                cArgs->pixelesDestino[y][x][c] = (unsigned char)(resultado < 0 ? 0 :
                                                                (resultado > 255 ? 255 : resultado));
            }
        }
    }
    return NULL;
}

// QUÉ: Aplicar filtro de desenfoque Gaussiano concurrente.
// CÓMO: Genera kernel, crea matriz destino, divide trabajo entre hilos.
// POR QUÉ: Demuestra convolución paralela y operaciones matriciales complejas.
void aplicarDesenfoqueConcurrente(ImagenInfo* info, int tamKernel, float sigma) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }
    
    if (tamKernel % 2 == 0 || tamKernel < 3) {
        printf("Tamaño de kernel debe ser impar y >= 3.\n");
        return;
    }
    
    // Generar kernel Gaussiano
    float** kernel = generarKernelGaussiano(tamKernel, sigma);
    
    // Crear matriz destino
    unsigned char*** pixelesDestino = (unsigned char***)malloc(info->alto * sizeof(unsigned char**));
    for (int y = 0; y < info->alto; y++) {
        pixelesDestino[y] = (unsigned char**)malloc(info->ancho * sizeof(unsigned char*));
        for (int x = 0; x < info->ancho; x++) {
            pixelesDestino[y][x] = (unsigned char*)malloc(info->canales * sizeof(unsigned char));
        }
    }
    
    const int numHilos = 4;
    pthread_t hilos[numHilos];
    ConvolucionArgs args[numHilos];
    int filasPorHilo = (int)ceil((double)info->alto / numHilos);
    
    // Configurar y lanzar hilos
    for (int i = 0; i < numHilos; i++) {
        args[i].pixelesOrigen = info->pixeles;
        args[i].pixelesDestino = pixelesDestino;
        args[i].inicio = i * filasPorHilo;
        args[i].fin = (i + 1) * filasPorHilo < info->alto ? (i + 1) * filasPorHilo : info->alto;
        args[i].ancho = info->ancho;
        args[i].alto = info->alto;
        args[i].canales = info->canales;
        args[i].kernel = kernel;
        args[i].tamKernel = tamKernel;
        
        if (pthread_create(&hilos[i], NULL, aplicarConvolucionHilo, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            return;
        }
    }
    
    // Esperar hilos
    for (int i = 0; i < numHilos; i++) {
        pthread_join(hilos[i], NULL);
    }
    
    // Reemplazar matriz original
    for (int y = 0; y < info->alto; y++) {
        for (int x = 0; x < info->ancho; x++) {
            free(info->pixeles[y][x]);
        }
        free(info->pixeles[y]);
    }
    free(info->pixeles);
    info->pixeles = pixelesDestino;
    
    // Liberar kernel
    for (int i = 0; i < tamKernel; i++) {
        free(kernel[i]);
    }
    free(kernel);
    
    printf("Desenfoque Gaussiano aplicado con %d hilos (kernel %dx%d, sigma=%.2f).\n", 
           numHilos, tamKernel, tamKernel, sigma);
}

// QUÉ: Estructura para pasar datos a hilos de rotación.
// CÓMO: Contiene matrices, ángulo, dimensiones y factores de transformación.
// POR QU��: Hilos necesitan acceso a matrices y parámetros de rotación.
typedef struct {
    unsigned char*** pixelesOrigen;
    unsigned char*** pixelesDestino;
    int inicio;
    int fin;
    int anchoOrigen;
    int altoOrigen;
    int anchoDestino;
    int altoDestino;
    int canales;
    float coseno;
    float seno;
    int centroOrigenX;
    int centroOrigenY;
    int centroDestinoX;
    int centroDestinoY;
} RotacionArgs;

// QUÉ: Rotar imagen en un rango de filas (para hilos).
// CÓMO: Usa matriz de rotación, interpola píxeles con interpolación bilineal.
// POR QUÉ: Procesa rotación en paralelo, maneja transformaciones geométricas.
void* rotarImagenHilo(void* args) {
    RotacionArgs* rArgs = (RotacionArgs*)args;
    
    for (int y = rArgs->inicio; y < rArgs->fin; y++) {
        for (int x = 0; x < rArgs->anchoDestino; x++) {
            // Transformar coordenadas al espacio original
            float dx = x - rArgs->centroDestinoX;
            float dy = y - rArgs->centroDestinoY;
            
            float srcX = dx * rArgs->coseno + dy * rArgs->seno + rArgs->centroOrigenX;
            float srcY = -dx * rArgs->seno + dy * rArgs->coseno + rArgs->centroOrigenY;
            
            // Interpolación bilineal
            if (srcX >= 0 && srcX < rArgs->anchoOrigen-1 && 
                srcY >= 0 && srcY < rArgs->altoOrigen-1) {
                
                int x1 = (int)srcX;
                int y1 = (int)srcY;
                int x2 = x1 + 1;
                int y2 = y1 + 1;
                
                float fx = srcX - x1;
                float fy = srcY - y1;
                
                for (int c = 0; c < rArgs->canales; c++) {
                    float p1 = rArgs->pixelesOrigen[y1][x1][c] * (1-fx) + 
                              rArgs->pixelesOrigen[y1][x2][c] * fx;
                    float p2 = rArgs->pixelesOrigen[y2][x1][c] * (1-fx) + 
                              rArgs->pixelesOrigen[y2][x2][c] * fx;
                    float resultado = p1 * (1-fy) + p2 * fy;
                    
                    rArgs->pixelesDestino[y][x][c] = (unsigned char)(resultado + 0.5f);
                }
            } else {
                // Píxel fuera de rango, usar negro
                for (int c = 0; c < rArgs->canales; c++) {
                    rArgs->pixelesDestino[y][x][c] = 0;
                }
            }
        }
    }
    return NULL;
}

// QUÉ: Rotar imagen en ángulo dado concurrentemente.
// CÓMO: Calcula nuevas dimensiones, crea matriz destino, aplica transformación.
// POR QUÉ: Demuestra transformaciones geométricas y concurrencia en acceso no secuencial.
void rotarImagenConcurrente(ImagenInfo* info, float angulo) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }
    
    float radianes = angulo * M_PI / 180.0f;
    float coseno = cosf(radianes);
    float seno = sinf(radianes);
    
    // Calcular nuevas dimensiones
    int anchoOrigen = info->ancho;
    int altoOrigen = info->alto;
    
    float corners[4][2] = {
        {0, 0}, {anchoOrigen, 0}, {anchoOrigen, altoOrigen}, {0, altoOrigen}
    };
    
    float minX = 0, maxX = 0, minY = 0, maxY = 0;
    
    for (int i = 0; i < 4; i++) {
        float x = corners[i][0] - anchoOrigen/2.0f;
        float y = corners[i][1] - altoOrigen/2.0f;
        
        float newX = x * coseno - y * seno;
        float newY = x * seno + y * coseno;
        
        if (i == 0) {
            minX = maxX = newX;
            minY = maxY = newY;
        } else {
            if (newX < minX) minX = newX;
            if (newX > maxX) maxX = newX;
            if (newY < minY) minY = newY;
            if (newY > maxY) maxY = newY;
        }
    }
    
    int anchoDestino = (int)(maxX - minX) + 1;
    int altoDestino = (int)(maxY - minY) + 1;
    
    // Crear matriz destino
    unsigned char*** pixelesDestino = (unsigned char***)malloc(altoDestino * sizeof(unsigned char**));
    for (int y = 0; y < altoDestino; y++) {
        pixelesDestino[y] = (unsigned char**)malloc(anchoDestino * sizeof(unsigned char*));
        for (int x = 0; x < anchoDestino; x++) {
            pixelesDestino[y][x] = (unsigned char*)malloc(info->canales * sizeof(unsigned char));
        }
    }
    
    const int numHilos = 4;
    pthread_t hilos[numHilos];
    RotacionArgs args[numHilos];
    int filasPorHilo = (int)ceil((double)altoDestino / numHilos);
    
    // Configurar y lanzar hilos
    for (int i = 0; i < numHilos; i++) {
        args[i].pixelesOrigen = info->pixeles;
        args[i].pixelesDestino = pixelesDestino;
        args[i].inicio = i * filasPorHilo;
        args[i].fin = (i + 1) * filasPorHilo < altoDestino ? (i + 1) * filasPorHilo : altoDestino;
        args[i].anchoOrigen = anchoOrigen;
        args[i].altoOrigen = altoOrigen;
        args[i].anchoDestino = anchoDestino;
        args[i].altoDestino = altoDestino;
        args[i].canales = info->canales;
        args[i].coseno = coseno;
        args[i].seno = seno;
        args[i].centroOrigenX = anchoOrigen / 2;
        args[i].centroOrigenY = altoOrigen / 2;
        args[i].centroDestinoX = anchoDestino / 2;
        args[i].centroDestinoY = altoDestino / 2;
        
        if (pthread_create(&hilos[i], NULL, rotarImagenHilo, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            return;
        }
    }
    
    // Esperar hilos
    for (int i = 0; i < numHilos; i++) {
        pthread_join(hilos[i], NULL);
    }
    
    // Reemplazar matriz original
    liberarImagen(info);
    info->ancho = anchoDestino;
    info->alto = altoDestino;
    info->pixeles = pixelesDestino;
    
    printf("Imagen rotada %.2f grados con %d hilos (nueva dimensión: %dx%d).\n", 
           angulo, numHilos, anchoDestino, altoDestino);

    }