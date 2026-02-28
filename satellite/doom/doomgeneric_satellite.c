/*
 * doomgeneric_satellite.c — Backend de doomgeneric para el entorno del satélite
 *
 * Responsabilidad:
 *   Implementar los callbacks requeridos por la capa de abstracción doomgeneric
 *   para que DOOM funcione headless en el satélite.
 *
 * Callbacks a implementar:
 *
 * DG_Init():
 *   - Reserva framebuffer QCIF (176x144 x 4 bytes RGBA, o x3 RGB)
 *   - Asigna DG_ScreenBuffer al framebuffer reservado
 *   - Configura sin audio (no inicializa subsistema de sonido)
 *   - Almacena referencia al frame_manager para submit de frames
 *
 * DG_DrawFrame():
 *   - Callback invocado cuando DOOM tiene un frame listo para mostrar
 *   - Copia DG_ScreenBuffer al frame_manager vía frame_manager_submit_frame()
 *   - Si es necesario, convierte de RGBA a RGB antes de submit
 *   - El encoder thread consumirá el frame de forma asíncrona
 *
 * DG_SleepMs(ms):
 *   - Implementa sleep preciso usando clock_nanosleep(CLOCK_MONOTONIC)
 *   - Necesario para mantener el game loop a 35 Hz
 *   - Usa TIMER_ABSTIME para mejor precisión que usleep/nanosleep relativo
 *
 * DG_GetTicksMs():
 *   - Retorna tiempo en milisegundos desde inicio del proceso
 *   - Usa clock_gettime(CLOCK_MONOTONIC) para monotonía garantizada
 *   - Calcula: (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000)
 *
 * DG_GetKey(pressed, key):
 *   - Lee el siguiente evento de tecla de la cola de input
 *   - La cola es alimentada por uplink_rx vía doom_inject_input()
 *   - Retorna 1 si hay evento, 0 si la cola está vacía
 *   - pressed: 1 = tecla presionada, 0 = tecla soltada
 *
 * DG_SetWindowTitle(title):
 *   - No-op (no hay ventana en modo headless)
 *   - Opcionalmente logear el título para debug
 */

/* TODO: Incluir doomgeneric.h, frame_manager.h, config.h */
/* TODO: Implementar callbacks */
