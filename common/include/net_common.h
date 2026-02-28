/*
 * net_common.h — Utilidades de red compartidas (sockets UDP)
 *
 * Responsabilidad:
 *   Abstraer operaciones de sockets UDP comunes a ambos lados
 *   (satélite y estación terrestre).
 *
 * Funciones a declarar:
 *   - net_create_udp_socket()   — Crea y configura socket UDP (non-blocking, buffer sizes, SO_REUSEADDR)
 *   - net_bind_socket()         — Vincula socket a dirección y puerto
 *   - net_send_packet()         — Envía buffer de datos por UDP a dirección destino
 *   - net_recv_packet()         — Recibe paquete UDP con timeout configurable (poll/select)
 *   - net_set_socket_options()  — Configura TOS/DSCP para QoS, tamaños de buffer del kernel
 *   - net_close_socket()        — Cierra socket y libera recursos
 *   - net_resolve_address()     — Resuelve hostname a struct sockaddr_in
 */

#ifndef DOOM_SAT_NET_COMMON_H
#define DOOM_SAT_NET_COMMON_H

/* TODO: Incluir sys/socket.h, netinet/in.h, arpa/inet.h */
/* TODO: Definir tipos de retorno de error */
/* TODO: Declarar funciones de red */

#endif /* DOOM_SAT_NET_COMMON_H */
