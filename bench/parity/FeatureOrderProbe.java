import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.RegistryDataLoader;
import net.minecraft.server.RegistryLayer;
import net.minecraft.server.packs.PackLocationInfo;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.PackType;
import net.minecraft.server.packs.PathPackResources;
import net.minecraft.server.packs.repository.PackSource;
import net.minecraft.server.packs.resources.CloseableResourceManager;
import net.minecraft.server.packs.resources.MultiPackResourceManager;
import net.minecraft.tags.TagLoader;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.FeatureSorter;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;

/**
 * Prints the overworld's features-per-step list exactly as ChunkGenerator builds it.
 *
 * This is the order that decides each placed feature's index within its step, and
 * therefore its setFeatureSeed. The C port reproduces this list to place features
 * at vanilla's positions; this probe is the ground truth it is checked against.
 *
 *   probe26.sh FeatureOrderProbe <dataPackDir>
 */
public class FeatureOrderProbe {
    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            throw new IllegalArgumentException("usage: FeatureOrderProbe <dataPackDir>");
        }
        PrintStream realOut = System.out;
        PrintStream realErr = System.err;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        System.setOut(realOut);
        System.setErr(realErr);

        PackLocationInfo location = new PackLocationInfo(
                "feature-order-probe", Component.literal("feature-order-probe"), PackSource.BUILT_IN, Optional.empty());
        PackResources pack = new PathPackResources(location, java.nio.file.Path.of(args[0]));
        CloseableResourceManager resources = new MultiPackResourceManager(PackType.SERVER_DATA, List.of(pack));
        LayeredRegistryAccess<RegistryLayer> layers = RegistryLayer.createRegistryAccess();
        RegistryAccess.Frozen staticLayer = layers.getLayer(RegistryLayer.STATIC);
        List<Registry.PendingTags<?>> staticTags = TagLoader.loadTagsForExistingRegistries(resources, staticLayer);
        List<HolderLookup.RegistryLookup<?>> context = TagLoader.buildUpdatedLookups(staticLayer, staticTags);
        RegistryAccess.Frozen worldgen = RegistryDataLoader.load(
                        resources, context, RegistryDataLoader.WORLDGEN_REGISTRIES, java.util.concurrent.ForkJoinPool.commonPool())
                .join();
        resources.close();

        Registry<MultiNoiseBiomeSourceParameterList> presets = worldgen
                .lookup(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
                .orElseThrow(() -> new IllegalStateException("parameter list registry missing"));
        Holder<MultiNoiseBiomeSourceParameterList> preset =
                presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        MultiNoiseBiomeSource source = MultiNoiseBiomeSource.createFromPreset(preset);
        Registry<Biome> biomeRegistry = worldgen.lookupOrThrow(Registries.BIOME);

        List<Holder<Biome>> biomes = new ArrayList<>(source.possibleBiomes());
        System.out.println("# possible biomes: " + biomes.size());
        for (Holder<Biome> b : biomes) {
            Identifier id = biomeRegistry.getKey(b.value());
            System.out.println("biome " + id);
        }

        List<FeatureSorter.StepFeatureData> steps = FeatureSorter.buildFeaturesPerStep(
                biomes, holder -> holder.value().getGenerationSettings().features(), true);
        System.out.println("# steps: " + steps.size());
        for (int step = 0; step < steps.size(); step++) {
            List<PlacedFeature> features = steps.get(step).features();
            System.out.println("step " + step + " count " + features.size());
            for (int index = 0; index < features.size(); index++) {
                Identifier id = worldgen.lookupOrThrow(Registries.PLACED_FEATURE).getKey(features.get(index));
                System.out.println("  " + index + " " + id);
            }
        }
    }
}
