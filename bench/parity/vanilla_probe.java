import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ForkJoinPool;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
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
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.levelgen.DensityFunction;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.NoiseRouter;
import net.minecraft.world.level.levelgen.Noises;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.synth.BlendedNoise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;

/**
 * In-process loader for the vanilla 26.2 worldgen registries, exposing the primitives the C port is
 * checked against: density functions (registry entries and noise-router entries), NormalNoise
 * values and multi-noise biome selection.
 *
 * Usage:
 *   VanillaProbe --data <packDir> --seed <n> --points <file> [--hex] <mode>
 *   modes: --df <id> | --router <entry|all> | --noise <id> | --biome
 *
 * Output: "x y z <value>" per input point ("x y z <biome id>" for --biome), value printed with 17
 * significant digits; --hex appends the exact hex float.
 */
final class VanillaProbe {
   public static void main(final String[] args) throws Exception {
      try {
         run(args);
      } catch (IllegalArgumentException e) {
         System.err.println("vanilla_probe: " + e.getMessage());
         System.exit(2);
      }
   }

   private static void run(final String[] args) throws Exception {
      Map<String, String> opts = new HashMap<>();
      List<String> flags = new ArrayList<>();
      List<String> positional = new ArrayList<>();
      for (int i = 0; i < args.length; i++) {
         String a = args[i];
         if (a.startsWith("--")) {
            String key = a.substring(2);
            if (key.equals("hex")) {
               flags.add(key);
            } else if (i + 1 < args.length && !args[i + 1].startsWith("--")) {
               opts.put(key, args[++i]);
            } else {
               flags.add(key);
            }
         } else {
            positional.add(a);
         }
      }

      String dataDir = opts.getOrDefault(
         "data", System.getenv().getOrDefault("VANILLA_PROBE_DATA", "/home/nataxcan/dev/vanilla-probe/pack")
      );
      if (flags.contains("help") || args.length == 0) {
         System.out.println(
            "usage: VanillaProbe --data <packDir> --seed <n> --points <file|-> [--hex]"
               + " (--df <id> | --router <entry|all> | --noise <id> | --biome)"
         );
         return;
      }

      long seed = Long.parseLong(require(opts, "seed"));
      String pointsSpec = require(opts, "points");
      boolean hex = flags.contains("hex");

      Path packRoot = Path.of(dataDir);
      if (!Files.isDirectory(packRoot.resolve("data"))) {
         throw new IllegalArgumentException("no data/ directory under " + packRoot.toAbsolutePath());
      }

      RegistryAccess.Frozen worldgen = loadWorldgenRegistries(packRoot);
      RandomState randomState = RandomState.create(worldgen, NoiseGeneratorSettings.OVERWORLD, seed);

      List<int[]> points = readPoints(pointsSpec);
      StringBuilder out = new StringBuilder();

      if (opts.containsKey("df")) {
         Identifier id = parseId(require(opts, "df"));
         Registry<DensityFunction> registry = worldgen.lookup(Registries.DENSITY_FUNCTION)
            .orElseThrow(() -> new IllegalStateException("density function registry missing"));
         DensityFunction raw = registry.getOptional(id)
            .orElseThrow(() -> new IllegalArgumentException("unknown density function " + id));

         DensityFunction wired = wireLikeRandomState(raw, worldgen, randomState, seed);
         for (int[] p : points) {
            double v = wired.compute(new DensityFunction.SinglePointContext(p[0], p[1], p[2]));
            appendValue(out, p, v, hex);
         }
      } else if (opts.containsKey("router")) {
         String entry = require(opts, "router");
         if (entry.equals("all")) {
            for (Map.Entry<String, DensityFunction> e : routerEntries(randomState.router()).entrySet()) {
               for (int[] p : points) {
                  double v = e.getValue().compute(new DensityFunction.SinglePointContext(p[0], p[1], p[2]));
                  out.append(e.getKey()).append(' ');
                  appendValue(out, p, v, hex);
               }
            }
         } else {
            DensityFunction fn = routerEntry(randomState.router(), entry);
            for (int[] p : points) {
               double v = fn.compute(new DensityFunction.SinglePointContext(p[0], p[1], p[2]));
               appendValue(out, p, v, hex);
            }
         }
      } else if (opts.containsKey("noise")) {
         Identifier id = parseId(require(opts, "noise"));
         Registry<NormalNoise.NoiseParameters> noises = worldgen.lookup(Registries.NOISE)
            .orElseThrow(() -> new IllegalStateException("noise registry missing"));
         if (!noises.containsKey(id)) {
            throw new IllegalArgumentException("unknown noise " + id);
         }

         NormalNoise noise = randomState.getOrCreateNoise(ResourceKey.create(Registries.NOISE, id));
         for (int[] p : points) {
            double v = noise.getValue(p[0], p[1], p[2]);
            appendValue(out, p, v, hex);
         }
      } else if (flags.contains("biome")) {
         Registry<MultiNoiseBiomeSourceParameterList> presets = worldgen
            .lookup(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
            .orElseThrow(() -> new IllegalStateException("parameter list registry missing"));
         Holder<MultiNoiseBiomeSourceParameterList> preset = presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
         MultiNoiseBiomeSource source = MultiNoiseBiomeSource.createFromPreset(preset);
         Registry<Biome> biomes = worldgen.lookupOrThrow(Registries.BIOME);
         for (int[] p : points) {
            Holder<Biome> biome = source.getNoiseBiome(p[0], p[1], p[2], randomState.sampler());
            Identifier id = biome.unwrapKey().map(ResourceKey::identifier).orElse(null);
            if (id == null) {
               id = biomes.getKey(biome.value());
            }

            out.append(p[0]).append(' ').append(p[1]).append(' ').append(p[2]).append(' ').append(id).append('\n');
         }
      } else {
         throw new IllegalArgumentException(
            "one of --df, --router, --noise, --biome is required" + (positional.isEmpty() ? "" : " (got " + positional + ")")
         );
      }

      System.out.print(out);
      System.out.flush();
   }

   /** Mirrors WorldLoader/RegistryLayer: static bootstrap layer + its tags as context, then worldgen. */
   static RegistryAccess.Frozen loadWorldgenRegistries(final Path packRoot) throws IOException {
      // Bootstrap.wrapStreams() replaces System.out/err with logging streams, which would swallow both
      // the reference values and our diagnostics; keep the real streams and put them back after booting.
      java.io.PrintStream realOut = System.out;
      java.io.PrintStream realErr = System.err;
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();
      System.setOut(realOut);
      System.setErr(realErr);
      PackLocationInfo location = new PackLocationInfo(
         "vanilla-probe", Component.literal("vanilla-probe"), PackSource.BUILT_IN, Optional.empty()
      );
      PackResources pack = new PathPackResources(location, packRoot);
      CloseableResourceManager resources = new MultiPackResourceManager(PackType.SERVER_DATA, List.of(pack));
      LayeredRegistryAccess<RegistryLayer> layers = RegistryLayer.createRegistryAccess();
      RegistryAccess.Frozen staticLayer = layers.getLayer(RegistryLayer.STATIC);
      List<Registry.PendingTags<?>> staticTags = TagLoader.loadTagsForExistingRegistries(resources, staticLayer);
      List<HolderLookup.RegistryLookup<?>> context = TagLoader.buildUpdatedLookups(staticLayer, staticTags);
      RegistryAccess.Frozen worldgen = RegistryDataLoader.load(
            resources, context, RegistryDataLoader.WORLDGEN_REGISTRIES, ForkJoinPool.commonPool()
         )
         .join();
      resources.close();
      return worldgen;
   }

   /**
    * Re-applies RandomState's private NoiseWiringHelper to an arbitrary registry density function:
    * unwrap nothing (holders keep delegating), give BlendedNoise its terrain random, seed the End
    * island function and bind noise holders to the seeded NormalNoise instances, exactly as
    * RandomState does while building the noise router.
    */
   static DensityFunction wireLikeRandomState(
      final DensityFunction function, final RegistryAccess.Frozen access, final RandomState state, final long seed
   ) {
      NoiseGeneratorSettings settings = access.lookupOrThrow(Registries.NOISE_SETTINGS)
         .getOrThrow(NoiseGeneratorSettings.OVERWORLD)
         .value();
      return function.mapAll(new Wiring(settings, state, seed));
   }

   private static final class Wiring implements DensityFunction.Visitor {
      private final RandomState state;
      private final RandomSource terrainRandom;
      private final long seed;
      private final Map<DensityFunction, DensityFunction> wrapped = new HashMap<>();

      Wiring(final NoiseGeneratorSettings settings, final RandomState state, final long seed) {
         this.state = state;
         this.seed = seed;
         this.terrainRandom = settings.useLegacyRandomSource()
            ? new LegacyRandomSource(seed)
            : settings.getRandomSource().newInstance(seed).forkPositional().fromHashOf(Identifier.withDefaultNamespace("terrain"));
      }

      private DensityFunction wrapNew(final DensityFunction function) {
         if (function instanceof BlendedNoise noise) {
            return noise.withNewRandom(this.terrainRandom);
         } else {
            return EndIsland.isEndIsland(function) ? EndIsland.create(this.seed) : function;
         }
      }

      @Override
      public DensityFunction apply(final DensityFunction function) {
         return this.wrapped.computeIfAbsent(function, this::wrapNew);
      }

      // createLegacyNetherBiome is deprecated but is what vanilla's own NoiseWiringHelper uses for the
      // two nether noises, so it is kept for bit parity; the compiler note is suppressed here.
      @SuppressWarnings("deprecation")
      @Override
      public DensityFunction.NoiseHolder visitNoise(final DensityFunction.NoiseHolder noise) {
         Holder<NormalNoise.NoiseParameters> data = noise.noiseData();
         if (data.is(Noises.TEMPERATURE_NETHER)) {
            return new DensityFunction.NoiseHolder(data, NormalNoise.createLegacyNetherBiome(new LegacyRandomSource(this.seed), data.value()));
         } else if (data.is(Noises.VEGETATION_NETHER)) {
            return new DensityFunction.NoiseHolder(
               data, NormalNoise.createLegacyNetherBiome(new LegacyRandomSource(this.seed + 1L), data.value())
            );
         } else {
            return new DensityFunction.NoiseHolder(data, this.state.getOrCreateNoise(data.unwrapKey().orElseThrow()));
         }
      }
   }

   static Map<String, DensityFunction> routerEntries(final NoiseRouter router) {
      Map<String, DensityFunction> out = new LinkedHashMap<>();
      out.put("barrier", router.barrierNoise());
      out.put("fluid_level_floodedness", router.fluidLevelFloodednessNoise());
      out.put("fluid_level_spread", router.fluidLevelSpreadNoise());
      out.put("lava", router.lavaNoise());
      out.put("temperature", router.temperature());
      out.put("vegetation", router.vegetation());
      out.put("continents", router.continents());
      out.put("erosion", router.erosion());
      out.put("depth", router.depth());
      out.put("ridges", router.ridges());
      out.put("preliminary_surface_level", router.preliminarySurfaceLevel());
      out.put("final_density", router.finalDensity());
      out.put("vein_toggle", router.veinToggle());
      out.put("vein_ridged", router.veinRidged());
      out.put("vein_gap", router.veinGap());
      return out;
   }

   static DensityFunction routerEntry(final NoiseRouter router, final String entry) {
      DensityFunction fn = routerEntries(router).get(entry);
      if (fn == null) {
         throw new IllegalArgumentException("unknown noise router entry " + entry + "; known: " + routerEntries(router).keySet());
      }

      return fn;
   }

   /**
    * {@code DensityFunctions.EndIslandDensityFunction} is not public, so reach it reflectively; it
    * carries the world seed and is re-created by RandomState's wiring pass.
    */
   private static final class EndIsland {
      private static final Class<?> TYPE;
      private static final java.lang.reflect.Constructor<?> CTOR;

      static {
         Class<?> type = null;
         java.lang.reflect.Constructor<?> ctor = null;
         try {
            type = Class.forName("net.minecraft.world.level.levelgen.DensityFunctions$EndIslandDensityFunction");
            ctor = type.getDeclaredConstructor(long.class);
            ctor.setAccessible(true);
         } catch (ReflectiveOperationException e) {
            type = null;
         }

         TYPE = type;
         CTOR = ctor;
      }

      static boolean isEndIsland(final Object o) {
         return TYPE != null && TYPE.isInstance(o);
      }

      static DensityFunction create(final long seed) {
         try {
            return (DensityFunction)CTOR.newInstance(seed);
         } catch (ReflectiveOperationException e) {
            throw new IllegalStateException("cannot construct EndIslandDensityFunction", e);
         }
      }
   }

   private static void appendValue(final StringBuilder out, final int[] p, final double v, final boolean hex) {
      out.append(p[0]).append(' ').append(p[1]).append(' ').append(p[2]).append(' ');
      out.append(String.format(Locale.ROOT, "%.17g", v));
      if (hex) {
         out.append(' ').append(Double.toHexString(v));
      }

      out.append('\n');
   }

   private static Identifier parseId(final String raw) {
      return raw.indexOf(':') >= 0 ? Identifier.parse(raw) : Identifier.withDefaultNamespace(raw);
   }

   private static String require(final Map<String, String> opts, final String key) {
      String value = opts.get(key);
      if (value == null) {
         throw new IllegalArgumentException("missing --" + key);
      }

      return value;
   }

   private static List<int[]> readPoints(final String spec) throws IOException {
      List<int[]> points = new ArrayList<>();
      BufferedReader reader = spec.equals("-")
         ? new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8))
         : Files.newBufferedReader(Path.of(spec), StandardCharsets.UTF_8);
      try (BufferedReader r = reader) {
         String line;
         while ((line = r.readLine()) != null) {
            String trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.startsWith("#")) {
               continue;
            }

            String[] parts = trimmed.split("[\\s,]+");
            if (parts.length < 3) {
               throw new IllegalArgumentException("bad point line: " + line);
            }

            points.add(new int[]{toInt(parts[0]), toInt(parts[1]), toInt(parts[2])});
         }
      }

      return points;
   }

   private static int toInt(final String token) {
      try {
         return Integer.parseInt(token);
      } catch (NumberFormatException e) {
         return (int)Double.parseDouble(token);
      }
   }
}
