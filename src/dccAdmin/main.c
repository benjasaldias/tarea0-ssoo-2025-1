#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "../input_manager/manager.h"

// Estructura de un proceso con lista ligada de procesos activos
typedef struct proceso
{
  pid_t pid;
  time_t inicio;
  struct proceso *siguiente;
} Proceso;

typedef struct Cola
{
  Proceso *head; // Primer proceso
  Proceso *tail; // Último proceso
} Cola;

Proceso *crear_proceso(pid_t pid)
{
  Proceso *nuevo_proceso = calloc(1, sizeof(Proceso));
  nuevo_proceso->pid = pid;
  nuevo_proceso->siguiente = NULL;
  nuevo_proceso->inicio = time(NULL);
  return nuevo_proceso;
}

Proceso *eliminar_proceso(Proceso *proceso, Proceso *anterior, Cola *cola)
{
  // if (kill(proceso->pid, SIGKILL) == 0) {
  //     printf("Proceso %d eliminado.\n", proceso->pid);
  // }

  if (cola->head == cola->tail)
  { // caso es el único en la cola
    cola->head = NULL;
    cola->tail = NULL;
  }
  else
  {
    if (anterior != NULL)
    {
      anterior->siguiente = proceso->siguiente;
    }
    if (cola->head == proceso)
    {
      cola->head = proceso->siguiente;
    }
    if (cola->tail == proceso)
    {
      cola->tail = anterior;
    }
  }

  proceso->pid = -1;
  Proceso *temp = proceso->siguiente;
  free(proceso);
  proceso = NULL;
  return temp;
}

void actualizar_procesos(Proceso *actual, Cola *cola)
{
  printf("actualizando:\n");
  Proceso *anterior = NULL;
  Proceso *siguiente = NULL;
  while (actual != NULL)
  {
    if (kill(actual->pid, 0) != 0)
    {
      printf("Proceso %d ha terminado\n", actual->pid);
      siguiente = eliminar_proceso(actual, anterior, cola);
    }
    else
    {
      anterior = actual;
      siguiente = actual->siguiente;
    }
    actual = siguiente;
  }
  return;
}

void enviar_advertencia(Proceso *proceso)
{
  if (kill(proceso->pid, SIGTERM) == 0)
  {
    printf("Advertencia SIGTERM enviada.\n");
  }
  else
  {
    printf("Error enviando advertencia...\n");
  }
  return;
}

int main(int argc, char const *argv[])
{
  Cola *cola_de_procesos = calloc(1, sizeof(Cola));
  while (1)
  {
    printf("\nmi_shell> ");
    fflush(stdout);

    char **input = read_user_input();

    if (input[0] == NULL)
    {
      free_user_input(input);
      continue;
    }

    //   EVENTO: QUIT
    if (strcmp(input[0], "quit") == 0)
    {
      for (int i = 0; i < 2; i++)
      {
        Proceso *actual = cola_de_procesos->head;
        Proceso *anterior = NULL;
        Proceso *siguiente = NULL;
        Proceso *temp = NULL;

        actualizar_procesos(actual, cola_de_procesos);
        actual = cola_de_procesos->head;
        while (actual != NULL)
        {
          if (i == 0)
          {
            printf("SE VA A ENVIAR UNA ADVERTENCIA\n");
            enviar_advertencia(actual);
          }
          else
          {
            temp = anterior;
            siguiente = eliminar_proceso(actual, anterior, cola_de_procesos);
          }
          anterior = temp;
          actual = siguiente;
        }
        if (i == 0)
        {
          // Esperamos 5 segundos a que finalicen los procesos advertidos
          printf("Saliendo...\n");
          sleep(5);
        }
      }
      free_user_input(input);
      break;
    }

    // Comando START
    if (strcmp(input[0], "start") == 0)
    {
      if (input[1] == NULL)
      {
        printf("Error: Debes indicar un ejecutable.\n");
        free_user_input(input);
        continue;
      }

      pid_t pid = fork();
      if (pid == 0)
      { // Proceso hijo
        execvp(input[1], &input[1]);
        perror("Error al ejecutar el comando");
        exit(1);
      }
      else if (pid > 0)
      { // Proceso padre
        printf("Proceso %d iniciado en segundo plano\n", pid);
        Proceso *nuevo_proceso = crear_proceso(pid);
        if (cola_de_procesos->head == NULL)
        {
          cola_de_procesos->head = nuevo_proceso;
        }
        else
        {
          cola_de_procesos->tail->siguiente = nuevo_proceso;
        }
        cola_de_procesos->tail = nuevo_proceso;
      }
      else
      {
        perror("Error al crear el proceso");
      }
    }
    else if (strcmp(input[0], "timeout") == 0)
    { // Comando TIMEOUT
      if (input[1] == NULL)
      {
        printf("Error: Debes indicar una cantidad de tiempo.\n");
        free_user_input(input);
        continue;
      }

      if (cola_de_procesos->head == NULL)
      {
        prinf("No hay procesos en ejecución. Timeout no se puede ejecutar.\n");
      }

      sleep(atoi(input[1]));
      printf("Timeout cumplido!\n");
    }
    else
    {
      printf("Error: Comando no reconocido.\n");
    }

    free_user_input(input);
  }
  Proceso *actual = cola_de_procesos->head;
  while (actual != NULL)
  {
    Proceso *temp = actual;
    actual = actual->siguiente;
    free(temp);
  }
  free(cola_de_procesos);
  return 0;
}
