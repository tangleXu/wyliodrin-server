/**************************************************************************************************
 * XMPP stuff: Wyliodrin namespace, XMPP connection port and module_fct signature
 *
 * Author: Razvan Madalin MATEI <matei.rm94@gmail.com>
 * Date last modified: July 2015
 *************************************************************************************************/

#ifndef _WXMPP_H
#define _WXMPP_H

#include <strophe.h> /* Strophe stuff */

#define WNS       "wyliodrin"  /* Wyliodrin namespace */
#define XMPP_PORT 5222         /* XMPP server port    */

/**
 * Module function signature
 */
typedef void (*module_fct)(const char *from, const char *to, int error, xmpp_stanza_t *stanza,
                           xmpp_conn_t *const conn, void *const userdata);

#endif /* _XMPP_H */
