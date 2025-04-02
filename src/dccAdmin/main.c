#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include "../input_manager/manager.h"

int tiempo_maximo = 0;

// Estructura de un proceso con lista ligada de procesos activos
typedef struct proceso
{
    pid_t pid;
    char nombre[64];
    time_t inicio;
    time_t final;
    int exit_code;
    int signal_value;
    struct proceso *siguiente;
    time_t sigterm_time;
} Proceso;

typedef struct Cola
{
    Proceso *head; // Primer proceso
    Proceso *tail; // Último proceso
} Cola;

typedef struct Timeout
{
    time_t start_time;
    int duration;
    struct Timeout *next;
} Timeout;

Timeout *timeout_list_head = NULL;

void crear_timeout(int duration)
{
    Timeout *new_timeout = calloc(1, sizeof(Timeout));
    new_timeout->start_time = time(NULL);
    new_timeout->duration = duration;
    new_timeout->next = timeout_list_head;

    timeout_list_head = new_timeout;
}

bool process_is_alive(Proceso *process)
{
    return (kill(process->pid, 0) == 0);
}

void print_process(Proceso *process)
{
    double tiempo_transcurrido;
    if (process->final)
    {
        tiempo_transcurrido = difftime(process->final, process->inicio);
    }
    else
    {
        tiempo_transcurrido = difftime(time(NULL), process->inicio);
    }
    printf("%d %s %.2f %d %d\n", process->pid, process->nombre, tiempo_transcurrido, process->exit_code, process->signal_value);
}

void apply_timeouts(Cola *process_list)
{
    Timeout *current_timeout = timeout_list_head;
    Timeout *previous_timeout = NULL;

    while (current_timeout != NULL)
    {
        if (difftime(time(NULL), current_timeout->start_time) >= current_timeout->duration)
        {
            printf("\nTimeout cumplido!\n");
            Proceso *current_process = process_list->head;
            while (current_process != NULL)
            {
                if (process_is_alive(current_process) && difftime(current_timeout->start_time, current_process->inicio) >= 0)
                {
                    print_process(current_process);
                    kill(current_process->pid, SIGTERM);
                }
                current_process = current_process->siguiente;
            }

            Timeout *used_timeout = current_timeout;
            current_timeout = used_timeout->next;

            if (previous_timeout != NULL)
            {
                previous_timeout->next = current_timeout;
            }
            else
            {
                timeout_list_head = current_timeout;
            }

            free(used_timeout);
        }
        else
        {
            previous_timeout = current_timeout;
            current_timeout = current_timeout->next;
        }
    }
}

Proceso *crear_proceso(pid_t pid, const char *nombre)
{
    Proceso *nuevo_proceso = calloc(1, sizeof(Proceso));
    nuevo_proceso->pid = pid;
    nuevo_proceso->exit_code = -1;
    nuevo_proceso->signal_value = -1;
    nuevo_proceso->siguiente = NULL;
    nuevo_proceso->inicio = time(NULL);
    nuevo_proceso->sigterm_time = 0;

    // Copiar el nombre dentro de la estructura
    strncpy(nuevo_proceso->nombre, nombre, sizeof(nuevo_proceso->nombre) - 1);
    nuevo_proceso->nombre[sizeof(nuevo_proceso->nombre) - 1] = '\0';

    return nuevo_proceso;
}

// Elimina un proceso de la cola
void eliminar_proceso(Proceso *proceso, Proceso *anterior, Cola *cola)
{
    if (kill(proceso->pid, SIGKILL) == 0)
    {
        printf("Proceso %d eliminado.\n", proceso->pid);
    }

    // if (cola->head == cola->tail)
    // { // caso es el único en la cola
    //     cola->head = NULL;
    //     cola->tail = NULL;
    //     // printf("cabeza y cola son null ahora\n");
    // }
    // else
    // {
    //     if (anterior != NULL)
    //     {
    //         anterior->siguiente = proceso->siguiente;
    //     }
    //     if (cola->head == proceso)
    //     {
    //         cola->head = proceso->siguiente;
    //     }
    //     if (cola->tail == proceso)
    //     {
    //         cola->tail = anterior;
    //     }
    // }

    // proceso->pid = -1;
    // Proceso *temp = proceso->siguiente;
    // free(proceso);
    // proceso = NULL;
    // return temp;
}

// Espera que no detiene el programa
int espera_no_bloqueante(int tiempo)
{
    fd_set set;
    struct timeval timeout;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeout.tv_sec = tiempo;
    timeout.tv_usec = 0;

    return select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
}

