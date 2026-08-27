#ifndef CWEB_HTTP_H
#define CWEB_HTTP_H

#include "cweb/app.h"

/* Run the HTTP server. Blocks until cweb_app_stop is called.        */
int cweb_http_serve(cweb_app *app);

#endif
