<div align="center">
  <h1>PARCIAL 2 - Organización De Computadores</h1>
  <h1> 🖼️ Procesador de Imágenes con Hilos POSIX</h1>
</div>

Aplicación de procesamiento de imágenes desarrollada en C que utiliza hilos POSIX para operaciones paralelas y las librerías `stb_image`/`stb_image_write` para manejo de formatos de imagen.


## 👥 Integrantes:
- Andres Felipe Velez Alvarez
- Samuel Samper Cardona
- Sebastian Salazar Henao
- Simon Mazo Gomez
- Juan Simon Ospina Martinez

---

## 🔧 Compilación y Ejecución

Para compilar y ejecutar el proyecto (C con hilos POSIX y `stb_image`/`stb_image_write` ya incluidos), ubícate en la carpeta raíz (`Parcial-2-SO-main`) y compila con GCC enlazando hilos y la librería matemática:

```bash
gcc -o exe parcial2.c -pthread -lm
```

Esto genera el binario `exe`.

### 🚀 Formas de Ejecución

La ejecución puede hacerse de dos formas:

- **📂 (a) Con argumento**: Pasando la imagen de entrada como argumento para que el programa la cargue de inmediato:
  ```bash
  ./exe imagen.jpg
  ```

- **🖱️ (b) Modo interactivo**: Lanzando `./exe` sin argumentos y usando el menú interactivo para **Cargar imagen** (opción 1):
  ```bash
  ./exe
  ```

### 📁 Formatos Soportados

El programa soporta formatos comunes: **PNG** 🖼️ | **JPG** 📷 | **BMP** 🎨 | **TGA** 🎭

Muestra información básica y una vista parcial de la matriz, y permite aplicar diversas operaciones de procesamiento de imágenes.

### 💾 Guardar Cambios

Cuando quieras persistir los cambios, usa **Guardar imagen (PNG)** (opción 3), ingresando el nombre de salida:
```
resultado.png
```

## ⚙️ Requisitos

- ✅ Tener `gcc` disponible (Linux/macOS; en Windows usar MinGW o WSL)
- ✅ Compilar desde el mismo directorio donde están `parcial2.c`, `stb_image.h` y `stb_image_write.h` para que las inclusiones se resuelvan correctamente

---

## ✨ Funcionalidades

### 🔹 1. Cargar imagen 📥
Permite al usuario seleccionar una imagen (en formatos como JPG, PNG, BMP o TGA) y cargarla en memoria. El programa utiliza la librería `stb_image.h` para decodificar los píxeles y almacenarlos en una matriz bidimensional que servirá de base para los posteriores procesamientos.

### 🔹 2. Mostrar información ℹ️
Despliega datos esenciales de la imagen cargada: nombre, dimensiones (ancho y alto), cantidad de canales de color y tamaño total en bytes. Esto ayuda a verificar que la carga se haya realizado correctamente antes de aplicar transformaciones.

### 🔹 3. Mostrar matriz 🔢
Imprime en pantalla una parte representativa de la matriz de píxeles (no toda, para evitar saturar la terminal). Esta vista parcial permite observar cómo están organizados los valores RGB que conforman la imagen original.

### 🔹 4. Aplicar filtro de brillo ☀️
Incrementa o disminuye el valor de brillo de cada píxel. Se realiza multiplicando los valores RGB por un factor definido por el usuario, logrando imágenes más claras o más oscuras sin alterar la estructura de color.

### 🔹 5. Aplicar filtro de desenfoque (blur) 🌫️
Implementa un desenfoque básico o gaussiano usando el promedio de píxeles vecinos. Este proceso suaviza los bordes y reduce el ruido visual, generando una apariencia más difusa en la imagen.

### 🔹 6. Aplicar filtro Sobel 🔍
Ejecuta la detección de bordes mediante el operador Sobel, calculando gradientes horizontales y verticales. El resultado resalta contornos y transiciones fuertes entre áreas de diferente intensidad, ideal para análisis de formas.

### 🔹 7. Rotar imagen 🔄
Permite rotar la imagen 90°, 180° o 270°, reorganizando la matriz de píxeles según la orientación elegida. Se utilizan cálculos de coordenadas para reasignar correctamente las posiciones.

### 🔹 8. Redimensionar imagen 📐
Modifica las dimensiones de la imagen (ancho y alto) usando interpolación simple, adaptando la cantidad de píxeles para obtener versiones más pequeñas o más grandes, sin perder la proporción visual.

### 🔹 9. Guardar imagen 💾
Guarda el resultado de las transformaciones aplicadas en un nuevo archivo, utilizando `stb_image_write.h`. El usuario elige el nombre de salida y el formato (generalmente `.png`), preservando así las modificaciones realizadas.

---

## 📝 Ejemplo de Uso

```bash
# Compilar el proyecto
gcc -o exe parcial2.c -pthread -lm

# Ejecutar con imagen de entrada
./exe mi_imagen.jpg

# O ejecutar en modo interactivo
./exe
```

---

## 👨‍💻 Tecnologías Utilizadas

- **Lenguaje**: C
- **Concurrencia**: Hilos POSIX (`pthread`)
- **Librerías**: 
  - `stb_image.h` - Carga de imágenes
  - `stb_image_write.h` - Guardado de imágenes
  - `math.h` - Operaciones matemáticas
 
## 📽️ Link del video

- www.youtube.com/watch?v=eLQFdhWOG8U&feature=youtu.be
