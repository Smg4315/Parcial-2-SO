// parcial2.c
// Versión mejorada: convolución, rotación, Sobel, resize (concurrencia pthread)
// Compilar: gcc -o exe parcial2.c -pthread -lm

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Constantes configurables
#define MAX_HILOS_DEFAULT 4
#define MIN_HILOS 1
#define MAX_HILOS 32
#define BUFFER_SIZE 512

typedef struct {
    int ancho;
    int alto;
    int canales;
    unsigned char*** pixeles; // [alto][ancho][canales]
} ImagenInfo;

// ============================================================================
// UTILIDADES Y HELPERS
// ============================================================================

static inline unsigned char clampuc(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

void limpiarBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int validarEnteroRango(const char* prompt, int min, int max, int valorDefault) {
    char buffer[100];
    int valor;
    
    printf("%s [%d-%d] (Enter para %d): ", prompt, min, max, valorDefault);
    
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        return valorDefault;
    }
    
    // Si solo presionó Enter
    if (buffer[0] == '\n') {
        return valorDefault;
    }
    
    // Intentar parsear
    char* endptr;
    errno = 0;
    long temp = strtol(buffer, &endptr, 10);
    
    if (errno != 0 || endptr == buffer || (*endptr != '\n' && *endptr != '\0')) {
        printf("⚠ Entrada inválida. Usando valor por defecto: %d\n", valorDefault);
        return valorDefault;
    }
    
    valor = (int)temp;
    
    if (valor < min || valor > max) {
        printf("⚠ Valor fuera de rango [%d-%d]. Usando valor por defecto: %d\n", 
               min, max, valorDefault);
        return valorDefault;
    }
    
    return valor;
}

float validarFloatRango(const char* prompt, float min, float max, float valorDefault) {
    char buffer[100];
    float valor;
    
    printf("%s [%.2f-%.2f] (Enter para %.2f): ", prompt, min, max, valorDefault);
    
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        return valorDefault;
    }
    
    if (buffer[0] == '\n') {
        return valorDefault;
    }
    
    char* endptr;
    errno = 0;
    valor = strtof(buffer, &endptr);
    
    if (errno != 0 || endptr == buffer || (*endptr != '\n' && *endptr != '\0')) {
        printf("⚠ Entrada inválida. Usando valor por defecto: %.2f\n", valorDefault);
        return valorDefault;
    }
    
    if (valor < min || valor > max) {
        printf("⚠ Valor fuera de rango. Usando valor por defecto: %.2f\n", valorDefault);
        return valorDefault;
    }
    
    return valor;
}

// ============================================================================
// GESTIÓN DE MEMORIA
// ============================================================================

void freeMatriz(unsigned char*** m, int alto, int ancho) {
    if (!m) return;
    for (int y = 0; y < alto; y++) {
        if (m[y]) {
            if (m[y][0]) free(m[y][0]);
            free(m[y]);
        }
    }
    free(m);
}

unsigned char*** crearMatrizPixeles(int alto, int ancho, int canales) {
    if (alto <= 0 || ancho <= 0 || canales <= 0) {
        fprintf(stderr, "❌ Error: Dimensiones inválidas (%dx%d, %d canales)\n", ancho, alto, canales);
        return NULL;
    }
    
    unsigned char*** m = malloc((size_t)alto * sizeof(unsigned char**));
    if (!m) {
        fprintf(stderr, "❌ Error: No se pudo asignar memoria para %d filas\n", alto);
        return NULL;
    }
    
    for (int y = 0; y < alto; y++) m[y] = NULL;

    for (int y = 0; y < alto; y++) {
        m[y] = malloc((size_t)ancho * sizeof(unsigned char*));
        if (!m[y]) {
            fprintf(stderr, "❌ Error: Memoria insuficiente en fila %d\n", y);
            for (int yy = 0; yy < y; yy++) {
                if (m[yy]) {
                    if (m[yy][0]) free(m[yy][0]);
                    free(m[yy]);
                }
            }
            free(m);
            return NULL;
        }
        
        size_t rowSize = (size_t)ancho * (size_t)canales * sizeof(unsigned char);
        unsigned char* rowBlock = malloc(rowSize);
        if (!rowBlock) {
            fprintf(stderr, "❌ Error: Memoria insuficiente para datos de fila %d\n", y);
            free(m[y]);
            for (int yy = 0; yy < y; yy++) {
                if (m[yy]) {
                    if (m[yy][0]) free(m[yy][0]);
                    free(m[yy]);
                }
            }
            free(m);
            return NULL;
        }
        
        for (int x = 0; x < ancho; x++) {
            m[y][x] = rowBlock + (size_t)x * (size_t)canales;
        }
        
        memset(rowBlock, 0, rowSize);
    }
    return m;
}

void liberarImagen(ImagenInfo* info) {
    if (!info) return;
    
    if (info->pixeles) {
        for (int y = 0; y < info->alto; y++) {
            if (info->pixeles[y]) {
                if (info->pixeles[y][0]) free(info->pixeles[y][0]);
                free(info->pixeles[y]);
            }
        }
        free(info->pixeles);
        info->pixeles = NULL;
    }
    info->ancho = 0;
    info->alto = 0;
    info->canales = 0;
}

// ============================================================================
// CARGA Y GUARDADO DE IMÁGENES
// ============================================================================

