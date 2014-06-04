#include <stdio.h>

struct hp_stream {
  int (*getc)(struct hp_stream *);
  int (*ungetc)(struct hp_stream *, char);
  int (*putc)(struct hp_stream *, char);
  int (*tell)(struct hp_stream *);
  int (*seek)(struct hp_stream *, int, int);
  int (*eof)(struct hp_stream *);
};

struct hp_stream *hp_stream_init(struct hp_stream *st, 
				 int (*getc)(struct hp_stream *),
				 int (*ungetc)(struct hp_stream *, char),
				 int (*putc)(struct hp_stream *, char),
				 int (*tell)(struct hp_stream *),
				 int (*seek)(struct hp_stream *, int, int),
				 int (*eof)(struct hp_stream *)
				 );

static inline
int hp_stream_getc(struct hp_stream *st)
{
  return ((*st->getc)(st));
}

static inline
int hp_stream_ungetc(struct hp_stream *st, char c)
{
  return ((*st->ungetc)(st, c));
}

static inline
int hp_stream_putc(struct hp_stream *st, char c)
{
  return ((*st->putc)(st, c));
}

int hp_stream_gets(struct hp_stream *st, char *buf, unsigned bufsize);
int hp_stream_puts(struct hp_stream *st, char *s);

static inline
int hp_stream_tell(struct hp_stream *st)
{
  return ((*st->tell)(st));
}

static inline
int hp_stream_seek(struct hp_stream *st, int ofs, int whence)
{
  return ((*st->seek)(st, ofs, whence));
}

static inline
int hp_stream_eof(struct hp_stream *st)
{
  return ((*st->eof)(st));
}

struct hp_stream_file {
  struct hp_stream base[1];
  FILE             *fp;
};

struct hp_stream_file *hp_stream_file_init(struct hp_stream_file *st, FILE *fp);

struct hp_stream_buf {
  struct hp_stream base[1];
  char             *buf;
  unsigned         bufsize;
  unsigned         ofs;
};

struct hp_stream_buf *hp_stream_buf_init(struct hp_stream_buf *st, char *buf, unsigned bufsize);

