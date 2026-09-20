// Reference dump of the vanilla 26.2 random sources and coherent noise, used by
// noise_check.sh to prove src/vanilla_rand_noise.c is bit-equal.
//
// Compile/run (see run.sh for the classpath recipe):
//   javac -nowarn -cp "$CP" -d /tmp/nref bench/parity/noise_ref.java
//   java -cp /tmp/nref:"$CP" noise_ref
//
// Every floating point value is printed as raw bits (Java's %x of
// Double.doubleToRawLongBits / Float.floatToRawIntBits) so the C side can be
// compared byte-for-byte without depending on printf's decimal formatting.

import java.io.PrintStream;
import java.util.Random;
import net.minecraft.SharedConstants;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.DensityFunction;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
import net.minecraft.world.level.levelgen.RandomSupport;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.synth.BlendedNoise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;

public class noise_ref {
  /* Bootstrap.bootStrap() swaps System.out for a log4j-backed stream that prefixes
     every line; keep the original so the dump stays plain. */
  static final PrintStream OUT = System.out;

  static void pi(String key, int v) {
    OUT.printf("i %s %08x\n", key, v);
  }

  static void pl(String key, long v) {
    OUT.printf("l %s %016x\n", key, v);
  }

  static void pf(String key, float v) {
    OUT.printf("f %s %08x\n", key, Float.floatToRawIntBits(v));
  }

  static void pd(String key, double v) {
    OUT.printf("d %s %016x\n", key, Double.doubleToRawLongBits(v));
  }

  static void section(String name) {
    OUT.println("== " + name + " ==");
  }

  /* 20 integer block positions shared by the NormalNoise and BlendedNoise dumps. */
  static int blockX(int i) {
    return i * 16 - 48;
  }

  static int blockY(int i) {
    return 64 + i * 7;
  }

  static int blockZ(int i) {
    return i * 13 - 40;
  }