int cargarImagen(const char* ruta, ImagenInfo* info) {
    if (!ruta || !info) {
        fprintf(stderr, "❌ Error: Parámetros inválidos\n");
        return 0;
    }
    
    printf("📂 Cargando imagen: %s...\n", ruta);
    
    int orig_channels = 0;
    int w = 0, h = 0;
    unsigned char* datos = stbi_load(ruta, &w, &h, &orig_channels, 0);
    
    if (!datos) {
        fprintf(stderr, "❌ Error: No se pudo cargar la imagen '%s'\n", ruta);
        fprintf(stderr, "   Verifica que el archivo existe y es un formato válido (PNG, JPG, BMP, etc.)\n");
        return 0;
    }
    
    int desired = (orig_channels == 1 || orig_channels == 3) ? orig_channels : 3;
    if (orig_channels != desired) {
        stbi_image_free(datos);
        datos = stbi_load(ruta, &w, &h, &orig_channels, desired);
        if (!datos) {
            fprintf(stderr, "❌ Error: No se pudo convertir imagen a %d canales\n", desired);
            return 0;
        }
        orig_channels = desired;
    }
    
    info->ancho = w;
    info->alto = h;
    info->canales = orig_channels;
    
    printf("   Dimensiones: %dx%d píxeles\n", w, h);
    printf("   Canales: %d (%s)\n", orig_channels, orig_channels == 1 ? "Escala de grises" : "RGB");
    
    info->pixeles = crearMatrizPixeles(h, w, info->canales);
    if (!info->pixeles) {
        fprintf(stderr, "❌ Error: No hay memoria suficiente para cargar la imagen\n");
        stbi_image_free(datos);
        return 0;
    }
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            for (int c = 0; c < info->canales; c++) {
                info->pixeles[y][x][c] = datos[(y * w + x) * info->canales + c];
            }
        }
    }
    
    stbi_image_free(datos);
    printf("✓ Imagen cargada exitosamente\n");
    return 1;
}

int guardarPNG(const ImagenInfo* info, const char* rutaSalida) {
    if (!info || !rutaSalida) {
        fprintf(stderr, "❌ Error: Parámetros inválidos\n");
        return 0;
    }
    
    if (!info->pixeles) {
        fprintf(stderr, "❌ Error: No hay imagen cargada para guardar\n");
        return 0;
    }
    
    if (info->ancho <= 0 || info->alto <= 0 || info->canales <= 0) {
        fprintf(stderr, "❌ Error: Dimensiones de imagen inválidas (%dx%d, %d canales)\n", 
                info->ancho, info->alto, info->canales);
        return 0;
    }
    
    printf("💾 Guardando imagen: %s\n", rutaSalida);
    printf("   Dimensiones: %dx%d, %d canales\n", info->ancho, info->alto, info->canales);
    
    int stride = info->ancho * info->canales;
    size_t total = (size_t)info->alto * (size_t)stride;
    
    unsigned char* datos1D = malloc(total);
    if (!datos1D) {
        fprintf(stderr, "❌ Error: Memoria insuficiente para guardar imagen\n");
        return 0;
    }
    
    memset(datos1D, 0, total);
    
    for (int y = 0; y < info->alto; y++) {
        for (int x = 0; x < info->ancho; x++) {
            for (int c = 0; c < info->canales; c++) {
                size_t idx = (size_t)y * stride + (size_t)x * info->canales + c;
                if (idx < total) {
                    datos1D[idx] = info->pixeles[y][x][c];
                }
            }
        }
    }
    
    int res = stbi_write_png(rutaSalida, info->ancho, info->alto, info->canales, datos1D, stride);
    free(datos1D);
    
    if (res) {
        printf("✓ Imagen guardada exitosamente\n");
        return 1;
    } else {
        fprintf(stderr, "❌ Error: No se pudo guardar el archivo PNG\n");
        return 0;
    }
}

void mostrarMatriz(const ImagenInfo* info) {
    if (!info || !info->pixeles) {
        printf("❌ No hay imagen cargada\n");
        return;
    }
    
    printf("\n📊 Información de la imagen:\n");
    printf("   Dimensiones: %dx%d píxeles\n", info->ancho, info->alto);
    printf("   Canales: %d (%s)\n", info->canales, info->canales == 1 ? "Grises" : "RGB");
    printf("   Memoria: ~%.2f MB\n", 
           (info->alto * info->ancho * info->canales) / (1024.0 * 1024.0));
    
    printf("\n📋 Primeras filas de la matriz (máximo 8 filas x 12 columnas):\n");
    int maxFilas = (info->alto < 8) ? info->alto : 8;
    int maxCols = (info->ancho < 12) ? info->ancho : 12;
    
    for (int y = 0; y < maxFilas; y++) {
        printf("   ");
        for (int x = 0; x < maxCols; x++) {
            if (info->canales == 1) {
                printf("%3u ", info->pixeles[y][x][0]);
            } else {
                printf("(%3u,%3u,%3u) ", 
                       info->pixeles[y][x][0], 
                       info->pixeles[y][x][1], 
                       info->pixeles[y][x][2]);
            }
        }
        printf("\n");
    }
    
    if (info->alto > maxFilas) {
        printf("   ... (%d filas más)\n", info->alto - maxFilas);
    }
}

// ============================================================================
// BRILLO CONCURRENTE
// ============================================================================

typedef struct {
    unsigned char*** pixeles;
    int inicio, fin, ancho, canales;
    int delta;
    int hiloId;
} BrilloArgs;

void* ajustarBrilloHilo(void* arg) {
    BrilloArgs* a = (BrilloArgs*)arg;
    
    for (int y = a->inicio; y < a->fin; y++) {
        for (int x = 0; x < a->ancho; x++) {
            for (int c = 0; c < a->canales; c++) {
                int v = (int)a->pixeles[y][x][c] + a->delta;
                a->pixeles[y][x][c] = clampuc(v);
            }
        }
    }
    
    return NULL;
}

void ajustarBrilloConcurrente(ImagenInfo* info, int delta, int numHilos) {
    if (!info || !info->pixeles) {
        printf("❌ No hay imagen cargada\n");
        return;
    }
    
    if (numHilos < MIN_HILOS) numHilos = MIN_HILOS;
    if (numHilos > MAX_HILOS) numHilos = MAX_HILOS;
    if (numHilos > info->alto) numHilos = info->alto;
    
    printf("🔧 Ajustando brillo %s%d con %d hilos...\n", 
           delta >= 0 ? "+" : "", delta, numHilos);
    
    pthread_t* hilos = malloc(sizeof(pthread_t) * (size_t)numHilos);
    BrilloArgs* args = malloc(sizeof(BrilloArgs) * (size_t)numHilos);
    
    if (!hilos || !args) {
        fprintf(stderr, "❌ Error: Memoria insuficiente para hilos\n");
        free(hilos);
        free(args);
        return;
    }
    
    int filasPor = (info->alto + numHilos - 1) / numHilos;
    int hilosCreados = 0;
    
    for (int i = 0; i < numHilos; i++) {
        args[i].pixeles = info->pixeles;
        args[i].inicio = i * filasPor;
        args[i].fin = ((i + 1) * filasPor < info->alto) ? (i + 1) * filasPor : info->alto;
        args[i].ancho = info->ancho;
        args[i].canales = info->canales;
        args[i].delta = delta;
        args[i].hiloId = i;
        
        if (args[i].inicio < args[i].fin) {
            if (pthread_create(&hilos[i], NULL, ajustarBrilloHilo, &args[i]) != 0) {
                fprintf(stderr, "⚠ Advertencia: No se pudo crear hilo %d\n", i);
                args[i].inicio = args[i].fin;
            } else {
                hilosCreados++;
            }
        }
    }
    
    for (int i = 0; i < numHilos; i++) {
        if (args[i].inicio < args[i].fin) {
            pthread_join(hilos[i], NULL);
        }
    }
    
    free(hilos);
    free(args);
    printf("✓ Brillo ajustado correctamente (%d hilos utilizados)\n", hilosCreados);
}

