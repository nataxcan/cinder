// Binary TCP: List<U32> of 0..255, not UTF-8 strings.
// ================================================

static u32 cinder_u32(Term t) {
  return (u32)(u64)t;
}

static Term bytes_recv_pack(Env e, IoWork* w) {
  Term r;
  if (w->code) {
    r = io_fail(e, w->code, NULL);
  } else {
    Term xs = term_pak(CID_NIL, 0);
    for (u64 i = w->size; i > 0; i -= 1) {
      xs = io_node(e, CID_CON, ((uint8_t*)w->data)[i - 1], xs, IO_HOTS & 16);
    }
    r = io_done(e, xs);
  }
  free(w->data);
  return io_tup(e, io_hand(w->hand), r);
}

static Term bytes_recv_more(Env e, IoWork* w) {
  int fd  = (int)w->hand;
  w->size = io_sys_end(w, recv(fd, w->data, (size_t)w->made, 0));
  return w->code == EAGAIN ? io_wait_on(w, fd, POLLIN, bytes_recv_more)
    : bytes_recv_pack(e, w);
}

Term bytes_recv_run(Env e, Term* f, IoWork* w) {
  w->hand = (intptr_t)io_hand_v(f[0]);
  w->made = f[1] < INT32_MAX ? (intptr_t)f[1] : INT32_MAX;
  if (w->made > 65536) {
    w->made = 65536;
  }
  w->data = io_mem(malloc((size_t)w->made + 1));
  return bytes_recv_more(e, w);
}

static Term bytes_send_more(Env e, IoWork* w) {
  int fd = (int)w->hand;
  while (w->code == 0 && (u64)w->made < w->size) {
    ssize_t n = send(fd, (char*)w->data + w->made, w->size - (u64)w->made, 0);
    if (n < 0 && errno == EAGAIN) {
      return io_wait_on(w, fd, POLLOUT, bytes_send_more);
    }
    w->made += io_sys_end(w, n);
  }
  Term r = w->code != 0 ? io_fail(e, w->code, NULL)
    : io_done(e, term_pak(CID_UNIT, 0));
  free(w->data);
  return io_tup(e, io_hand(w->hand), r);
}

Term bytes_send_run(Env e, Term* f, IoWork* w) {
  u64 cap = 256;
  u64 n = 0;
  char* data = io_mem(malloc(cap));
  Term s = f[1];
  while (term_aux(s) == CID_CON) {
    Term fb[2];
    spare_free(e, cls_fit(2), ctr_take(e, s, 2, fb));
    if (n == cap) {
      cap *= 2;
      data = io_mem(realloc(data, cap));
    }
    data[n] = (uint8_t)cinder_u32(fb[0]);
    n += 1;
    s = fb[1];
  }
  w->hand = (intptr_t)io_hand_v(f[0]);
  w->data = data;
  w->size = n;
  w->made = 0;
  w->code = 0;
  return bytes_send_more(e, w);
}

static void __attribute__((constructor)) bytes_io_use(void) {
  io_eff(CID_BYTES_RECV, bytes_recv_run, IO_READ);
  io_eff(CID_BYTES_SEND, bytes_send_run, 0);
}
