// Binary TCP: List of 0..255, not UTF-8 strings.

function bytes_list(u8, n) {
  let xs = { $: "Nil" };
  for (let i = n - 1; i >= 0; i -= 1) {
    xs = { $: "Con", head: u8[i], tail: xs };
  }
  return xs;
}

function bytes_flat(xs) {
  const out = [];
  let cur = xs;
  while (cur && cur.$ === "Con") {
    out.push(Number(cur.head) & 255);
    cur = cur.tail;
  }
  return Uint8Array.from(out);
}

function bytes_recv(socket, max, k) {
  const sys = io_sys();
  const fd = socket;
  const cap = Math.max(1, Math.min(Number(max), 65536));
  const b = new Uint8Array(cap);
  const again = sys.mac ? 35 : 11;
  const go = () => {
    const n = Number(sys.recv(fd, sys.ptr(b), cap, 0));
    if (n < 0) {
      const code = sys.errno();
      if (code === again) {
        io_park_on(fd, false, k, go);
        return undefined;
      }
      return io_tup(socket, io_fail(code));
    }
    return io_tup(socket, io_done(bytes_list(b, n)));
  };
  return go();
}

function bytes_recv_need() {
  return { read: true };
}

function bytes_send(socket, data, k) {
  const sys = io_sys();
  const fd = socket;
  const b = bytes_flat(data);
  const again = sys.mac ? 35 : 11;
  const go = (at) => {
    while (at < b.length) {
      const part = b.subarray(at);
      const n = Number(sys.send(fd, sys.ptr(part), part.length, 0));
      if (n < 0) {
        const code = sys.errno();
        if (code === again) {
          io_park_on(fd, true, k, () => go(at));
          return undefined;
        }
        return io_tup(socket, io_fail(code));
      }
      at += n;
    }
    return io_tup(socket, io_done({ $: "Unit" }));
  };
  return go(0);
}
