#include "hp_stream.h"

struct hp_stream *
hp_stream_init(struct hp_stream *st, 
	       int (*getc)(struct hp_stream *),
	       int (*ungetc)(struct hp_stream *, char),
	       int (*putc)(struct hp_stream *, char),
	       int (*tell)(struct hp_stream *),
	       int (*seek)(struct hp_stream *, int, int)
	       )
{
  st->getc   = getc;
  st->ungetc = ungetc;
  st->putc   = putc;
  st->tell   = tell;
  st->seek   = seek;

  return (st);
}

int
hp_stream_puts(struct hp_stream *st, char *s)
{
  int  result;
  char c;

  for (result = 0; c = *s; ++s, ++result) {
    if (hp_stream_putc(st, c) < 0)  return (-1);
  }

  return (result);
}

static int
hp_stream_file_getc(struct hp_stream *st)
{
  return (fgetc(((struct hp_stream_file *) st)->fp));
}

static int
hp_stream_file_ungetc(struct hp_stream *st, char c)
{
  return (ungetc(c, ((struct hp_stream_file *) st)->fp));
}

static int
hp_stream_file_putc(struct hp_stream *st, char c)
{
  return (fputc(c, ((struct hp_stream_file *) st)->fp));
}

static int
hp_stream_file_tell(struct hp_stream *st)
{
  return (ftell(((struct hp_stream_file *) st)->fp));
}

static int
hp_stream_file_seek(struct hp_stream *st, int ofs, int whence)
{
  return (fseek(((struct hp_stream_file *) st)->fp, ofs, whence));
}

struct hp_stream_file *
hp_stream_file_init(struct hp_stream_file *st, FILE *fp)
{
  hp_stream_init(st->base,
		 hp_stream_file_getc,
		 hp_stream_file_ungetc,
		 hp_stream_file_putc,
		 hp_stream_file_tell,
		 hp_stream_file_seek
		 );

  st->fp = fp;

  return (st);
}


static int
hp_stream_buf_getc(struct hp_stream *st)
{
  struct hp_stream_buf *stb = (struct hp_stream_buf *) st;

  if (stb->ofs >= stb->bufsize)  return (-1);

  return (stb->buf[stb->ofs++]);
}

static int
hp_stream_buf_ungetc(struct hp_stream *st, char c)
{
  struct hp_stream_buf *stb = (struct hp_stream_buf *) st;

  if (stb->ofs == 0)  return (-1);

  --stb->ofs;

  return (c);
}

static int
hp_stream_buf_putc(struct hp_stream *st, char c)
{
  struct hp_stream_buf *stb = (struct hp_stream_buf *) st;

  if (stb->ofs >= stb->bufsize)  return (-1);

  stb->buf[stb->ofs++] = c;

  return (c);
}

static int
hp_stream_buf_tell(struct hp_stream *st)
{
  return (((struct hp_stream_buf *) st)->ofs);
}

static int
hp_stream_buf_seek(struct hp_stream *st, int ofs, int whence)
{
  struct hp_stream_buf *stb = (struct hp_stream_buf *) st;
  int                                                  nofs;

  switch (whence) {
  case SEEK_SET:
    nofs = ofs;
    break;
  case SEEK_CUR:
    nofs = (int) stb->ofs + ofs;
    break;
  case SEEK_END:
    nofs = (int) stb->bufsize + ofs;
    break;
  default:
    return (-1);
  }

  if (nofs < 0 || nofs > (int) stb->bufsize)  return (-1);

  return (stb->ofs = nofs);
}

struct hp_stream_buf *
hp_stream_buf_init(struct hp_stream_buf *st, char *buf, unsigned bufsize)
{
  hp_stream_init(st->base,
		 hp_stream_buf_getc,
		 hp_stream_buf_ungetc,
		 hp_stream_buf_putc,
		 hp_stream_buf_tell,
		 hp_stream_buf_seek
		 );

  st->buf     = buf;
  st->bufsize = bufsize;
  st->ofs     = 0;

  return (st);
}

