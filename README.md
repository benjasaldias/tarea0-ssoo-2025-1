## Instrucciones para Correr
1. make
2. ./dccAdmin

Estoy usando harto Valgrind todavía por los errores de memoria, pero estoy bastante seguro de que ya no quedan.

## Variables
*Proceso: puntero a un struct proceso que tiene:
- pid
- tiempo de comienzo de ejecución
- proceso *siguiente (proceso que le sigue en la lista ligada)

Cola* cola_de_procesos: lista ligada de los procesos creados, tiene:
- head
- tail

## Funciones
**crear_proceso:** crea el proceso ejecutado por start _executable_ y le hace allocation de memoria. Recibe el pid del proceso como argumento (retornado por execvp).

**eliminar_proceso:** recibe un proceso y el proceso que le antecede en la lista ligada (si es que tiene) y lo elimina de la lista ligada. Luego libera su espacio de memoria.

**actualizar_procesos:** recorre la cola_de_procesos revisando si cada uno sigue activo con kill(proceso->pid, 0) == 0 (retorna 1 si está activo). Si no lo están, los elimina con eliminar_proceso.

**enviar_advertencia:** recibe un proceso como argumento y le envía una advertencia de que debe finalizar sus actividades en 5 segundos máximo.
