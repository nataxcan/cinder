import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.RandomSupport;
import net.minecraft.util.RandomSource;

/**
 * Simulates just the placement part of a placed feature for one chunk: the
 * decoration seed, the feature seed, and then the draws the modifier chain makes
 * (count -> in_square). The positions it prints are where vanilla would try to
 * place that feature, which is what the C port must reproduce before any feature
 * logic runs.
 *
 *   probe26.sh PlacementProbe <cx> <cz> <step> <index> <counts...>
 *
 * <counts...> are the candidate count values in weighted-list order, e.g. 10 11
 * for the tree features (weights 9 and 1).
 */
public class PlacementProbe {
    public static void main(String[] args) {
        int cx = Integer.parseInt(args[0]);
        int cz = Integer.parseInt(args[1]);
        int step = Integer.parseInt(args[2]);
        int index = Integer.parseInt(args[3]);
        int[] candidates = new int[args.length - 4];
        for (int i = 4; i < args.length; i++) {
            candidates[i - 4] = Integer.parseInt(args[i]);
        }

        WorldgenRandom random = new WorldgenRandom(new XoroshiroRandomSource(RandomSupport.generateUniqueSeed()));
        long worldSeed = 1L;
        long decoration = random.setDecorationSeed(worldSeed, cx * 16, cz * 16);
        random.setFeatureSeed(decoration, index, step);
        System.out.println("decorationSeed=" + decoration);
        System.out.println("featureSeed=" + (decoration + index + 10000L * step));

        // WeightedListInt.sample: one nextInt(totalWeight) picks the entry.
        int total = 0;
        int[] weights = new int[candidates.length];
        for (int i = 0; i < weights.length; i++) {
            weights[i] = 1;
        }
        if (weights.length == 2) {
            weights[0] = 9;
            weights[1] = 1;
        }
        for (int w : weights) {
            total += w;
        }
        int pick = random.nextInt(total);
        int count = candidates[candidates.length - 1];
        int acc = 0;
        for (int i = 0; i < weights.length; i++) {
            acc += weights[i];
            if (pick < acc) {
                count = candidates[i];
                break;
            }
        }
        System.out.println("count=" + count + " (pick " + pick + " of " + total + ")");
        for (int i = 0; i < count; i++) {
            int x = random.nextInt(16) + cx * 16;
            int z = random.nextInt(16) + cz * 16;
            System.out.println("  candidate " + x + " " + z);
        }
    }
}
