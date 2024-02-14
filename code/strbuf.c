#include "sekai.h"
#include "strbuf.h"

void StringBuffer( strbuf_t *sb, char *buf, int length ) {
    if ( !buf || length < 1 ) {
        Com_Error( ERR_DROP, "Unable to initialize string buffer\n" );
    }

    sb->buf = buf;
    sb->cur = buf;
    sb->end = buf + length;
}

void AppendToBuffer( strbuf_t *sb, const char *s ) {
    int len;
    int size;

    size = sb->end - sb->cur;
    len = strlen( s );
    if ( len >= size ) {
        Com_Error( ERR_DROP, "String buffer overflow\n" );
    }
    Com_Memcpy( sb->cur, s, len );
    sb->cur += len;
}
