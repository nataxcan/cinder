# Cinder: a GPU-native Minecraft server written in Bend.
import Base
import ./server.bend as Srv

def main() -> IO(Unit):
  Srv.run()
