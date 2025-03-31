#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "../input_manager/manager.h"

// Estructura de un proceso con lista ligada de procesos activos
typedef struct proceso {
    pid_t pid;
    char nombre[64];
    time_t inicio;
    int exit_code;
    int signal_value;
    struct proceso *siguiente;
} Proceso;

typedef struct Cola {
    Proceso *head;  // Primer proceso
    Proceso *tail;   // Último proceso
  } Cola;

Proceso* crear_proceso(pid_t pid, const char* nombre) {
    Proceso* nuevo_proceso = calloc(1, sizeof(Proceso));
    nuevo_proceso->pid = pid;
    nuevo_proceso->exit_code = -999;
    nuevo_proceso->signal_value = -999;
    nuevo_proceso->siguiente = NULL;
    nuevo_proceso->inicio = time(NULL);

    // Copiar el nombre dentro de la estructura
    strncpy(nuevo_proceso->nombre, nombre, sizeof(nuevo_proceso->nombre) - 1);
    nuevo_proceso->nombre[sizeof(nuevo_proceso->nombre) - 1] = '\0';

    return nuevo_proceso;
}

// Elimina un proceso de la cola
Proceso* eliminar_proceso(Proceso* proceso, Proceso* anterior, Cola* cola) {
    // if (kill(proceso->pid, SIGKILL) == 0) {
    //     printf("Proceso %d eliminado.\n", proceso->pid);   
    // }

    if (cola->head == cola->tail) { // caso es el único en la cola
        cola->head = NULL;
        cola->tail = NULL;
    } else {
        if (anterior != NULL) {
            anterior->siguiente = proceso->siguiente;
        }
        if (cola->head == proceso) {
            cola->head = proceso->siguiente;
        } 
        if (cola->tail == proceso) {
            cola->tail = anterior;
        }
    }

    proceso->pid = -1;
    Proceso* temp = proceso->siguiente;
    free(proceso);
    proceso = NULL;
    return temp; 
}

// Revisa los procesos ya terminados y los elimina de la cola
void actualizar_procesos(Cola* cola){
    // printf("actualizando:\n");
    Proceso* anterior = NULL;
    Proceso* siguiente = NULL;
    Proceso* actual = cola->head;
    while (actual != NULL) {
        if (kill(actual->pid, 0) != 0) {
            // printf("Proceso %d ha terminado\n", actual->pid);
            siguiente = eliminar_proceso(actual, anterior, cola);
        }
        else {
            anterior = actual;
            siguiente = actual->siguiente;
        }
        actual = siguiente;
    }
    return;
}

// registro de senales recibidas
void registro_senal_manual(Proceso* proceso, int senal) {
    if (!proceso) return;

    proceso->signal_value = senal;
    proceso->exit_code = -1;

    printf("Proceso %d (%s) recibió la señal %d\n", proceso->pid, proceso->nombre, senal);
}

// Envía la advertencia de terminación SIGTERM 
void enviar_advertencia(Proceso* proceso) {
    if (kill(proceso->pid, SIGTERM) == 0) {
        printf("Advertencia SIGTERM enviada.\n");
        registro_senal_manual(proceso, SIGTERM);
    }
    else {
        printf("Error enviando advertencia...\n");
    }
    return;
}

Cola * cola_de_procesos = NULL;
bool prompted = false;

// Manejador de SIGINT (Ctrl+C)
void manejar_sigint(int sig) {
    // Proceso* proceso = cola_de_procesos->tail;
    // registro_senal_manual(proceso, sig);
    printf("\nmi_shell> "); 
    prompted = true;
    fflush(stdout);
}

// obtener exit code y signal value
void obtener_estado_proceso(Proceso* proceso, pid_t pid, int status) {

    // exit code
    if (WIFEXITED(status)) {
        proceso->exit_code = WEXITSTATUS(status);
    } else {
        proceso->exit_code = -1;
    }

    // signal value
    if (WIFSIGNALED(status)) {
        proceso->signal_value = WTERMSIG(status);
    } else {
        proceso->signal_value = -1;
    }
}



