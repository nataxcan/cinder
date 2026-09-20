import java.io.IOException;
import java.io.PrintStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ForkJoinPool;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.QuartPos;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.RegistryDataLoader;
import net.minecraft.resources.ResourceKey;
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
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.ProtoChunk;
import net.minecraft.world.level.chunk.UpgradeData;
import net.minecraft.world.level.levelgen.Aquifer;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.carver.CarvingContext;
import net.minecraft.world.level.levelgen.carver.ConfiguredWorldCarver;

/**
 * Vanilla oracle for the carver stage.
 *
 * Replays NoiseBasedChunkGenerator.applyCarvers' decision phase for one target chunk against the
 * real 26.2 server jar: real biome source, real RandomState, real ConfiguredWorldCarver instances
 * loaded from the real registry data, and the real CaveWorldCarver/CanyonWorldCarver.carve() code.
 * Every random draw the carvers make is logged, so the C port's trace (built with
 * -DCINDER_CARVER_TRACE, see carver_trace.sh) can be diffed event by event.
 *
 * The chunk handed to the carvers is an empty ProtoChunk (every block is air), so carveBlock()
 * bails out before touching the aquifer or the chunk and no state is left behind: the only thing
 * the carvers do is consume RNG. Every RNG draw in WorldCarver/CaveWorldCarver/CanyonWorldCarver
 * happens before the block loop anyway, so the log below is the carvers' complete RNG consumption.
 *
 * Usage:
 *   carver_trace --data <packDir> --seed <n> [--cx 20] [--cz 11] [--radius 8]
 *   carver_trace --data <packDir> --seed <n> --configs
 *
 * Trace format (identical to the C trace):
 *   # seed <seed> target <cx> <cz>
 *   SRC <srcCx> <srcCz> <index> <biome> <carver>
 *   S <16 hex digits>            state after setLargeFeatureSeed
 *   F <8 hex digits>             nextFloat() result bits   (the isStartChunk draw, then tunnel draws)
 *   I <bound> <value>            nextInt(bound)
 *   L <16 hex digits>            nextLong()
 *   X <0|1>                      isStartChunk result
 *   # end
 */
public class carver_trace {
   public static void main(final String[] args) throws Exception {
      try {
         run(args);
      } catch (IllegalArgumentException e) {
         System.err.println("carver_trace: " + e.getMessage());
         System.exit(2);
      }
   }

