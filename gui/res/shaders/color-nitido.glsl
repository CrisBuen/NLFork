// Color nitido: reescalado del color (croma) guiado por el brillo (luma), version para chiaki-ng.
// Formato: shader de usuario estilo mpv (libplacebo). Se carga en Ajustes > Video > Custom Shaders > Add.
//
// El stream de la PS5 viene en 4:2:0: el color llega a la MITAD de resolucion que el brillo
// (960x540 de color en un stream 1080p). Normalmente el color se agranda con un filtro comun y
// los bordes de color (textos del HUD, objetos rojos o azules) quedan blandos o con halo.
// Este shader mira, alrededor de cada pixel, como se relaciona el color con el brillo y usa el
// brillo a resolucion completa para dibujar los bordes de color en su sitio.
// Donde color y brillo no estan relacionados, usa un filtro Catmull-Rom normal.
// El resultado nunca se sale del rango de color de los 4 vecinos mas cercanos (sin halos).
//
// STRENGTH: 0.0 = desactivado (solo Catmull-Rom), 1.0 = efecto completo.

//!HOOK CHROMA
//!BIND HOOKED
//!BIND LUMA
//!WIDTH LUMA.w
//!HEIGHT LUMA.h
//!WHEN CHROMA.w LUMA.w <
//!OFFSET ALIGN
//!DESC Color nitido (croma guiado por el brillo)

#define STRENGTH 1.0

vec4 catmull_rom(float t)
{
    float t2 = t * t, t3 = t2 * t;
    return 0.5 * vec4(-t3 + 2.0 * t2 - t,
                      3.0 * t3 - 5.0 * t2 + 2.0,
                      -3.0 * t3 + 4.0 * t2 + t,
                      t3 - t2);
}

vec4 hook()
{
    // Posicion de este pixel en texels de color, y el texel de color de arriba a la izquierda
    vec2 pos = HOOKED_pos * HOOKED_size - vec2(0.5);
    vec2 fp = floor(pos);
    vec2 fr = pos - fp;
    // Mismo punto fisico en la textura de brillo (las dos cubren el cuadro entero)
    vec2 to_luma = LUMA_pos - HOOKED_pos;
    // Dos muestras a +-1/4 de texel de color: promedian el brillo con el mismo filtro con el
    // que se reduce el color (1-2-1 si el color va alineado a la izquierda, 1-1 si va centrado)
    vec2 half_step = vec2(0.25 * HOOKED_pt.x, 0.0);
    float y = LUMA_tex(LUMA_pos).x;

    vec4 wx = catmull_rom(fr.x);
    vec4 wy = catmull_rom(fr.y);

    float sw = 0.0, sl = 0.0, sll = 0.0;
    vec2 sc = vec2(0.0), slc = vec2(0.0), scc = vec2(0.0);
    vec2 base = vec2(0.0);
    vec2 cmin = vec2(1e9), cmax = vec2(-1e9);

    // Vecindario de 4x4 texels de color
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            vec2 tp = (fp + vec2(float(i) - 0.5, float(j) - 0.5)) * HOOKED_pt;
            vec2 c = HOOKED_tex(tp).xy;
            // Brillo promedio en la zona que cubre ese texel de color
            float l = 0.5 * (LUMA_tex(tp + to_luma - half_step).x + LUMA_tex(tp + to_luma + half_step).x);

            base += c * (wx[i] * wy[j]);

            vec2 d = vec2(float(i) - 1.0, float(j) - 1.0) - fr;
            float w = exp(-0.5 * dot(d, d));
            sw += w;
            sl += w * l;
            sll += w * l * l;
            sc += w * c;
            slc += w * l * c;
            scc += w * c * c;

            if (i >= 1 && i <= 2 && j >= 1 && j <= 2) {
                cmin = min(cmin, c);
                cmax = max(cmax, c);
            }
        }
    }

    sl /= sw; sll /= sw; sc /= sw; slc /= sw; scc /= sw;
    float var_l = max(sll - sl * sl, 0.0);
    vec2 var_c = max(scc - sc * sc, vec2(0.0));
    vec2 cov = slc - sl * sc;

    // Prediccion: color = a * brillo + b, ajustada en el vecindario
    vec2 pred = sc + cov / (var_l + 1e-6) * (y - sl);

    // Confianza: que tan bien el brillo explica el color aqui (0..1). Si el brillo casi no
    // cambia, la relacion no es fiable y se usa el filtro normal.
    vec2 corr = clamp(cov * cov / (var_l * var_c + 1e-10), 0.0, 1.0);
    corr *= smoothstep(2e-5, 2e-4, var_l);

    vec2 outc = mix(base, pred, corr * STRENGTH);
    outc = clamp(outc, cmin, cmax);
    return vec4(outc, 0.0, 1.0);
}
