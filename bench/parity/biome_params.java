import com.mojang.datafixers.util.Pair;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.biome.OverworldBiomeBuilder;

/**
 * Dumps the vanilla overworld multi-noise parameter list, in call order, as JSON.
 * Compile/run against the deobfuscated vanilla server jar (see run.sh).
 */
public class biome_params {
   public static void main(final String[] args) throws Exception {
      OverworldBiomeBuilder builder = new OverworldBiomeBuilder();
      List<Pair<Climate.ParameterPoint, ResourceKey<Biome>>> out = new ArrayList<>();
      Method addBiomes = OverworldBiomeBuilder.class.getDeclaredMethod("addBiomes", Consumer.class);
      addBiomes.setAccessible(true);
      addBiomes.invoke(builder, (Consumer<Pair<Climate.ParameterPoint, ResourceKey<Biome>>>)out::add);

      StringBuilder sb = new StringBuilder();
      sb.append("{\"count\":").append(out.size()).append(",\"entries\":[\n");
      boolean first = true;
      for (Pair<Climate.ParameterPoint, ResourceKey<Biome>> e : out) {
         Climate.ParameterPoint p = e.getFirst();
         if (!first) {
            sb.append(",\n");
         }

         first = false;
         sb.append("  {\"biome\":\"").append(e.getSecond().identifier()).append('"');
         appendParam(sb, "temperature", p.temperature());
         appendParam(sb, "humidity", p.humidity());
         appendParam(sb, "continentalness", p.continentalness());
         appendParam(sb, "erosion", p.erosion());
         appendParam(sb, "depth", p.depth());
         appendParam(sb, "weirdness", p.weirdness());
         sb.append(",\"offset\":").append(p.offset()).append('}');
      }

      sb.append("\n]}\n");
      System.out.print(sb);
   }

   private static void appendParam(final StringBuilder sb, final String name, final Climate.Parameter p) {
      sb.append(",\"").append(name).append("\":[").append(p.min()).append(',').append(p.max()).append(']');
   }
}