  public static void main(String[] args) {
    /* Registries must exist before DensityFunction/NormalNoise statics load; the
       version has to be detected first (the server jar carries version.json). */
    SharedConstants.tryDetectVersion();
    Bootstrap.bootStrap();

    /* ---------------------------------------------------------------- (a) */
    section("xoroshiro seed 1234");
    XoroshiroRandomSource xs = new XoroshiroRandomSource(1234L);
    for (int i = 0; i < 8; i++) pl("xoro_long_" + i, xs.nextLong());
    for (int i = 0; i < 8; i++) pi("xoro_int_" + i, xs.nextInt());
    for (int i = 0; i < 8; i++) pi("xoro_bound7_" + i, xs.nextInt(7));
    for (int i = 0; i < 8; i++) pi("xoro_bound128_" + i, xs.nextInt(128));
    for (int i = 0; i < 8; i++) pf("xoro_float_" + i, xs.nextFloat());
    for (int i = 0; i < 8; i++) pd("xoro_double_" + i, xs.nextDouble());

    /* ------------------------------------------- (a2) positional factories */
    section("xoroshiro positional");
    PositionalRandomFactory fac = new XoroshiroRandomSource(777L).forkPositional();
    int[][] positions = {{0, 0, 0}, {1, 2, 3}, {-5, 100, -17}, {334, 64, -1024}, {1234567, -300, 987654}};
    for (int i = 0; i < positions.length; i++) {
      RandomSource at = fac.at(positions[i][0], positions[i][1], positions[i][2]);
      pl("xoro_at_long_" + i, at.nextLong());
      pi("xoro_at_bound7_" + i, at.nextInt(7));
      pd("xoro_at_double_" + i, at.nextDouble());
    }
    RandomSource hashed = fac.fromHashOf("minecraft:overworld");
    for (int i = 0; i < 4; i++) pl("xoro_hash_long_" + i, hashed.nextLong());
    RandomSource hashedOct = fac.fromHashOf("octave_-8");
    for (int i = 0; i < 4; i++) pi("xoro_hash_oct_bound1000_" + i, hashedOct.nextInt(1000));
    RandomSupport.Seed128bit rawM8 = RandomSupport.seedFromHashOf("octave_-8");
    pl("seed_hash_lo_octave_m8", rawM8.seedLo());
    pl("seed_hash_hi_octave_m8", rawM8.seedHi());
    RandomSupport.Seed128bit raw3 = RandomSupport.seedFromHashOf("octave_3");
    pl("seed_hash_lo_octave_3", raw3.seedLo());
    pl("seed_hash_hi_octave_3", raw3.seedHi());
    RandomSupport.Seed128bit rawEmpty = RandomSupport.seedFromHashOf("");
    pl("seed_hash_lo_empty", rawEmpty.seedLo());
    pl("seed_hash_hi_empty", rawEmpty.seedHi());

    /* ---------------------------------------------------------------- (b) */
    double[][] amplitudeSets = {{1.0}, {1.0, 1.0, 1.0, 1.0}, {1.5, 0.0, 1.0, 0.0, 0.0, 1.0}};
    section("normal noise (seed 1234, firstOctave -8)");
    for (int s = 0; s < amplitudeSets.length; s++) {
      NormalNoise nn = NormalNoise.create(new XoroshiroRandomSource(1234L), -8, amplitudeSets[s]);
      pd("nn" + s + "_max", nn.maxValue());
      for (int i = 0; i < 20; i++) {
        pd("nn" + s + "_v_" + i, nn.getValue(blockX(i) + 0.25, blockY(i) + 0.5, blockZ(i) + 0.75));
      }
    }

    /* Zero-amplitude octaves: the overworld noise files carry these, and the
       skip path (levels created only for non-zero amplitudes, plus the
       2^(n-1)/(2^n - 1) value factor) is easy to get wrong. */
    double[][] zeroAmplitudeSets = {
      {1.0, 1.0, 0.0, 1.0},
      {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0},
      {0.0, 1.0},
      {1.0, 0.0},
      {0.0, 1.0, 0.0, 0.0, 1.0, 1.0}
    };
    section("normal noise zero amplitudes");
    for (int s = 0; s < zeroAmplitudeSets.length; s++) {
      NormalNoise nn = NormalNoise.create(new XoroshiroRandomSource(1234L), -8, zeroAmplitudeSets[s]);
      pd("nz" + s + "_max", nn.maxValue());
      for (int i = 0; i < 20; i++) {
        pd("nz" + s + "_v_" + i, nn.getValue(blockX(i) + 0.25, blockY(i) + 0.5, blockZ(i) + 0.75));
      }
    }

    /* Same amplitudes, different firstOctave: the offset of the zero octave moves. */
    double[] shiftedAmps = {1.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    section("normal noise shifted first octave");
    for (int s = 0; s < 3; s++) {
      int firstOctave = -8 + s * 2;
      NormalNoise nn = NormalNoise.create(new XoroshiroRandomSource(1234L), firstOctave, shiftedAmps);
      pd("ns" + s + "_max", nn.maxValue());
      for (int i = 0; i < 20; i++) {
        pd("ns" + s + "_v_" + i, nn.getValue(blockX(i) + 0.25, blockY(i) + 0.5, blockZ(i) + 0.75));
      }
    }

    /* ---------------------------------------------------------------- (c) */
    section("blended noise (seed 99)");
    BlendedNoise bn = new BlendedNoise(new XoroshiroRandomSource(99L), 0.25, 0.125, 80.0, 160.0, 8.0);
    pd("bn_max", bn.maxValue());
    for (int i = 0; i < 20; i++) {
      pd("bn_v_" + i, bn.compute(new DensityFunction.SinglePointContext(blockX(i), blockY(i), blockZ(i))));
    }

    /* ---------------------------------------------------------------- (d) */
    section("legacy source (seed 42)");
    LegacyRandomSource lr = new LegacyRandomSource(42L);
    for (int i = 0; i < 8; i++) pi("legacy_int_" + i, lr.nextInt());
    for (int i = 0; i < 8; i++) pi("legacy_bound1000_" + i, lr.nextInt(1000));
    for (int i = 0; i < 8; i++) pi("legacy_bound7_" + i, lr.nextInt(7));
    for (int i = 0; i < 8; i++) pi("legacy_bound1024_" + i, lr.nextInt(1024));
    for (int i = 0; i < 8; i++) pf("legacy_float_" + i, lr.nextFloat());
    for (int i = 0; i < 8; i++) pd("legacy_double_" + i, lr.nextDouble());
    for (int i = 0; i < 8; i++) pl("legacy_long_" + i, lr.nextLong());

    section("legacy consumeCount");
    LegacyRandomSource cr = new LegacyRandomSource(42L);
    cr.consumeCount(262);
    for (int i = 0; i < 4; i++) pi("consume_int_" + i, cr.nextInt());
    for (int i = 0; i < 4; i++) pl("consume_long_" + i, cr.nextLong());
    for (int i = 0; i < 4; i++) pd("consume_double_" + i, cr.nextDouble());

    /* Wide sweep: negative coordinates, and coordinates large enough to exercise
       PerlinNoise.wrap (|x / 3.3554432E7| near/over 0.5). */
    section("noise coordinate sweep");
    long[] sweepSeeds = {0L, 1L, -1L, 123456789012345L};
    int[] sweepX = {0, -1, 1, -12345, 987654, 33554432, 33554433, -33554432, 67108864, 1000000000};
    int[] sweepY = {-64, 0, 63, 64, 320, -33554432, 33554432};
    int[] sweepZ = {7, -7, 12345678, -98765432, 33554431, -1000000000};
    for (int si = 0; si < sweepSeeds.length; si++) {
      NormalNoise nn = NormalNoise.create(new XoroshiroRandomSource(sweepSeeds[si]), -8, shiftedAmps);
      BlendedNoise sbn = new BlendedNoise(new XoroshiroRandomSource(sweepSeeds[si]), 1.0, 1.0, 80.0, 160.0, 8.0);
      for (int i = 0; i < sweepX.length; i++) {
        for (int j = 0; j < sweepY.length; j++) {
          for (int k = 0; k < sweepZ.length; k++) {
            pd("sw" + si + "_nn_" + i + "_" + j + "_" + k,
               nn.getValue(sweepX[i] + 0.5, sweepY[j] - 0.25, sweepZ[k] + 0.125));
          }
        }
      }
      for (int i = 0; i < sweepX.length; i++) {
        for (int j = 0; j < sweepY.length; j++) {
          pd("sw" + si + "_bn_" + i + "_" + j, sbn.compute(
             new DensityFunction.SinglePointContext(sweepX[i], sweepY[j], sweepZ[i % sweepZ.length])));
        }
      }
    }
    /* positional factory across many coordinates, including negative */
    section("positional sweep");
    PositionalRandomFactory sfac = new XoroshiroRandomSource(-424242L).forkPositional();
    for (int x = -2; x <= 2; x++) {
      for (int y = -2; y <= 2; y++) {
        for (int z = -2; z <= 2; z++) {
          RandomSource at = sfac.at(x * 1000003, y * 7919, z * 104729);
          pl("ps_" + x + "_" + y + "_" + z, at.nextLong());
          pi("psb_" + x + "_" + y + "_" + z, at.nextInt(13));
        }
      }
    }

    section("worldgen large feature / decoration seeds");
    WorldgenRandom lfs = new WorldgenRandom(new LegacyRandomSource(0L));
    lfs.setLargeFeatureSeed(1L, -3, 7);
    for (int i = 0; i < 8; i++) pl("lfs_long_" + i, lfs.nextLong());
    for (int i = 0; i < 4; i++) pi("lfs_bound1000_" + i, lfs.nextInt(1000));

    WorldgenRandom dec = new WorldgenRandom(new LegacyRandomSource(0L));
    dec.setDecorationSeed(1L, -3, 7);
    for (int i = 0; i < 8; i++) pl("dec_long_" + i, dec.nextLong());
    for (int i = 0; i < 4; i++) pi("dec_bound1000_" + i, dec.nextInt(1000));

    section("java.util.Random.nextLong(bound)");
    Random jr = new Random(42L);
    for (int i = 0; i < 8; i++) pl("jl_bound1000_" + i, jr.nextLong(1000));
    for (int i = 0; i < 8; i++) pl("jl_bound1024_" + i, jr.nextLong(1024));
    for (int i = 0; i < 8; i++) pl("jl_bound7_" + i, jr.nextLong(7));
    for (int i = 0; i < 4; i++) pl("jl_bound1_" + i, jr.nextLong(1));
    for (int i = 0; i < 4; i++) pl("jl_bound3_" + i, jr.nextLong(3));
    for (int i = 0; i < 4; i++) pl("jl_boundmaxint_" + i, jr.nextLong(Integer.MAX_VALUE));
    for (int i = 0; i < 4; i++) pl("jl_bound2p62_" + i, jr.nextLong(1L << 62));
    for (int i = 0; i < 4; i++) pl("jl_boundmaxlong_" + i, jr.nextLong(Long.MAX_VALUE));
    /* Bounds whose rejection loop triggers with probability 1/4..1/2, so the
       retry path is actually exercised (see the C-side comment). */
    for (int i = 0; i < 12; i++) pl("jl_boundreject_" + i, jr.nextLong(4611686018427387905L));
    for (int i = 0; i < 12; i++) pl("jl_boundreject3_" + i, jr.nextLong(2305843009213693953L));
    for (int i = 0; i < 12; i++) pl("jl_boundreject2_" + i, jr.nextLong(3074457345618258603L));

    section("xoroshiro high-rejection bounds");
    XoroshiroRandomSource xr = new XoroshiroRandomSource(2024L);
    for (int i = 0; i < 12; i++) pi("xr_reject_" + i, xr.nextInt(1431655766));
    for (int i = 0; i < 12; i++) pi("xr_reject2_" + i, xr.nextInt(1073741825));
    for (int i = 0; i < 12; i++) pi("xr_reject3_" + i, xr.nextInt(1000000007));
    for (int i = 0; i < 12; i++) pi("xr_pow2_" + i, xr.nextInt(1073741824));
    for (int i = 0; i < 12; i++) pi("xr_bound1_" + i, xr.nextInt(1));
  }
}
