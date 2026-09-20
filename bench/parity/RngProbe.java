import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.RandomSupport;

/**
 * Prints the decoration seed and the first draws of the feature RNG for the
 * canonical reference chunk (world seed 1, chunk 16,8). Cinder reproduces this
 * with bench/parity/rng_probe.c; the two must agree digit for digit.
 */
public class RngProbe {
    public static void main(String[] args) {
        long worldSeed = args.length > 0 ? Long.parseLong(args[0]) : 1L;
        int x = args.length > 1 ? Integer.parseInt(args[1]) : 256;
        int z = args.length > 2 ? Integer.parseInt(args[2]) : 128;
        WorldgenRandom random = new WorldgenRandom(new XoroshiroRandomSource(RandomSupport.generateUniqueSeed()));
        long decoration = random.setDecorationSeed(worldSeed, x, z);
        System.out.println("decorationSeed=" + decoration);
        int[] steps = {0, 1, 2, 6, 9, 10};
        int[] indices = {0, 1, 5, 10, 20, 33, 40};
        for (int step : steps) {
            for (int index : indices) {
                random.setFeatureSeed(decoration, index, step);
                StringBuilder sb = new StringBuilder();
                for (int i = 0; i < 5; i++) {
                    sb.append(random.nextInt(16)).append(i == 4 ? "" : " ");
                }
                sb.append(" | floats");
                for (int i = 0; i < 3; i++) {
                    sb.append(' ').append(Float.floatToIntBits(random.nextFloat()));
                }
                sb.append(" | longs ");
                for (int i = 0; i < 2; i++) {
                    sb.append(' ').append(random.nextLong());
                }
                System.out.println("step=" + step + " index=" + index + " " + sb);
            }
        }
    }
}