// ============================================================================
// INTERPOLACIÓN BILINEAL
// ============================================================================

void sampleBilinear(unsigned char*** src, int srcW, int srcH, int channels, 
                     float fx, float fy, unsigned char* out) {
    if (channels <= 0) return;
    
    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    
    float dx = fx - x0;
    float dy = fy - y0;
    
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= srcW) x1 = srcW - 1;
    if (y1 >= srcH) y1 = srcH - 1;
    
    for (int c = 0; c < channels; c++) {
        float v00 = src[y0][x0][c];
        float v10 = src[y0][x1][c];
        float v01 = src[y1][x0][c];
        float v11 = src[y1][x1][c];
        
        float v0 = v00 * (1 - dx) + v10 * dx;
        float v1 = v01 * (1 - dx) + v11 * dx;
        float v = v0 * (1 - dy) + v1 * dy;
        
        out[c] = clampuc((int)roundf(v));
    }
}

// ============================================================================
// CONVOLUCIÓN GAUSSIANA
// ============================================================================

typedef struct {
    unsigned char*** src;
    unsigned char*** dst;
    int inicio, fin, ancho, alto, canales, tamKernel;
    float* kernel;
    int hiloId;
} ConvArgs;

void* aplicarConvolucionHilo(void* arg) {
    ConvArgs* a = (ConvArgs*)arg;
    int k2 = a->tamKernel / 2;
    
    for (int y = a->inicio; y < a->fin; y++) {
        for (int x = 0; x < a->ancho; x++) {
            for (int c = 0; c < a->canales; c++) {
                float acc = 0.0f;
                
                for (int ky = -k2; ky <= k2; ky++) {
                    for (int kx = -k2; kx <= k2; kx++) {
                        int yy = y + ky;
                        int xx = x + kx;
                        
                        if (yy < 0) yy = 0;
                        if (yy >= a->alto) yy = a->alto - 1;
                        if (xx < 0) xx = 0;
                        if (xx >= a->ancho) xx = a->ancho - 1;
                        
                        float kval = a->kernel[(ky + k2) * a->tamKernel + (kx + k2)];
                        acc += kval * (float)(a->src[yy][xx][c]);
                    }
                }
                
                a->dst[y][x][c] = clampuc((int)roundf(acc));
            }
        }
    }
    
    return NULL;
}

float* generarKernelGauss(int tam, float sigma) {
    if (tam % 2 == 0 || tam < 3) {
        fprintf(stderr, "❌ Error: Tamaño de kernel inválido (%d)\n", tam);
        return NULL;
    }
    
    int k2 = tam / 2;
    float* kernel = malloc((size_t)tam * tam * sizeof(float));
    if (!kernel) {
        fprintf(stderr, "❌ Error: No se pudo asignar memoria para kernel\n");
        return NULL;
    }
    
    float sum = 0.0f;
    float denom = 2.0f * sigma * sigma;
    
    for (int y = -k2; y <= k2; y++) {
        for (int x = -k2; x <= k2; x++) {
            float val = expf(-((float)(x * x + y * y)) / denom) / (M_PI * denom);
            kernel[(y + k2) * tam + (x + k2)] = val;
            sum += val;
        }
    }
    
    if (sum > 0.0f) {
        for (int i = 0; i < tam * tam; i++) {
            kernel[i] /= sum;
        }
    } else {
        for (int i = 0; i < tam * tam; i++) kernel[i] = 0.0f;
        kernel[k2 * tam + k2] = 1.0f;
    }
    
    return kernel;
}

void aplicarConvolucionConcurrente(ImagenInfo* info, int tamKernel, float sigma, int numHilos) {
    if (!info || !info->pixeles) {
        printf("❌ No hay imagen cargada\n");
        return;
    }
    
    if (tamKernel % 2 == 0 || tamKernel < 3) {
        printf("❌ Error: El tamaño del kernel debe ser impar y >= 3\n");
        return;
    }
    
    if (sigma <= 0.0f) {
        printf("⚠ Sigma inválido, usando 1.0\n");
        sigma = 1.0f;
    }
    
    if (numHilos < MIN_HILOS) numHilos = MIN_HILOS;
    if (numHilos > MAX_HILOS) numHilos = MAX_HILOS;
    if (numHilos > info->alto) numHilos = info->alto;
    
    printf("🔧 Aplicando convolución Gaussiana (kernel %dx%d, σ=%.2f) con %d hilos...\n", 
           tamKernel, tamKernel, sigma, numHilos);
    
    float* kernel = generarKernelGauss(tamKernel, sigma);
    if (!kernel) return;
    
    unsigned char*** dst = crearMatrizPixeles(info->alto, info->ancho, info->canales);
    if (!dst) {
        fprintf(stderr, "❌ Error: No se pudo crear matriz destino\n");
        free(kernel);
        return;
    }
    
    pthread_t* hilos = malloc(sizeof(pthread_t) * (size_t)numHilos);
    ConvArgs* args = malloc(sizeof(ConvArgs) * (size_t)numHilos);
    
    if (!hilos || !args) {
        fprintf(stderr, "❌ Error: Memoria insuficiente para hilos\n");
        free(hilos);
        free(args);
        free(kernel);
        freeMatriz(dst, info->alto, info->ancho);
        return;
    }
    
    int filas = (info->alto + numHilos - 1) / numHilos;
    int hilosCreados = 0;
    
    for (int i = 0; i < numHilos; i++) {
        args[i].src = info->pixeles;
        args[i].dst = dst;
        args[i].inicio = i * filas;
        args[i].fin = ((i + 1) * filas < info->alto) ? (i + 1) * filas : info->alto;
        args[i].ancho = info->ancho;
        args[i].alto = info->alto;
        args[i].canales = info->canales;
        args[i].tamKernel = tamKernel;
        args[i].kernel = kernel;
        args[i].hiloId = i;
        
        if (args[i].inicio < args[i].fin) {
            if (pthread_create(&hilos[i], NULL, aplicarConvolucionHilo, &args[i]) != 0) {
                fprintf(stderr, "⚠ Advertencia: No se pudo crear hilo %d\n", i);
                args[i].inicio = args[i].fin;
            } else {
                hilosCreados++;
            }
        }
    }
    
    for (int i = 0; i < numHilos; i++) {
        if (args[i].inicio < args[i].fin) {
            pthread_join(hilos[i], NULL);
        }
    }
    
    int ancho_orig = info->ancho;
    int alto_orig = info->alto;
    int canales_orig = info->canales;
    
    liberarImagen(info);
    info->pixeles = dst;
    info->ancho = ancho_orig;
    info->alto = alto_orig;
    info->canales = canales_orig;
    
    free(hilos);
    free(args);
    free(kernel);
    printf("✓ Convolución aplicada correctamente (%d hilos utilizados)\n", hilosCreados);
}

