#ifndef INPUT_H
#define INPUT_H

/**
 * Inicializa el subsistema de captura de input y transmisión uplink.
 *
 * Parámetros:
 *   satellite_ip: Dirección IP del satélite (string, ej: "127.0.0.1")
 *   satellite_port: Puerto UDP del satélite para recibir input
 *
 * Responsabilidades:
 * - Crea el socket UDP de salida hacia el satélite
 * - Configura struct sockaddr_in con la IP/puerto del satélite
 * - Inicializa el bitfield interno de estado de teclas a 0
 * - Inicializa el sequence number de paquetes de input a 0
 *
 * Nota: SDL2 ya está inicializado por display_init(), por lo que el
 * subsistema de eventos ya está disponible.
 *
 * Retorna:
 *   0 en éxito, -1 en error (escribe error a stderr)
 */
int input_init(const char* satellite_ip, int satellite_port);

/**
 * Procesa eventos de input del jugador y transmite al satélite.
 *
 * Responsabilidades:
 * - Procesa todos los SDL_Event pendientes con SDL_PollEvent()
 * - Actualiza un bitfield interno con las teclas mapeadas a acciones DOOM:
 *   • Flechas arriba/abajo = avanzar/retroceder
 *   • Flechas izquierda/derecha = girar
 *   • Ctrl = disparar
 *   • Espacio = usar/abrir
 *   • Shift izquierdo/derecho = strafe
 *   • Teclas 1-7 = cambio de arma
 * - Detecta eventos de quit (tecla ESC, SDL_QUIT)
 * - Si el bitfield cambió respecto al frame anterior:
 *   • Empaqueta con pack_input() incluyendo timestamp y sequence number
 *   • Envía el paquete con sendto() al socket UDP del satélite
 *   • Incrementa el sequence number
 *
 * Retorna:
 *   1 si se solicitó quit (ESC o cierre de ventana)
 *   0 en caso contrario
 *
 * Nota: Esta función debe ser llamada en cada iteración del loop principal.
 */
int input_poll(void);

/**
 * Limpia y cierra el subsistema de input.
 *
 * Responsabilidades:
 * - Cierra el socket UDP de transmisión de input
 *
 * Debe ser llamada antes de display_shutdown().
 */
void input_shutdown(void);

#endif // INPUT_H
