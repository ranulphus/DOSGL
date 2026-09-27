/* log.c - serial log lines and the error string. */
#include "dgl.h"
#include "mga/serial.h"
#include <stdarg.h>
#include <stdio.h>

static char dgl_error[160] = "no error";

void dgl_logf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    if (!serial_enabled())
        serial_init(SERIAL_COM1, 115200);
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    serial_puts(buf);
    serial_puts("\n");
}

void dgl_set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dgl_error, sizeof dgl_error, fmt, ap);
    va_end(ap);
    DGL_ERR("DGL-ERROR %s", dgl_error);
}

const char *dglGetErrorString(void)
{
    return dgl_error;
}
