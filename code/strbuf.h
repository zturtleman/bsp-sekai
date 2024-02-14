#ifndef __STRBUF_H__
#define __STRBUF_H__

typedef struct strbuf_s {
    char    *buf, *end, *cur;
    int     length;
} strbuf_t;

void StringBuffer( strbuf_t *sb, char *buf, int length );
void AppendToBuffer( strbuf_t *sb, const char *s );

#endif /* !__STRBUF_H__ */
