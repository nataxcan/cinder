# Binary TCP for Minecraft packets (UTF-8 strings cannot carry 0xFF).
import Base

def Bytes.recv(sock: Socket, max: U32) ->
  IO(Socket & Result<&1, &1, U32 & String, List<&2, U32>>):
  import "./bytes_io.c"
  import "./bytes_io.js"

def Bytes.send(sock: Socket, data: List<&2, U32>) ->
  IO(Socket & Result<&1, &1, U32 & String, Unit>):
  import "./bytes_io.c"
  import "./bytes_io.js"