void actualizar_procesos(Cola *cola)
{
    Proceso *actual = cola->head;

    while (actual != NULL)
    {
        time_t tiempo_actual = time(NULL);
        double tiempo_transcurrido = difftime(tiempo_actual, actual->inicio);

        // Si el proceso ha excedido su tiempo máximo, intenta matarlo
        if (tiempo_transcurrido > tiempo_maximo && tiempo_maximo > 0)
        {
            if (waitpid(actual->pid, NULL, WNOHANG) == 0) // Si el proceso sigue vivo
            {
                if (!actual->sigterm_time)
                {
                    kill(actual->pid, SIGTERM);
                    actual->sigterm_time = time(NULL);
                    printf("\nWarned\n");
                    fflush(stdout);
                }
                else if (difftime(time(NULL), actual->sigterm_time) > 5)
                {
                    kill(actual->pid, SIGKILL);
                    printf("\nKilled\n");
                    fflush(stdout);
                }
            }
        }

        // Verificar si el proceso ha terminado para evitar zombis
        int estado;
        if (waitpid(actual->pid, &estado, WNOHANG) > 0)
        {
            if (WIFEXITED(estado))
            {
                actual->exit_code = WEXITSTATUS(estado); // Guardar código de salida
            }
            else if (WIFSIGNALED(estado))
            {
                actual->signal_value = WTERMSIG(estado); // Guardar señal que lo mató
            }
        }

        actual = actual->siguiente;
    }
}

// registro de senales recibidas
void registro_senal_manual(Proceso *proceso, int senal)
{
    if (!proceso)
        return;

    proceso->signal_value = senal;
    proceso->exit_code = -1;

    // printf("Proceso %d (%s) recibió la señal %d\n", proceso->pid, proceso->nombre, senal);
}

// Envía la señal de interrupción SIGINT
void enviar_advertencia(Proceso *proceso)
{
    signal(SIGINT, SIG_DFL);

    if (kill(proceso->pid, SIGINT) == 0)
    {
        printf("\nAdvertencia SIGINT enviada a %d.\n", proceso->pid);
        registro_senal_manual(proceso, SIGINT);
    }
    else
    {
        printf("Error enviando advertencia...\n");
    }
    return;
}

Cola *cola_de_procesos = NULL;
bool prompted = false;

// obtener exit code y signal value
void obtener_estado_proceso(Proceso *proceso, pid_t pid, int status)
{

    // exit code
    if (WIFEXITED(status))
    {
        proceso->exit_code = WEXITSTATUS(status);
    }
    else
    {
        proceso->exit_code = -1;
    }

    // signal value
    if (WIFSIGNALED(status))
    {
        proceso->signal_value = WTERMSIG(status);
    }
    else
    {
        proceso->signal_value = -1;
    }
}

// actualiza las señales de salida y recibidas
void actualizar_child(pid_t pid, int status)
{
    if (cola_de_procesos == NULL)
    {
        printf("[ERROR] cola_de_procesos es NULL en manejador_sigchld\n");
        return;
    }

    Proceso *actual = cola_de_procesos->head;
    while (actual != NULL)
    {
        if (actual->pid == pid)
        {
            actual->final = time(NULL);
            if (WIFEXITED(status))
            {
                actual->exit_code = WEXITSTATUS(status);
                // printf("Proceso %d terminó con código de salida %d\n", pid, actual->exit_code);
            }
            else if (WIFSIGNALED(status))
            {
                actual->signal_value = WTERMSIG(status);
                // printf("Proceso %d terminó por la señal %d\n", pid, actual->signal_value);
            }
            return; // Salimos después de actualizar el proceso
        }
        actual = actual->siguiente;
    }
    return;
}

// elimina zombies
void manejador_sigchld(int sig)
{

    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        printf("Proceso %d terminó\n", pid);
        // if (prompted == false) {
        //     printf("\n");
        //     fflush(stdout);
        // }
        actualizar_child(pid, status);
    }
}

void handle_quit()
{
    printf("\nTerminando ejecucion de DCCAdmin\n");
    for (int i = 0; i < 2; i++)
    {
        Proceso *anterior = NULL;
        Proceso *siguiente = NULL;
        Proceso *temp = NULL;
        actualizar_procesos(cola_de_procesos);
        Proceso *actual = cola_de_procesos->head;
        actual = cola_de_procesos->head;

        while (actual != NULL)
        {
            if (i == 0 && kill(actual->pid, 0) == 0)
            {
                enviar_advertencia(actual);
            }
            else if (i == 1 && kill(actual->pid, 0) == 0)
            {
                temp = anterior;
                if (actual->pid != 0)
                {
                    printf("Estos son los codigos para %d:\n", actual->pid);
                    printf("%d %d\n", actual->exit_code, actual->signal_value);
                }
                if (actual->exit_code == -1 && actual->signal_value == -1)
                {
                    eliminar_proceso(actual, anterior, cola_de_procesos);
                    siguiente = actual->siguiente;
                }
                else
                {
                    siguiente = actual->siguiente;
                }
            }
            anterior = temp;
            actual = siguiente;
        }
        if (i == 0)
        {
            for (int i = 0; i < 10; i++)
            {
                printf("%d\n", 10 - i);
                sleep(1);
            }
        }
    }

    printf("DCCAdmin finalizado\n");

    Proceso *actual = cola_de_procesos->head;

    while (actual != NULL)
    {
        print_process(actual);
        actual = actual->siguiente;
    }
    free_processes();
    exit(0);
}