// ============================================================================
// ROTACIÓN
// ============================================================================

typedef struct {
    unsigned char*** pixelesOrigen;
    unsigned char*** pixelesDestino;
    int inicio, fin;
    int anchoOrigen, altoOrigen, canales;
    int anchoDestino, altoDestino;
    float cosA, sinA;
    float minX, minY;
    float cx, cy;
    int hiloId;
} RotArgs;

void* rotarWorker(void* arg) {
    RotArgs* r = (RotArgs*)arg;
    unsigned char out_local[4];
    
    for (int y = r->inicio; y < r->fin; y++) {
        for (int x = 0; x < r->anchoDestino; x++) {
            float X = (float)x + r->minX;
            float Y = (float)y + r->minY;
            
            float sx = r->cosA * (X - r->cx) + r->sinA * (Y - r->cy) + r->cx;
            float sy = -r->sinA * (X - r->cx) + r->cosA * (Y - r->cy) + r->cy;
            
            if (sx >= 0.0f && sx < (float)r->anchoOrigen && 
                sy >= 0.0f && sy < (float)r->altoOrigen) {
                sampleBilinear(r->pixelesOrigen, r->anchoOrigen, r->altoOrigen, 
                              r->canales, sx, sy, out_local);
                for (int c = 0; c < r->canales; c++) {
                    r->pixelesDestino[y][x][c] = out_local[c];
                }
            } else {
                for (int c = 0; c < r->canales; c++) {
                    r->pixelesDestino[y][x][c] = 0;
                }
            }
        }
    }
    
    return NULL;
}

void rotarImagenConcurrente(ImagenInfo* info, float anguloGrados, int numHilos) {
    if (!info || !info->pixeles) {
        printf("❌ No hay imagen cargada\n");
        return;
    }
    
    if (numHilos < MIN_HILOS) numHilos = MIN_HILOS;
    if (numHilos > MAX_HILOS) numHilos = MAX_HILOS;
    
    printf("🔧 Rotando imagen %.2f° con %d hilos...\n", anguloGrados, numHilos);
    
    float ang = anguloGrados * (float)M_PI / 180.0f;
    float cosA = cosf(ang), sinA = sinf(ang);
    int w = info->ancho, h = info->alto;
    float cx = (w - 1) / 2.0f, cy = (h - 1) / 2.0f;
    
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    float corners[4][2] = {{0, 0}, {(float)(w - 1), 0}, 
                          {0, (float)(h - 1)}, {(float)(w - 1), (float)(h - 1)}};
    
    for (int i = 0; i < 4; i++) {
        float x = corners[i][0], y = corners[i][1];
        float rx = cosA * (x - cx) - sinA * (y - cy) + cx;
        float ry = sinA * (x - cx) + cosA * (y - cy) + cy;
        if (rx < minX) minX = rx;
        if (rx > maxX) maxX = rx;
        if (ry < minY) minY = ry;
        if (ry > maxY) maxY = ry;
    }
    
    int anchoDestino = (int)floorf(maxX - minX) + 1;
    int altoDestino = (int)floorf(maxY - minY) + 1;
    
    if (anchoDestino <= 0) anchoDestino = 1;
    if (altoDestino <= 0) altoDestino = 1;
    
    printf("   Nueva dimensión: %dx%d píxeles\n", anchoDestino, altoDestino);
    
    unsigned char*** dst = crearMatrizPixeles(altoDestino, anchoDestino, info->canales);
    if (!dst) {
        fprintf(stderr, "❌ Error: No se pudo crear matriz destino para rotación\n");
        return;
    }
    
    if (numHilos > altoDestino) numHilos = altoDestino;
    
    pthread_t* hilos = malloc(sizeof(pthread_t) * (size_t)numHilos);
    RotArgs* args = malloc(sizeof(RotArgs) * (size_t)numHilos);
    
    if (!hilos || !args) {
        fprintf(stderr, "❌ Error: Memoria insuficiente para hilos\n");
        free(hilos);
        free(args);
        freeMatriz(dst, altoDestino, anchoDestino);
        return;
    }
    
    int filas = (altoDestino + numHilos - 1) / numHilos;
    int hilosCreados = 0;
    
    for (int i = 0; i < numHilos; i++) {
        args[i].pixelesOrigen = info->pixeles;
        args[i].pixelesDestino = dst;
        args[i].inicio = i * filas;
        args[i].fin = ((i + 1) * filas < altoDestino) ? (i + 1) * filas : altoDestino;
        args[i].anchoOrigen = info->ancho;
        args[i].altoOrigen = info->alto;
        args[i].canales = info->canales;
        args[i].anchoDestino = anchoDestino;
        args[i].altoDestino = altoDestino;
        args[i].cosA = cosA;
        args[i].sinA = sinA;
        args[i].minX = minX;
        args[i].minY = minY;
        args[i].cx = cx;
        args[i].cy = cy;
        args[i].hiloId = i;
        
        if (args[i].inicio < args[i].fin) {
            if (pthread_create(&hilos[i], NULL, rotarWorker, &args[i]) != 0) {
                fprintf(stderr, "⚠ Advertencia: No se pudo crear hilo %d\n", i);
                args[i].inicio = args[i].fin;
            } else {
                hilosCreados++;
            }
        }
    }
    
    for (int i = 0; i < numHilos; i++) {
        if (args[i].inicio < args[i].fin) {
            pthread_join(hilos[i], NULL);
        }
    }
    
    int canales_originales = info->canales;
    
    liberarImagen(info);
    info->ancho = anchoDestino;
    info->alto = altoDestino;
    info->canales = canales_originales;
    info->pixeles = dst;
    
    free(hilos);
    free(args);
    printf("✓ Rotación completada (%d hilos utilizados)\n", hilosCreados);
}

