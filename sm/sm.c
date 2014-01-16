enum xxx_sm_msg_class {
  XXX_SM_MSG_CLASS_REQ,		/* Request */
  XXX_SM_MSG_CLASS_REPLY,	/* Reply */
  XXX_SM_MSG_CLASS_IND,		/* Indication */
  XXX_SM_MSG_CLASS_CONF		/* Confirmation */
};

struct xxx_sm_timer {
  struct xxx_sm_timer *prev, *next;
  uint32              deadline;
  void                (*func)(void *);
  void                *arg;
};

struct {
  uint32 id;
  uint8  state;
  uint32 xid;
} xxx_sm_inst[1];


struct {
  int16               fd;
  uint8               msg_buf[1024];
  uint16              msg_len;
  uint32              id_last;
  uint32              xid_last;
  struct xxx_sm_timer *timer_list;
} xxx_sm_data[1];


struct xxx_sm_timer *
xxx_sm_timer_start(uint32 tmout, void (*func)(void *), void *arg)
{
  struct xxx_sm_timer *tmr;

  tmr = (struct xxx_sm_timer *) calloc(1, sizeof(*tmr));
  if (tmr == 0) {
    xxx_sm_log(XXX_SM_LOG_LVL_FATAL, "write() = %d, errno = %d", rc, errno);
    exit(1);
  }
 
  tmr->deadline = /* current time */ + tmout;
  tmr->func     = func;
  tmr->arg      = arg;

  for (q = xxx_sm_data->timer_list; q && tmr->deadline >= q->deadline; q = q->next);
  p = q->prev;

  tmr->prev = p;
  tmr->next = q;

  *(p ? &p->next : &xxx_sm_data->timer_list) = tmr;
  if (q)  q->prev = tmr;

  return (tmr);
}

void
xxx_sm_timer_cancel(struct xxx_sm_timer *tmr)
{
  p = tmr->prev;
  q = tmr->next;

  *(p ? &p->next : &xxx_sm_data->timer_list) = q;
  if (q)  q->prev = p;

  free(tmr);
}

int32
xxx_sm_timers_tmout_first(void)
{
  p = xxx_sm_data->timer_list;
  if (p == 0)  return (-1);

  t = /* current time */;

  return (t >= p->deadline ? 0 : p->deadline - t);
}

void
xxx_sm_timers_run(void)
{
  t = /* current time */;

  while ((tmr = xxx_sm_data->timer_list) && ((int32) t - (int32) tmr->deadline) >= 0) {
    (*tmr->func)(tmr->arg);

    xxx_sm_data->timer_list = tmr->next;

    free(tmr);
  }

  if (tmr)  tmr->prev = 0;
}

void
xxx_sm_msg_send(uint32 len, uint8 *data)
{
  do {
    ++xxx_sm_data->xid_last;
  } while (xxx_sm_inst_find_by_xid(xxx_sm_data->xid_last));

  xxx_sm_msg_xid_set(data, xxx_sm_data->xid_last);

  rc = write(xxx_sm_data->fd, data, len);
  if (rc < 0) {
    xxx_sm_log(XXX_SM_LOG_LVL_FATAL, "write() = %d, errno = %d", rc, errno);
    exit(1);
  }
}

void
xxx_sm_ev(struct xxx_sm_inst *inst, enum xxx_sm_ev ev, void *arg)
{
}

void
xxx_sm_inst_init(struct xxx_sm_inst *inst)
{
  do {
    ++xxx_sm_data->id_last;
  } while (xxx_sm_inst_find_by_id(xxx_sm_data->id_last));

  inst->id    = xxx_sm_data->id_last;
  inst->state = XXX_SM_STATE_INIT;
}

void
xxx_sm_inst_create(uint32 op)
{
  struct xxx_sm_inst *inst;

  inst = (struct xxx_sm_inst *) calloc(1, sizeof(*inst));
  if (inst == 0) {
    xxx_sm_log(XXX_SM_LOG_LVL_ERR, "calloc() failed");
    return;
  }

  xxx_sm_inst_init(inst);

  xxx_sm_ev(inst, op, 0);
}

void
xxx_sm_msg(void)
{
  uint32 xid = xxx_sm_msg_xid();
  uint8  op  = xxx_sm_msg_op();
  uint32 id  = xxx_sm_msg_id();

  switch (xxx_sm_msg_class()) {
  case XXX_SM_MSG_CLASS_REPLY:
  case XXX_SM_MSG_CLASS_CONF:
    inst = xxx_sm_inst_find_by_xid(xid);
    if (inst == 0) {
      xxx_sm_log(XXX_SM_LOG_LVL_WARN, "xid not found, xid = %u", xid);
      break;
    }
    xxx_sm_ev(inst, op, 0);
    break;

  default:
    if (op == CREATE) {
      xxx_sm_inst_create(op);
    } else {
      inst = xxx_sm_inst_find_by_id(id);
      if (inst == 0) {
	xxx_sm_log(XXX_SM_LOG_LVL_WARN, "id not found, id = %u", id);
	break;
      }
      xxx_sm_ev(inst, op, 0);
    }
  }
}

void
xxx_sm_forever(void)
{
  struct pollfd   pollfd[1];
  int32           tmout_ms;
  struct timespec tmout_ts[1];

  pollfd->fd = xxx_sm_data->fd;
  pollfd->events = POLLIN | POLLPRI | POLLRDHUP;

  tmout_ms = xxx_sm_timers_tmout_first();
  if (tmout_ms < 0) {
    tmout_ts->tv_sec  = -1;
    tmout_ts->tv_nsec = -1;
  } else {
    tmout_ts->tv_sec  = tmout_ms / 1000;
    tmout_ts->tv_nsec = (tmout_ms % 1000) * 1000000;
  }
  
  rc = poll(pollfd, ARRAY_SIZE(pollfd), tmout_ts, 0);
  if (rc < 0) {
    xxx_sm_log(XXX_SM_LOG_LVL_FATAL, "poll() = %d, errno = %d", rc, errno);
    exit(1);
  } else if (rc == 0) {
    xxx_sm_timers_run();
  } else {
    rc = read(xxx_sm_data->fd, xxx_sm_data->msg_buf, sizeof(xxx_sm_data->msg_buf));
    if (rc < 0) {
      xxx_sm_log(XXX_SM_LOG_LVL_FATAL, "read() = %d, errno = %d", rc, errno);
      exit(1);
    }

    xxx_sm_data->msg_len = (uint16) rc;
    xxx_sm_msg();
  }
}


void
main(void)
{
  xxx_sm_init();

  for (;;)  xxx_sm_forever();
}
