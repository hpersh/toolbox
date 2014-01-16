enum hp_sm_msg_class {
  HP_SM_MSG_CLASS_REQ,		/* Request */
  HP_SM_MSG_CLASS_REPLY,	/* Reply */
  HP_SM_MSG_CLASS_IND,		/* Indication */
  HP_SM_MSG_CLASS_CONF		/* Confirmation */
};

enum hp_sm_timer_mode {
  HP_SM_TIMER_MODE_SINGLE,
  HP_SM_TIMER_MODE_PERIODIC
};

struct hp_sm_timer {
  struct hp_sm_timer    *prev, *next;
  enum hp_sm_timer_mode mode;
  uint32                 tmout;
  uint32                 deadline;
  void                   (*func)(struct hp_sm_timer *);
};

struct hp_sm_inst_timer {
  struct hp_sm_timer    base[1];
  uint32                 inst_id;
  void                   (*func)(struct hp_sm_inst_timer *);
};

struct hp_sm_inst {
  uint32 id;
  uint32 xid;
};

struct hp_sm_inst_index {
};

struct hp_sm_inst *hp_sm_inst_find(struct hp_sm_inst_index *index, uint32 key);

struct xm_sm_class {
  int16  fd;
  uint8  msg_buf[1024];
  uint16 msg_len;
  uint32 id_last;
  uint32 xid_last;
  struct {
    struct hp_sm_inst_index by_id[1];
    struct hp_sm_inst_index by_xid[1];
  } inst_index[1];
  struct hp_sm_timer *timer_list;
};

void
hp_sm_timer_insert(struct hp_sm_class *cl,
		    struct hp_sm_timer *tmr
		    )
{
  for (p = 0, q = cl->timer_list; q && tmr->deadline >= q->deadline; q = (p = q)->next);

  tmr->prev = p;
  tmr->next = q;

  *(p ? &p->next : &cl->timer_list) = tmr;
  if (q)  q->prev = tmr;
}

struct hp_sm_timer *
hp_sm_timer_start(struct hp_sm_class    *cl,
		   struct hp_sm_timer    *tmr
		   enum hp_sm_timer_mode mode,
		   uint32                 tmout,
		   void                   (*func)(struct hp_sm_timer *)
		   )
{
  tmr->mode  = mode;
  tmr->tmout = tmout;
  tmr->func  = func;

  tmr->deadline = hp_sm_time() + tmr->tmout;

  hp_sm_timer_insert(cl, tmr);

  return (tmr);
}

void
hp_sm_inst_timer_tmout(struct hp_sm_class      *cl,
			struct hp_sm_inst_timer *tmr)
{
  inst = hp_sm_inst_find(cl->inst_index->by_id, tmr->id);
  if (inst == 0) {
    hp_sm_log(HP_SM_LOG_LVL_WARN, "Stale instance timer");
    return;
  }

  hp_sm_ev(inst, TMOUT, tmr);
}

struct hp_sm_inst_timer *
hp_sm_inst_timer_start(struct hp_sm_class      *cl,
			struct hp_sm_inst_timer *tmr,
			struct hp_sm_inst       *inst,
			enum hp_sm_timer_mode   mode,
			uint32                   tmout,
			void                     (*func)(struct hp_sm_inst_timer *)
			)
{
  tmr->inst_id = inst->id;
  tmr->func    = func;

  hp_sm_timer_start(cl, tmr->base, mode, tmout, hp_sm_inst_timer_tmout);

  return (tmr);
}

void
hp_sm_timer_cancel(struct hp_sm_class *cl,
		    struct hp_sm_timer *tmr)
{
  p = tmr->prev;
  q = tmr->next;

  *(p ? &p->next : &cl->timer_list) = q;
  if (q)  q->prev = p;
}

int32
hp_sm_timers_tmout_first(struct hp_sm_class *cl)
{
  p = cl->timer_list;
  if (p == 0)  return (-1);

  t = hp_sm_time();

  return (((int32) t - (int32) p->deadline) >= 0 ? 0 : p->deadline - t);
}

