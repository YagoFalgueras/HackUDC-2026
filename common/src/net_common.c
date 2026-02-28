/*
 * net_common.c — Helpers de sockets UDP
 *
 * Implementar:
 *
 * net_create_udp_socket():
 *   - Llama socket(AF_INET, SOCK_DGRAM, 0)
 *   - Configura non-blocking con fcntl(O_NONBLOCK)
 *   - Establece SO_REUSEADDR
 *   - Configura tamaños de buffer de envío/recepción del kernel (SO_SNDBUF, SO_RCVBUF)
 *   - Retorna el file descriptor del socket
 *
 * net_bind_socket():
 *   - Llama bind() con la dirección y puerto proporcionados
 *   - Retorna código de error si falla
 *
 * net_send_packet():
 *   - Llama sendto() con el buffer, tamaño y dirección destino
 *   - Maneja EAGAIN/EWOULDBLOCK para sockets non-blocking
 *
 * net_recv_packet():
 *   - Usa poll() o select() con timeout configurable
 *   - Llama recvfrom() si hay datos disponibles
 *   - Retorna bytes leídos o -1 si timeout
 *
 * net_set_socket_options():
 *   - Configura IP_TOS para DSCP/QoS (prioridad del tráfico)
 *   - Ajusta tamaños de buffer del kernel según necesidad
 *
 * net_close_socket():
 *   - Llama close() sobre el file descriptor
 *
 * net_resolve_address():
 *   - Resuelve hostname a struct sockaddr_in usando getaddrinfo()
 *   - Rellena la estructura con IP y puerto
 */

#include "net_common.h"

/* TODO: Implementar funciones */