// Manejador de SIGINT (Ctrl+C)
void manejar_sigint(int sig)
{
    handle_quit();
}

void free_processes()
{
    Proceso *actual = cola_de_procesos->head;
    while (actual != NULL)
    {
        Proceso *temp = actual;
        actual = actual->siguiente;
        free(temp);
    }
    free(cola_de_procesos);
}

int main(int argc, char const *argv[])
{
    if (argc > 1)
    {                                  // Si se pasó time_max
        tiempo_maximo = atoi(argv[1]); // Convertir a int
    }
    cola_de_procesos = calloc(1, sizeof(Cola));
    cola_de_procesos->head = NULL;
    cola_de_procesos->tail = NULL;

    signal(SIGINT, manejar_sigint);
    signal(SIGCHLD, manejador_sigchld);

    printf("mi_shell> ");
    fflush(stdout);

    while (1)
    {
        actualizar_procesos(cola_de_procesos);

        fd_set set;
        struct timeval timeout;

        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity;
        do
        {
            activity = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
            apply_timeouts(cola_de_procesos);
        } while (activity < 0 && errno == EINTR);

        if (activity < 0)
        {
            perror("select error");
            break;
        }
        else if (activity > 0 && FD_ISSET(STDIN_FILENO, &set))
        {
            char **input = read_user_input();

            if (input[0] == NULL)
            {
                free_user_input(input);
                continue;
            }

            //   EVENTO: QUIT
            if (strcmp(input[0], "quit") == 0)
            {
                free_user_input(input);
                handle_quit();
            }
            else if (strcmp(input[0], "start") == 0) // Comando START
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

                    signal(SIGINT, SIG_DFL);

                    execvp(input[1], &input[1]);
                    perror("Error al ejecutar el comando");
                    exit(1);
                }
                else if (pid > 0)
                { // Proceso padre
                    Proceso *nuevo_proceso = crear_proceso(pid, input[1]);

                    // correr sin segundo plano
                    int status;
                    // waitpid(pid, &status, 0);
                    // obtener_estado_proceso(nuevo_proceso, pid, status);

                    // printf("Proceso %d iniciado en segundo plano\n", pid);

                    if (cola_de_procesos->head == NULL)
                    {
                        cola_de_procesos->head = nuevo_proceso;
                    }
                    else
                    {
                        cola_de_procesos->tail->siguiente = nuevo_proceso;
                    }
                    cola_de_procesos->tail = nuevo_proceso;
                    nuevo_proceso->siguiente = NULL;
                    // printf("agregado a la cola\n");
                }
                else
                {
                    perror("Error al crear el proceso");
                }
            }
            else if (strcmp(input[0], "info") == 0) // Comando INFO
            {
                Proceso *actual = cola_de_procesos->head;
                int contador = 0;
                while (actual != NULL)
                {
                    contador++;
                    printf("\n### Proceso %d ###\n", contador);
                    printf("PID: %d \n", actual->pid);
                    printf("Nombre: %s \n", actual->nombre);
                    printf("Exit Code: %d \n", actual->exit_code);
                    printf("Signal Value: %d \n", actual->signal_value);
                    printf("Elapsed time: %.2f\n", difftime(actual->final, actual->inicio));
                    actual = actual->siguiente;
                }
            }
            else if (strcmp(input[0], "timeout") == 0) // Comando  TIMEOUT
            {
                actualizar_procesos(cola_de_procesos);
                if (input[1] == NULL || input[1][0] == '\0')
                {
                    printf("Error: Debes indicar un tiempo válido.\n");
                    continue;
                }

                char *endptr;
                long time_value = strtol(input[1], &endptr, 10);

                // Validating that the entire string was a number
                if (*endptr != '\0' || time_value <= 0)
                {
                    printf("Error: Debes indicar un tiempo válido.\n");
                    continue;
                }
                // int time_value = input[1] - '0';
                printf("time value ingresado: %ld\n", time_value);
                Proceso *actual = cola_de_procesos->head;
                actual = cola_de_procesos->head;

                bool uno_corriendo = false;
                while (actual != NULL)
                {
                    if (process_is_alive(actual))
                    {
                        uno_corriendo = true;
                        break;
                    }
                    actual = actual->siguiente;
                }

                if (!uno_corriendo)
                {
                    printf("No hay procesos en ejecución. Timeout no se puede ejecutar.\n");
                    continue;
                }

                crear_timeout(time_value);
            }
            else
            {
                printf("%s: comando no reconocido.\n", input[0]);
            }
            free_user_input(input);
            printf("mi_shell> ");
            fflush(stdout);
        }
    }
    free_processes();
    return 0;
}