void
hp_sm_timers_run(struct hp_sm_class *cl)
{
  t = /* current time */;

  while ((tmr = cl->timer_list) && ((int32) t - (int32) tmr->deadline) >= 0) {
    (*tmr->func)(tmr);

    q = tmr->next;
    if (q)  q->prev = 0;
    cl->timer_list = q;

    if (tmr->mode == HP_SM_TIMER_MODE_PERIODIC) {
      tmr->deadline += tmr->tmout;

      hp_sm_timer_insert(cl, tmr);
    }
  }
}

void
hp_sm_msg_send(uint32 len, uint8 *data)
{
  do {
    ++hp_sm_data->xid_last;
  } while (hp_sm_inst_find(hp_sm_data->inst_index->by_xid, hp_sm_data->xid_last));

  hp_sm_msg_xid_set(data, hp_sm_data->xid_last);

  rc = write(hp_sm_data->fd, data, len);
  if (rc < 0) {
    hp_sm_log(HP_SM_LOG_LVL_FATAL, "write() = %d, errno = %d", rc, errno);
    exit(1);
  }
}

void
hp_sm_ev(struct hp_sm_inst *inst, enum hp_sm_ev ev, void *arg)
{
}

void
hp_sm_inst_init(struct hp_sm_inst *inst)
{
  do {
    ++hp_sm_data->id_last;
  } while (hp_sm_inst_find(hp_sm_data->inst_index->by_id, hp_sm_data->id_last));

  inst->id    = hp_sm_data->id_last;
  inst->state = HP_SM_STATE_INIT;
}

void
hp_sm_inst_create(uint32 op)
{
  struct hp_sm_inst *inst;

  inst = hp_sm_inst_alloc();
  if (inst == 0) {
    hp_sm_log(HP_SM_LOG_LVL_ERR, "hp_sm_inst_alloc() failed");
    return;
  }

  hp_sm_inst_init(inst);

  hp_sm_ev(inst, op, 0);
}

void hp_sm_inst_free(struct hp_sm_inst *inst);

void
hp_sm_msg(void)
{
  switch (hp_sm_msg_class()) {
  case HP_SM_MSG_CLASS_REPLY:
  case HP_SM_MSG_CLASS_CONF:
    xid  = hp_sm_msg_xid();
    inst = hp_sm_inst_find(hp_sm_data->inst_index->by_xid, xid);
    if (inst == 0) {
      hp_sm_log(HP_SM_LOG_LVL_WARN, "xid not found, xid = %u", xid);
      break;
    }
    hp_sm_ev(inst, op, 0);
    break;

  default:
    op = hp_sm_msg_op();

    if (op == CREATE) {
      hp_sm_inst_create(op);
    } else {
      id   = hp_sm_msg_id();
      inst = hp_sm_inst_find(hp_sm_data->inst_index->by_id, id);
      if (inst == 0) {
	hp_sm_log(HP_SM_LOG_LVL_WARN, "id not found, id = %u", id);
	break;
      }
      hp_sm_ev(inst, op, 0);
    }
  }
}

void
hp_sm_forever(void)
{
  struct pollfd   pollfd[1];
  int32           tmout_ms;
  struct timespec tmout_ts[1];

  pollfd->fd = hp_sm_data->fd;
  pollfd->events = POLLIN | POLLPRI | POLLRDHUP;

  tmout_ms = hp_sm_timers_tmout_first();
  if (tmout_ms < 0) {
    tmout_ts->tv_sec  = -1;
    tmout_ts->tv_nsec = -1;
  } else {
    tmout_ts->tv_sec  = tmout_ms / 1000;
    tmout_ts->tv_nsec = (tmout_ms % 1000) * 1000000;
  }
  
  rc = poll(pollfd, ARRAY_SIZE(pollfd), tmout_ts, 0);
  if (rc < 0) {
    hp_sm_log(HP_SM_LOG_LVL_FATAL, "poll() = %d, errno = %d", rc, errno);
    exit(1);
  } else if (rc == 0) {
    hp_sm_timers_run();
  } else {
    rc = read(hp_sm_data->fd, hp_sm_data->msg_buf, sizeof(hp_sm_data->msg_buf));
    if (rc < 0) {
      hp_sm_log(HP_SM_LOG_LVL_FATAL, "read() = %d, errno = %d", rc, errno);
      exit(1);
    }

    hp_sm_data->msg_len = (uint16) rc;
    hp_sm_msg();
  }
}


void
main(void)
{
  hp_sm_init();

  for (;;)  hp_sm_forever();
}