int main(int argc, char const *argv[]) {

    Cola * cola_de_procesos = calloc(1, sizeof(Cola));
    signal(SIGINT, manejar_sigint);

    while (1) {
      if (prompted == false) {
        printf("mi_shell> "); 
      } 
      prompted = false;
      fflush(stdout);

      char **input = read_user_input(); 

      if (input[0] == NULL) {  
          free_user_input(input);
          continue;
      }

    //   EVENTO: QUIT
      if (strcmp(input[0], "quit") == 0) {  
        for (int i = 0; i < 2; i++) {    
            Proceso* anterior = NULL;
            Proceso* siguiente = NULL;
            Proceso* temp = NULL;
            actualizar_procesos(cola_de_procesos);
            Proceso* actual = cola_de_procesos->head;
            actual = cola_de_procesos->head;
            while (actual != NULL) {          
                if (i == 0) {
                    enviar_advertencia(actual);
                }
                else {
                    temp = anterior;
                    siguiente = eliminar_proceso(actual, anterior, cola_de_procesos);
                }
                anterior = temp;
                actual = siguiente;
            }
            if (i == 0) {
                // Esperamos 5 segundos a que finalicen los procesos advertidos
                // printf("Saliendo...\n");
                sleep(5);
            }
        }
        free_user_input(input);
        break;
      } else

        // Comando START
        if (strcmp(input[0], "start") == 0) {
            if (input[1] == NULL) {
                printf("Error: Debes indicar un ejecutable.\n");
                free_user_input(input);
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) {  // Proceso hijo

                signal(SIGINT, SIG_DFL);

                execvp(input[1], &input[1]);
                perror("Error al ejecutar el comando");
                exit(1);
            } else if (pid > 0) {  // Proceso padre
                Proceso* nuevo_proceso = crear_proceso(pid, input[1]);

                // correr sin segundo plano
                int status;
                waitpid(pid, &status, 0);
                obtener_estado_proceso(nuevo_proceso, pid, status);

                // printf("Proceso %d iniciado en segundo plano\n", pid);

                
                if (cola_de_procesos->head == NULL) {
                    cola_de_procesos->head = nuevo_proceso;
                } else {
                    cola_de_procesos->tail->siguiente = nuevo_proceso;
                }
                cola_de_procesos->tail = nuevo_proceso;
            } else {
                perror("Error al crear el proceso");
            }
        } else

        // Comando INFO
        if (strcmp(input[0], "info") == 0) {
            Proceso* actual = cola_de_procesos->head;
            int contador = 0;
            while (actual != NULL) {
                contador++;
                printf("\n### Proceso %d ###\n", contador);
                printf("PID: %d \n", actual->pid);
                printf("Nombre: %s \n", actual->nombre);
                printf("Exit Code: %d \n", actual->exit_code);
                printf("Signal Value: %d \n", actual->signal_value);
                actual = actual->siguiente;  
            }

        } else

            // Comando  TIMEOUT
            if (strcmp(input[0], "timeout") == 0) {
                actualizar_procesos(cola_de_procesos);
                if (input[1] == NULL) {
                    printf("Error: Debes indicar un tiempo.\n");
                    continue;
                }

                // Asegurarse de que input[1] no esté vacío
                if (strlen(input[1]) == 0) {
                    printf("Error: Debes indicar un tiempo válido.\n");
                    continue;
                }

                // Verificar si input[1] es un número válido
                int is_valid_time = 1;
                for (int i = 0; input[1][i] != '\0'; i++) {
                    if (!isdigit(input[1][i])) {
                        is_valid_time = 0;
                        break;
                    }
                }

                if (!is_valid_time) {
                    printf("Error: Debes indicar un tiempo válido.\n");
                } else {
                    int time_value = input[1] - '0';
                    Proceso* actual = cola_de_procesos->head;
                    actual = cola_de_procesos->head;
    
                    bool uno_corriendo = false; 
                    while (actual != NULL) {          
                        if (actual->exit_code == -999 && actual->signal_value == -999) {
                            uno_corriendo = true;
                        } else {
                            sleep(time_value);
                        }
                        actual = actual->siguiente;
                    }
    
                    if (uno_corriendo == false) {
                        printf("No hay procesos en ejecución. Timeout no se puede ejecutar.\n");
                    }
                }
            }

        else {
            printf("%s: comando no reconocido.\n", input[0]);
        }

      free_user_input(input);
  }
    Proceso* actual = cola_de_procesos->head;
    while (actual != NULL) {
        Proceso* temp = actual;
        actual = actual->siguiente;
        free(temp);
    }
    free(cola_de_procesos);
    return 0;
}
