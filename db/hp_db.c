struct hp_db {
  char     *dirname;
  unsigned num_recs;
};

struct hp_db_iter {
  struct hp_db *db;
};

int
hp_db_open(struct hp_db *db, char *name)
{
}

int
hp_db_close(struct hp_db *db)
{
}

int
hp_db_insert(struct hp_db *db, unsigned key_len, char *key, unsigned data_len, char *data)
{
  chdir(db->dirname);

  fd = creat(key);
  
  write(fd, data, data_len + 1);

  close(fd);

  return (0);
}

int
hp_db_iter_first(struct hp_db *db, struct hp_db_iter *iter)
{
}

int
hp_db_iter_last(struct hp_db *db, struct hp_db_iter *iter)
{
}

int
hp_db_find(struct hp_db *db, struct hp_db_iter *iter, unsigned mode, unsigned key_len, char *key)
{
  chdir(db->dirname);

  fd = open(key, O_RD);
  if (fd < 0)  return (-1);

  close(fd);
}

int
hp_db_iter_prev(struct hp_db_iter *iter)
{
}

int
hp_db_iter_next(struct hp_db_iter *iter)
{
}

unsigned
hp_db_iter_at_end(struct hp_db_iter *iter)
{
}

int
hp_db_iter_rec(struct hp_db_iter *iter, unsigned *key_len, char *key, unsigned *data_len, char *data)
{
}

int
hp_db_erase(struct hp_db_iter *iter)
{
}

int
hp_db_cnt(struct hp_db *db, unsigned *cnt)
{
}

int
hp_db_sync(struct hp_db *db)
{
}
