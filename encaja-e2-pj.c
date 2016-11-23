#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int nr_of_threads;
#pragma omp threadprivate(nr_of_threads)

typedef unsigned char Byte;
typedef struct { Byte r, g, b; } Pixel;
typedef struct {
  unsigned int ancho, alto;
  Pixel *datos;
} Imagen;

int lee_ppm(char nombre[], Imagen *ima)
{
  FILE *F;
  unsigned x, y;
  char almohadilla[2];
  Pixel *p;

  F = fopen(nombre, "rb");
  if (F == NULL) {
    fprintf(stderr, "ERROR: Have no found file \"%s\".\n", nombre);
    return 1;
  }
  if (fgetc(F) != 'P' || fgetc(F) != '6') {
    fclose(F);
    fprintf(stderr, "ERROR: The file \"%s\" does not have proper format.\n", nombre);
    return 2;
  }
  while (fscanf(F, " %1[#]", almohadilla) == 1) fscanf(F, "%*[^\n]");
  fscanf(F, "%d%d%*d", &ima->ancho, &ima->alto);
  fgetc(F);
  ima->datos = (Pixel *)malloc(ima->ancho * ima->alto * sizeof(Pixel));
  if (ima->datos == NULL) {
    fclose(F);
    fprintf(stderr, "ERROR: There is no memory for %ux%u pixels.\n", ima->ancho, ima->alto);
    return 3;
  }
  p = ima->datos;
  for (y = 0; y < ima->alto; y++) {
    for (x = 0; x < ima->ancho; x++) {
      p->r = fgetc(F);
      p->g = fgetc(F);
      p->b = fgetc(F);
      p++;
    }
  }
  fclose(F);
  return 0;
}

int escribe_ppm(char nombre[], Imagen *ima)
{
  FILE *F;
  unsigned x, y;
  Pixel *p;

  F = fopen(nombre, "wb");
  if (F == NULL) {
    fprintf(stderr, "ERROR: Hasn't been possible to create the archive \"%s\".\n", nombre);
    return 1;
  }
  fprintf(F, "P6\n%d %d\n255\n", ima->ancho, ima->alto);
  p = ima->datos;
  for (y = 0; y < ima->alto; y++)
    for (x = 0; x < ima->ancho; x++) {
      fputc(p->r, F);
      fputc(p->g, F);
      fputc(p->b, F);
      p++;
    }
  fclose(F);
  return 0;
}

#define A(x, y) (ima->datos[(x)+(y)*ima->ancho])

void intercambia_lineas(Imagen *ima, unsigned linea1, unsigned linea2)
{
  unsigned i;
  Pixel aux, *p, *q;

  p = &A(0, linea1);
  q = &A(0, linea2);
  for (i = 0; i < ima->ancho; i++) {
    aux = *p;
    *p++ = *q;
    *q++ = aux;
  }
}

unsigned diferencia(Pixel *p, Pixel *q)
{
  unsigned dif;
  int d;

  d = p->r - q->r;
  if (d < 0) d = -d;
  dif = d;
  d = p->g - q->g;
  if (d < 0) d = -d;
  dif += d;
  d = p->b - q->b;
  if (d < 0) d = -d;
  dif += d;
  return dif;
}

void encaja(Imagen *ima)
{
  unsigned n, i, j, x, linea_minima = 0;
  long unsigned distancia, distancia_minima;
  const long unsigned grande = 1 + ima->ancho * 768ul;

  n = ima->alto - 2;
  for (i = 0; i < n; i++) {
    /* Buscamos la linea que mas se parece a la i y la ponemos en i+1 */
    distancia_minima = grande;
    #pragma omp parallel for private(x, distancia) //num_threads(100)
    for (j = i + 1; j < ima->alto; j++) {
      nr_of_threads = omp_get_num_threads();  // no need to protect because the same value is written

      distancia = 0;
      for (x = 0; x < ima->ancho; x++)
        distancia += diferencia(&A(x, i), &A(x, j));

      if (distancia < distancia_minima) {
        #pragma omp critical
        if (distancia < distancia_minima) {
            distancia_minima = distancia;
            linea_minima = j;
        }
      }
    }
    intercambia_lineas(ima, i+1, linea_minima);
  }

  printf("Number of threads: %d\n", nr_of_threads);

}

int main(int argc, char *argv[])
{
  int escribir = 1;
  Imagen ima;
  char *entrada = "binLenna1024c.ppm", *salida = "Lenna.ppm";
// *entrada = "~/UPV/CPA/Prac2/binLenna1024c.ppm",

  while (*++argv) {
    if (**argv == '-') ++*argv;
    switch (*(*argv)++) {
    case 'i':
      entrada = **argv ? *argv : *++argv;
      break;
    case 'o':
      salida = **argv ? *argv : *++argv;
      break;
    case 't':   // the image will be written to salida file
      escribir = 0;
      break;
    case 'h':
      // thrds = int(**argv ? *argv : *++argv); 
    default:
      fprintf(stderr, "Usage: encaja-e2-pj -i Input.ppm -o Output.ppm -t -h\n");
      return 1;
    }
  }

  if (lee_ppm(entrada, &ima)) return 2;

  double t1, t2;

  t1 = omp_get_wtime();
  encaja(&ima);
  t2 = omp_get_wtime();

  printf("Parallel image reconstruction time (loop: j): %f\n", t2-t1);

  if (escribir) if (escribe_ppm(salida, &ima)) return 3;

  return 0;
}