// ============================================================================
// DETECCIÓN DE BORDES SOBEL
// ============================================================================

typedef struct {
    unsigned char*** src;
    unsigned char*** dst;
    int inicio, fin, ancho, alto, canales;
    int hiloId;
} SobelArgs;

void* sobelWorker(void* arg) {
    SobelArgs* s = (SobelArgs*)arg;
    int gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int gy[3][3] = {{1, 2, 1}, {0, 0, 0}, {-1, -2, -1}};
    
    for (int y = s->inicio; y < s->fin; y++) {
        for (int x = 0; x < s->ancho; x++) {
            float sumx = 0.0f, sumy = 0.0f;
            
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int yy = y + ky;
                    int xx = x + kx;
                    
                    if (yy < 0) yy = 0;
                    if (yy >= s->alto) yy = s->alto - 1;
                    if (xx < 0) xx = 0;
                    if (xx >= s->ancho) xx = s->ancho - 1;
                    
                    float valc;
                    if (s->canales >= 3) {
                        float r = (float)s->src[yy][xx][0];
                        float g = (float)s->src[yy][xx][1];
                        float b = (float)s->src[yy][xx][2];
                        valc = 0.299f * r + 0.587f * g + 0.114f * b;
                    } else {
                        valc = (float)s->src[yy][xx][0];
                    }
                    
                    float gx_val = (float)gx[ky + 1][kx + 1];
                    float gy_val = (float)gy[ky + 1][kx + 1];
                    
                    sumx += gx_val * valc;
                    sumy += gy_val * valc;
                }
            }
            
            float magnitude = sqrtf(sumx * sumx + sumy * sumy);
            s->dst[y][x][0] = clampuc((int)roundf(magnitude));
        }
    }
    
    return NULL;
}

void detectarBordesSobelConcurrente(ImagenInfo* info, int numHilos) {
    if (!info || !info->pixeles) {
        printf("❌ No hay imagen cargada\n");
        return;
    }
    
    if (numHilos < MIN_HILOS) numHilos = MIN_HILOS;
    if (numHilos > MAX_HILOS) numHilos = MAX_HILOS;
    
    int ancho = info->ancho, alto = info->alto;
    
    printf("🔧 Detectando bordes (Sobel) con %d hilos...\n", numHilos);
    printf("   Imagen de entrada: %dx%d, %d canales\n", ancho, alto, info->canales);
    
    unsigned char*** dst = crearMatrizPixeles(alto, ancho, 1);
    if (!dst) {
        fprintf(stderr, "❌ Error: No se pudo crear matriz destino para Sobel\n");
        return;
    }
    
    if (numHilos > alto) numHilos = alto;
    
    pthread_t* hilos = malloc(sizeof(pthread_t) * (size_t)numHilos);
    SobelArgs* args = malloc(sizeof(SobelArgs) * (size_t)numHilos);
    
    if (!hilos || !args) {
        fprintf(stderr, "❌ Error: Memoria insuficiente para hilos\n");
        free(hilos);
        free(args);
        freeMatriz(dst, alto, ancho);
        return;
    }
    
    int filas = (alto + numHilos - 1) / numHilos;
    int hilosCreados = 0;
    
    for (int i = 0; i < numHilos; i++) {
        args[i].src = info->pixeles;
        args[i].dst = dst;
        args[i].inicio = i * filas;
        args[i].fin = ((i + 1) * filas < alto) ? (i + 1) * filas : alto;
        args[i].ancho = ancho;
        args[i].alto = alto;
        args[i].canales = info->canales;
        args[i].hiloId = i;
        
        if (args[i].inicio < args[i].fin) {
            if (pthread_create(&hilos[i], NULL, sobelWorker, &args[i]) != 0) {
                fprintf(stderr, "⚠ Advertencia: No se pudo crear hilo %d\n", i);
                args[i].inicio = args[i].fin;
            } else {
                hilosCreados++;
            }
        }
    }
    
    for (int i = 0; i < numHilos; i++) {
        if (args[i].inicio < args[i].fin) {
            pthread_join(hilos[i], NULL);
        }
    }
    
    int ancho_orig = info->ancho;
    int alto_orig = info->alto;
    
    liberarImagen(info);
    info->pixeles = dst;
    info->ancho = ancho_orig;
    info->alto = alto_orig;
    info->canales = 1;
    
    free(hilos);
    free(args);
    printf("✓ Detección de bordes completada (%d hilos utilizados)\n", hilosCreados);
    printf("   Imagen de salida: escala de grises (1 canal)\n");
}

// ============================================================================
// REDIMENSIONAR
// ============================================================================

typedef struct {
    unsigned char*** src;
    unsigned char*** dst;
    int inicio, fin, anchoSrc, altoSrc, anchoDst, altoDst, canales;
    float scaleX, scaleY;
    int hiloId;
} ResizeArgs;

void* resizeWorker(void* arg) {
    ResizeArgs* r = (ResizeArgs*)arg;
    unsigned char out_local[4];
    
    for (int y = r->inicio; y < r->fin; y++) {
        for (int x = 0; x < r->anchoDst; x++) {
            float fx = (x + 0.5f) * r->scaleX - 0.5f;
            float fy = (y + 0.5f) * r->scaleY - 0.5f;
            
            sampleBilinear(r->src, r->anchoSrc, r->altoSrc, r->canales, fx, fy, out_local);
            
            for (int c = 0; c < r->canales; c++) {
                r->dst[y][x][c] = out_local[c];
            }
        }
    }
    
    return NULL;
}