   private static void run(final String[] args) throws Exception {
      Map<String, String> opts = new HashMap<>();
      List<String> flags = new ArrayList<>();
      for (int i = 0; i < args.length; i++) {
         String a = args[i];
         if (a.startsWith("--")) {
            String key = a.substring(2);
            if (i + 1 < args.length && !args[i + 1].startsWith("--")) {
               opts.put(key, args[++i]);
            } else {
               flags.add(key);
            }
         }
      }

      long seed = Long.parseLong(require(opts, "seed"));
      String dataDir = opts.getOrDefault(
         "data", System.getenv().getOrDefault("VANILLA_PROBE_DATA", "/home/nataxcan/dev/vanilla-probe/pack")
      );
      Path packRoot = Path.of(dataDir);
      if (!Files.isDirectory(packRoot.resolve("data"))) {
         throw new IllegalArgumentException("no data/ directory under " + packRoot.toAbsolutePath());
      }

      RegistryAccess.Frozen worldgen = loadWorldgenRegistries(packRoot);
      RandomState randomState = RandomState.create(worldgen, NoiseGeneratorSettings.OVERWORLD, seed);

      if (flags.contains("configs")) {
         dumpConfigs(worldgen);
         return;
      }

      int cx = Integer.parseInt(opts.getOrDefault("cx", "20"));
      int cz = Integer.parseInt(opts.getOrDefault("cz", "11"));
      int radius = Integer.parseInt(opts.getOrDefault("radius", "8"));

      Registry<MultiNoiseBiomeSourceParameterList> presets = worldgen
         .lookup(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
         .orElseThrow(() -> new IllegalStateException("parameter list registry missing"));
      Holder<MultiNoiseBiomeSourceParameterList> preset = presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
      MultiNoiseBiomeSource biomeSource = MultiNoiseBiomeSource.createFromPreset(preset);
      Holder<NoiseGeneratorSettings> settings = worldgen
         .lookupOrThrow(Registries.NOISE_SETTINGS)
         .getOrThrow(NoiseGeneratorSettings.OVERWORLD);

      NoiseGeneratorSettings settingsValue = settings.value();
      int minY = settingsValue.noiseSettings().minY();
      int height = settingsValue.noiseSettings().height();
      LevelHeightAccessor heightAccessor = LevelHeightAccessor.create(minY, height);
      NoiseBasedChunkGenerator generator = new NoiseBasedChunkGenerator(biomeSource, settings);

      // noiseChunk is only reachable through topMaterial(); carveBlock() returns before that
      // because every block of the empty proto chunk is air (not in the replaceable tag).
      CarvingContext context = new CarvingContext(
         generator, worldgen, heightAccessor, null, randomState, settingsValue.surfaceRule()
      );
      Aquifer aquifer = Aquifer.createDisabled((x, y, z) -> new Aquifer.FluidStatus(minY, net.minecraft.world.level.block.Blocks.WATER.defaultBlockState()));
      PalettedContainerFactory containers = PalettedContainerFactory.create(worldgen);
      ProtoChunk chunk = new ProtoChunk(new ChunkPos(cx, cz), UpgradeData.EMPTY, heightAccessor, containers, null);
      Holder<Biome> fallbackBiome = worldgen.lookupOrThrow(Registries.BIOME).getOrThrow(plainsKey());

      PrintStream out = System.out;
      selftest(out);
      out.println("# seed " + seed + " target " + cx + " " + cz);

      for (int dx = -radius; dx <= radius; dx++) {
         for (int dz = -radius; dz <= radius; dz++) {
            ChunkPos sourcePos = new ChunkPos(cx + dx, cz + dz);
            Holder<Biome> biome = biomeSource.getNoiseBiome(
               QuartPos.fromBlock(sourcePos.getMinBlockX()), 0, QuartPos.fromBlock(sourcePos.getMinBlockZ()), randomState.sampler()
            );
            String biomeName = biome.unwrapKey().map(k -> k.identifier().toString()).orElse("?");
            var carvers = biome.value().getGenerationSettings().getCarvers();
            int index = 0;
            for (Holder<ConfiguredWorldCarver<?>> carverHolder : carvers) {
               ConfiguredWorldCarver<?> carver = carverHolder.value();
               String carverName = carverHolder.unwrapKey().map(k -> k.identifier().toString()).orElse("?");
               out.println("SRC " + sourcePos.x() + " " + sourcePos.z() + " " + index + " " + biomeName + " " + carverName);
               LoggingRandom random = new LoggingRandom(out);
               random.largeFeatureSeed(seed + index, sourcePos.x(), sourcePos.z());
               boolean start = carver.isStartChunk(random);
               out.println("X " + (start ? 1 : 0));
               if (start) {
                  carver.carve(context, chunk, pos -> fallbackBiome, random, aquifer, sourcePos, chunk.getOrCreateCarvingMask());
               }
               index++;
            }
         }
      }
      out.println("# end");
      out.flush();
   }

   /** The overworld preset's plains entry; only used as a biomeGetter fallback (never invoked). */
   private static ResourceKey<Biome> plainsKey() {
      return ResourceKey.create(Registries.BIOME, Identifier.withDefaultNamespace("plains"));
   }

   /** Prints the real registry JSON of every configured carver, for the config audit. */
   private static void dumpConfigs(final RegistryAccess.Frozen worldgen) {
      Registry<ConfiguredWorldCarver<?>> registry = worldgen.lookupOrThrow(Registries.CONFIGURED_CARVER);
      for (Map.Entry<ResourceKey<ConfiguredWorldCarver<?>>, ConfiguredWorldCarver<?>> e : registry.entrySet()) {
         String id = e.getKey().identifier().toString();
         if (!id.startsWith("minecraft:")) {
            continue;
         }
         Optional<com.google.gson.JsonElement> json = ConfiguredWorldCarver.DIRECT_CODEC
            .encodeStart(com.mojang.serialization.JsonOps.INSTANCE, e.getValue())
            .result();
         System.out.println(id + " " + json.map(Object::toString).orElse("<unencodable>"));
      }
   }

   /**
    * Checks the hand-rolled LegacyRandomSource against the real one, so a divergence in the logger
    * itself cannot be mistaken for a carver divergence: the same call sequence must produce the same
    * values, and largeFeatureSeed() must leave the same state as WorldgenRandom.setLargeFeatureSeed.
    */
   private static void selftest(final PrintStream log) {
      boolean ok = true;
      java.util.Random bounds = new java.util.Random(7L);
      LoggingRandom a = new LoggingRandom(null);
      LegacyRandomSource b = new LegacyRandomSource(0L);
      for (int i = 0; i < 4096 && ok; i++) {
         int bound = 1 + bounds.nextInt(300);
         if (a.silentNextInt(bound) != b.nextInt(bound)) ok = false;
         if (a.silentNextLong() != b.nextLong()) ok = false;
         if (Float.floatToRawIntBits(a.silentNextFloat()) != Float.floatToRawIntBits(b.nextFloat())) ok = false;
         if (Double.doubleToRawLongBits(a.silentNextDouble()) != Double.doubleToRawLongBits(b.nextDouble())) ok = false;
      }
      LegacyRandomSource seeds = new LegacyRandomSource(99L);
      for (int i = 0; i < 256 && ok; i++) {
         long seed = seeds.nextLong();
         int x = seeds.nextInt(2000) - 1000;
         int z = seeds.nextInt(2000) - 1000;
         WorldgenRandom real = new WorldgenRandom(new LegacyRandomSource(0L));
         real.setLargeFeatureSeed(seed, x, z);
         LoggingRandom mine = new LoggingRandom(null);
         mine.largeFeatureSeed(seed, x, z);
         for (int k = 0; k < 8 && ok; k++) {
            if (mine.silentNextLong() != real.nextLong()) ok = false;
         }
         for (int k = 0; k < 16 && ok; k++) {
            if (Float.floatToRawIntBits(mine.silentNextFloat()) != Float.floatToRawIntBits(real.nextFloat())) ok = false;
         }
      }
      log.println("# selftest " + (ok ? "ok" : "FAILED"));
   }

   /**
    * A LegacyRandomSource that logs the calls the carvers make. Draws made by one public method on
    * another (nextLong -> next(32)) are not logged twice: only the public RandomSource surface is
    * intercepted, which is exactly the surface the carvers use, and it is the surface the C port
    * mirrors with legacy_next_float / legacy_next_bound / legacy_next_long / legacy_set_seed.
    */
   static final class LoggingRandom extends LegacyRandomSource {
      private static final long MULT = 25214903917L;
      private static final long MASK = 281474976710655L;
      private final PrintStream out;

      LoggingRandom(final PrintStream out) {
         super(0L);
         this.out = out;
      }

      private static String hex(final long v) {
         return String.format("%016x", v);
      }

      /** setLargeFeatureSeed without logging the intermediate setSeed/nextLong pair. */
      void largeFeatureSeed(final long seed, final int x, final int z) {
         super.setSeed(seed);
         long xScale = super.nextLong();
         long zScale = super.nextLong();
         long result = (long)x * xScale ^ (long)z * zScale ^ seed;
         super.setSeed(result);
         log("S " + hex((result ^ MULT) & MASK));
      }

      void setSeedSilently(final long seed) {
         super.setSeed(seed);
      }

      long silentNextLong() {
         return super.nextLong();
      }

      int silentNextInt(final int bound) {
         return super.nextInt(bound);
      }

      float silentNextFloat() {
         return super.nextFloat();
      }

      double silentNextDouble() {
         return super.nextDouble();
      }

      private void log(final String line) {
         if (this.out != null) {
            this.out.println(line);
         }
      }

      @Override
      public void setSeed(final long seed) {
         super.setSeed(seed);
         log("S " + hex((seed ^ MULT) & MASK));
      }

      @Override
      public int nextInt() {
         int value = super.nextInt();
         log("N 32 " + value);
         return value;
      }

      @Override
      public int nextInt(final int bound) {
         int value = super.nextInt(bound);
         log("I " + bound + " " + value);
         return value;
      }

      @Override
      public long nextLong() {
         long value = super.nextLong();
         log("L " + hex(value));
         return value;
      }

      @Override
      public float nextFloat() {
         float value = super.nextFloat();
         log("F " + Integer.toHexString(Float.floatToRawIntBits(value)));
         return value;
      }

      @Override
      public double nextDouble() {
         double value = super.nextDouble();
         log("D " + hex(Double.doubleToRawLongBits(value)));
         return value;
      }

      @Override
      public boolean nextBoolean() {
         return super.nextBoolean();
      }

      @Override
      public RandomSource fork() {
         return super.fork();
      }
   }

   private static String require(final Map<String, String> opts, final String key) {
      String value = opts.get(key);
      if (value == null) {
         throw new IllegalArgumentException("missing --" + key);
      }
      return value;
   }

   /** Mirrors WorldLoader/RegistryLayer as bench/parity/vanilla_probe.java does. */
   static RegistryAccess.Frozen loadWorldgenRegistries(final Path packRoot) throws IOException {
      java.io.PrintStream realOut = System.out;
      java.io.PrintStream realErr = System.err;
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();
      System.setOut(realOut);
      System.setErr(realErr);
      PackLocationInfo location = new PackLocationInfo(
         "carver-trace", Component.literal("carver-trace"), PackSource.BUILT_IN, Optional.empty()
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
}