void redimensionarConcurrente(ImagenInfo* info, int nuevoAncho, int nuevoAlto, int numHilos) {
    if (!info || !info->pixeles) {
        printf("❌ No hay imagen cargada\n");
        return;
    }
    
    if (nuevoAncho <= 0 || nuevoAlto <= 0) {
        printf("❌ Error: Dimensiones inválidas (%dx%d)\n", nuevoAncho, nuevoAlto);
        return;
    }
    
    if (numHilos < MIN_HILOS) numHilos = MIN_HILOS;
    if (numHilos > MAX_HILOS) numHilos = MAX_HILOS;
    
    int anchoSrc = info->ancho, altoSrc = info->alto;
    
    printf("🔧 Redimensionando imagen de %dx%d a %dx%d con %d hilos...\n",
           anchoSrc, altoSrc, nuevoAncho, nuevoAlto, numHilos);
    
    unsigned char*** dst = crearMatrizPixeles(nuevoAlto, nuevoAncho, info->canales);
    if (!dst) {
        fprintf(stderr, "❌ Error: No se pudo crear matriz destino para resize\n");
        return;
    }
    
    float scaleX = (float)anchoSrc / (float)nuevoAncho;
    float scaleY = (float)altoSrc / (float)nuevoAlto;
    
    if (numHilos > nuevoAlto) numHilos = nuevoAlto;
    
    pthread_t* hilos = malloc(sizeof(pthread_t) * (size_t)numHilos);
    ResizeArgs* args = malloc(sizeof(ResizeArgs) * (size_t)numHilos);
    
    if (!hilos || !args) {
        fprintf(stderr, "❌ Error: Memoria insuficiente para hilos\n");
        free(hilos);
        free(args);
        freeMatriz(dst, nuevoAlto, nuevoAncho);
        return;
    }
    
    int filas = (nuevoAlto + numHilos - 1) / numHilos;
    int hilosCreados = 0;
    
    for (int i = 0; i < numHilos; i++) {
        args[i].src = info->pixeles;
        args[i].dst = dst;
        args[i].inicio = i * filas;
        args[i].fin = ((i + 1) * filas < nuevoAlto) ? (i + 1) * filas : nuevoAlto;
        args[i].anchoSrc = anchoSrc;
        args[i].altoSrc = altoSrc;
        args[i].anchoDst = nuevoAncho;
        args[i].altoDst = nuevoAlto;
        args[i].canales = info->canales;
        args[i].scaleX = scaleX;
        args[i].scaleY = scaleY;
        args[i].hiloId = i;
        
        if (args[i].inicio < args[i].fin) {
            if (pthread_create(&hilos[i], NULL, resizeWorker, &args[i]) != 0) {
                fprintf(stderr, "⚠ Advertencia: No se pudo crear hilo %d\n", i);
                args[i].inicio = args[i].fin;
            } else {
                hilosCreados++;
            }
        }
    }
    
    for (int i = 0; i < numHilos; i++) {
        if (args[i].inicio < args[i].fin) {
            pthread_join(hilos[i], NULL);
        }
    }
    
    int canales_orig = info->canales;
    
    liberarImagen(info);
    info->ancho = nuevoAncho;
    info->alto = nuevoAlto;
    info->canales = canales_orig;
    info->pixeles = dst;
    
    free(hilos);
    free(args);
    printf("✓ Redimensionamiento completado (%d hilos utilizados)\n", hilosCreados);
}

// ============================================================================
// MENÚ Y MAIN
// ============================================================================

void mostrarBanner() {
    printf("\n");
    printf("==============================================================\n");
    printf("                                                              \n");
    printf("       🚀 PLATAFORMA DE EDICION DE IMAGENES CONCURRENTE     \n");
    printf("                   ⚡ Procesamiento en Paralelo ⚡           \n");
    printf("                                                              \n");
    printf("==============================================================\n");
    printf("\n");
}

void mostrarMenu() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║                    📋 MENU PRINCIPAL                     ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  1. 📂 Cargar imagen                                     ║\n");
    printf("║     Formatos: PNG, JPG, BMP, TGA, etc.                   ║\n");
    printf("║                                                          ║\n");
    printf("║  2. 📊 Mostrar informacion y matriz                      ║\n");
    printf("║     Ver dimensiones y primeros pixeles                   ║\n");
    printf("║                                                          ║\n");
    printf("║  3. 💾 Guardar imagen (PNG)                              ║\n");
    printf("║     Exportar resultado de las operaciones                ║\n");
    printf("║                                                          ║\n");
    printf("║  4. ☀️  Ajustar brillo                                    ║\n");
    printf("║     Incrementar/decrementar luminosidad (-255 a +255)    ║\n");
    printf("║                                                          ║\n");
    printf("║  5. 🌫️  Aplicar desenfoque Gaussiano                      ║\n");
    printf("║     Suavizar imagen con convolucion (kernel 3x3, 5x5)    ║\n");
    printf("║                                                          ║\n");
    printf("║  6. 🔄 Rotar imagen                                      ║\n");
    printf("║     Rotacion con interpolacion bilineal (cualquier deg)  ║\n");
    printf("║                                                          ║\n");
    printf("║  7. 🔍 Detectar bordes (Sobel)                           ║\n");
    printf("║     Resaltar contornos y gradientes                      ║\n");
    printf("║                                                          ║\n");
    printf("║  8. 📐 Redimensionar                                     ║\n");
    printf("║     Cambiar tamaño con interpolacion de calidad          ║\n");
    printf("║                                                          ║\n");
    printf("║  9. 👋 Salir                                             ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n🎯 Opcion: ");
}

void mostrarEstadoImagen(const ImagenInfo* info) {
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║               📊 ESTADO ACTUAL DE LA IMAGEN              ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    
    if (info && info->pixeles) {
        printf("║  ✅ Imagen cargada                                       ║\n");
        printf("║  📏 Dimensiones: %-5d x %-5d píxeles                   ║\n", 
               info->ancho, info->alto);
        printf("║  🎨 Canales: %d (%s)                                     ║\n", 
               info->canales, info->canales == 1 ? "Escala de grises" : "RGB");
        float mb = (info->alto * info->ancho * info->canales) / (1024.0f * 1024.0f);
        printf("║  💽 Memoria: %.2f MB                                    ║\n", mb);
    } else {
        printf("║  ❌ No hay imagen cargada                                ║\n");
        printf("║  💡 Use la opcion 1 para cargar una imagen               ║\n");
    }
    
    printf("╚══════════════════════════════════════════════════════════╝\n");
}

int main(int argc, char* argv[]) {
    ImagenInfo imagen = {0, 0, 0, NULL};
    char ruta[BUFFER_SIZE];
    
    mostrarBanner();
    
    // Cargar imagen desde argumentos si se proporciona
    if (argc > 1) {
        strncpy(ruta, argv[1], sizeof(ruta) - 1);
        ruta[sizeof(ruta) - 1] = '\0';
        printf("🚀 Cargando imagen desde argumentos: %s\n", ruta);
        if (!cargarImagen(ruta, &imagen)) {
            printf("⚠ No se pudo cargar la imagen. Puede cargar otra desde el menú.\n");
        }
    }
    
    int opcion = 0;
    
    while (1) {
        mostrarEstadoImagen(&imagen);
        mostrarMenu();
        
        if (scanf("%d", &opcion) != 1) {
            limpiarBuffer();
            printf("\n❌ Entrada inválida. Por favor ingrese un número del 1 al 9.\n");
            continue;
        }
        limpiarBuffer();
        
        switch (opcion) {
            case 1: {
                // Cargar imagen
                printf("\n📂 CARGAR IMAGEN\n");
                printf("────────────────────────────────────────────────────────\n");
                printf("Ingrese la ruta del archivo: ");
                
                if (!fgets(ruta, sizeof(ruta), stdin)) {
                    printf("❌ Error leyendo la ruta\n");
                    break;
                }
                
                ruta[strcspn(ruta, "\n")] = '\0';
                
                if (strlen(ruta) == 0) {
                    printf("❌ Ruta vacía\n");
                    break;
                }
                
                liberarImagen(&imagen);
                cargarImagen(ruta, &imagen);
                break;
            }
            
            case 2: {
                // Mostrar matriz
                printf("\n📊 INFORMACIÓN DE LA IMAGEN\n");
                printf("────────────────────────────────────────────────────────\n");
                mostrarMatriz(&imagen);
                break;
            }
            
            case 3: {
                // Guardar imagen
                if (!imagen.pixeles) {
                    printf("\n❌ No hay imagen cargada. Use la opción 1 primero.\n");
                    break;
                }
                
                printf("\n💾 GUARDAR IMAGEN\n");
                printf("────────────────────────────────────────────────────────\n");
                
                char out[BUFFER_SIZE];
                
                printf("Ingrese el nombre del archivo: ");
                
                if (!fgets(out, sizeof(out), stdin)) {
                    printf("❌ Error leyendo el nombre\n");
                    break;
                }
                
                out[strcspn(out, "\n")] = '\0';
                
                if (strlen(out) == 0) {
                    printf("❌ Nombre de archivo vacío\n");
                    break;
                }
                
                // Verificar si ya tiene .png y agregarlo si no lo tiene
                size_t len = strlen(out);
                if (len < 4 || strcmp(out + len - 4, ".png") != 0) {
                    if (len + 4 < sizeof(out)) {
                        strcat(out, ".png");
                    } else {
                        printf("❌ Nombre demasiado largo\n");
                        break;
                    }
                }
                
                guardarPNG(&imagen, out);
                break;
            }
            
            case 4: {
                // Ajustar brillo
                if (!imagen.pixeles) {
                    printf("\n❌ No hay imagen cargada. Use la opción 1 primero.\n");
                    break;
                }
                
                printf("\n☀️  AJUSTAR BRILLO\n");
                printf("────────────────────────────────────────────────────────\n");
                printf("Ingrese el ajuste de brillo:\n");
                printf("  • Valores positivos aumentan el brillo (+1 a +255)\n");
                printf("  • Valores negativos reducen el brillo (-1 a -255)\n");
                
                int delta = validarEnteroRango("Ajuste de brillo", -255, 255, 0);
                int threads = validarEnteroRango("Número de hilos", MIN_HILOS, MAX_HILOS, MAX_HILOS_DEFAULT);
                
                if (delta == 0) {
                    printf("⚠ Ajuste de brillo = 0. No se realizarán cambios.\n");
                } else {
                    ajustarBrilloConcurrente(&imagen, delta, threads);
                }
                break;
            }
            
            case 5: {
                // Convolución Gaussiana
                if (!imagen.pixeles) {
                    printf("\n❌ No hay imagen cargada. Use la opción 1 primero.\n");
                    break;
                }
                
                printf("\n🌫️  DESENFOQUE GAUSSIANO\n");
                printf("────────────────────────────────────────────────────────\n");
                printf("El desenfoque Gaussiano suaviza la imagen aplicando una convolución.\n\n");
                
                printf("📏 TAMAÑO DEL KERNEL (debe ser IMPAR):\n");
                printf("  • 3x3:   desenfoque muy ligero, procesamiento rápido\n");
                printf("  • 5x5:   desenfoque ligero, buena velocidad\n");
                printf("  • 7x7:   desenfoque moderado\n");
                printf("  • 9x9:   desenfoque notable\n");
                printf("  • 15x15: desenfoque fuerte\n");
                printf("  • 25x25+: efectos artísticos extremos (lento)\n");
                printf("  ⚠ Solo números impares (3, 5, 7, 9, 11, etc.)\n\n");
                
                printf("🎚️  SIGMA (intensidad del desenfoque):\n");
                printf("  • 0.5-1.0: desenfoque muy sutil\n");
                printf("  • 1.0-2.0: desenfoque ligero (recomendado para kernel 3x3-5x5)\n");
                printf("  • 2.0-5.0: desenfoque moderado (recomendado para kernel 7x7-9x9)\n");
                printf("  • 5.0-10.0: desenfoque fuerte (recomendado para kernel 15x15+)\n");
                printf("  • 10.0+: efectos artísticos extremos\n\n");
                
                printf("💡 DIFERENCIA:\n");
                printf("  • KERNEL: define el ÁREA de influencia (cuántos píxeles vecinos)\n");
                printf("  • SIGMA: define la INTENSIDAD de la mezcla (qué tanto se mezclan)\n");
                printf("  • Kernel grande + sigma pequeño = bordes suaves pero preservados\n");
                printf("  • Kernel pequeño + sigma grande = desenfoque intenso pero localizado\n\n");
                
                int tam = validarEnteroRango("Tamaño del kernel (solo números impares: 3-51)", 3, 51, 5);
                
                // Asegurar que sea impar
                if (tam % 2 == 0) {
                    tam++;
                    printf("⚠ Ajustado a %d (el kernel DEBE ser impar para tener centro)\n", tam);
                }
                
                // Sugerir sigma basado en el tamaño del kernel
                float sigma_sugerido = (float)tam / 6.0f;
                if (sigma_sugerido < 0.5f) sigma_sugerido = 0.5f;
                
                printf("\n💡 Para kernel %dx%d, sigma recomendado: %.2f\n", tam, tam, sigma_sugerido);
                printf("   (Puedes usar valores más altos para mayor intensidad)\n");
                
                float sigma = validarFloatRango("Sigma (intensidad)", 0.1f, 50.0f, sigma_sugerido);
                int threads = validarEnteroRango("Número de hilos", MIN_HILOS, MAX_HILOS, MAX_HILOS_DEFAULT);
                
                // Mostrar estimación de resultado
                if (tam <= 5 && sigma <= 2.0f) {
                    printf("\n📊 Efecto esperado: Desenfoque sutil, ideal para suavizar ruido\n");
                } else if (tam <= 9 && sigma <= 5.0f) {
                    printf("\n📊 Efecto esperado: Desenfoque moderado, efecto dreamy\n");
                } else if (tam <= 15 && sigma <= 10.0f) {
                    printf("\n📊 Efecto esperado: Desenfoque notable, efecto artístico\n");
                } else {
                    printf("\n📊 Efecto esperado: Desenfoque extremo, efecto muy artístico\n");
                }
                
                // Advertencia para kernels muy grandes
                if (tam > 15) {
                    printf("\n⚠ ADVERTENCIA: Kernel %dx%d es grande\n", tam, tam);
                    printf("   • Tiempo de procesamiento: puede ser LENTO\n");
                    printf("   • Uso de memoria: %d valores por píxel\n", tam * tam);
                    printf("   • Recomendación: prueba con kernel más pequeño primero\n");
                    printf("\n¿Continuar con kernel %dx%d? (s/N): ", tam, tam);
                    char respuesta;
                    if (scanf(" %c", &respuesta) == 1 && (respuesta == 's' || respuesta == 'S')) {
                        limpiarBuffer();
                        aplicarConvolucionConcurrente(&imagen, tam, sigma, threads);
                    } else {
                        limpiarBuffer();
                        printf("⏭ Operación cancelada. Puedes intentar con un kernel más pequeño.\n");
                    }
                } else {
                    aplicarConvolucionConcurrente(&imagen, tam, sigma, threads);
                }
                break;
            }
            
            case 6: {
                // Rotar imagen
                if (!imagen.pixeles) {
                    printf("\n❌ No hay imagen cargada. Use la opción 1 primero.\n");
                    break;
                }
                
                printf("\n🔄 ROTAR IMAGEN\n");
                printf("────────────────────────────────────────────────────────\n");
                printf("Ingrese el ángulo de rotación:\n");
                printf("  • Valores positivos: rotación antihoraria\n");
                printf("  • Valores negativos: rotación horaria\n");
                printf("  • Ejemplos: 90, -45, 180, 30.5\n");
                
                float ang = validarFloatRango("Ángulo (grados)", -360.0f, 360.0f, 90.0f);
                int threads = validarEnteroRango("Número de hilos", MIN_HILOS, MAX_HILOS, MAX_HILOS_DEFAULT);
                
                rotarImagenConcurrente(&imagen, ang, threads);
                break;
            }
            
            case 7: {
                // Sobel
                if (!imagen.pixeles) {
                    printf("\n❌ No hay imagen cargada. Use la opción 1 primero.\n");
                    break;
                }
                
                printf("\n🔍 DETECCIÓN DE BORDES (SOBEL)\n");
                printf("────────────────────────────────────────────────────────\n");
                printf("El filtro Sobel resalta los bordes y contornos.\n");
                printf("La imagen resultante será en escala de grises.\n");
                
                int threads = validarEnteroRango("Número de hilos", MIN_HILOS, MAX_HILOS, MAX_HILOS_DEFAULT);
                
                detectarBordesSobelConcurrente(&imagen, threads);
                break;
            }
            
            case 8: {
                // Redimensionar
                if (!imagen.pixeles) {
                    printf("\n❌ No hay imagen cargada. Use la opción 1 primero.\n");
                    break;
                }
                
                printf("\n📐 REDIMENSIONAR IMAGEN\n");
                printf("────────────────────────────────────────────────────────\n");
                printf("Dimensiones actuales: %dx%d píxeles\n", imagen.ancho, imagen.alto);
                printf("Ingrese las nuevas dimensiones:\n");
                
                int w = validarEnteroRango("Nuevo ancho", 1, 10000, imagen.ancho / 2);
                int h = validarEnteroRango("Nuevo alto", 1, 10000, imagen.alto / 2);
                int threads = validarEnteroRango("Número de hilos", MIN_HILOS, MAX_HILOS, MAX_HILOS_DEFAULT);
                
                redimensionarConcurrente(&imagen, w, h, threads);
                break;
            }
            
            case 9: {
                // Salir
                printf("\n👋 Cerrando aplicación...\n");
                liberarImagen(&imagen);
                printf("✓ Memoria liberada correctamente\n");
                printf("¡Hasta pronto!\n\n");
                return EXIT_SUCCESS;
            }
            
            default: {
                printf("\n❌ Opción inválida. Por favor seleccione una opción del 1 al 9.\n");
                break;
            }
        }
        
        // Pausa para que el usuario pueda leer los mensajes
        printf("\nPresione Enter para continuar...");
        getchar();
    }
    
    return EXIT_SUCCESS;
}