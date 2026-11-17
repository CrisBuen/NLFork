// CAS - Contrast Adaptive Sharpening (algoritmo de AMD FidelityFX CAS), version para chiaki-ng.
// Formato: shader de usuario estilo mpv (libplacebo). Se carga en Ajustes > Video > Custom Shaders > Add.
//
// Se aplica DESPUES del reescalado (etapa SCALED), a la resolucion de tu monitor, asi que funciona
// encima de FSRCNNX, FSR o cualquier otro reescalador.
// Afila mas donde la imagen esta blanda y menos donde ya hay contraste, para no exagerar
// el ruido de compresion del stream.
//
// SHARPNESS: 0.0 = minimo, 1.0 = maximo. Este archivo usa un valor suave.

//!HOOK SCALED
//!BIND HOOKED
//!DESC CAS suave (nitidez adaptativa)

#define SHARPNESS 0.4

vec4 hook()
{
    // Vecindario 3x3:  a b c / d e f / g h i
    vec3 a = HOOKED_texOff(vec2(-1.0, -1.0)).rgb;
    vec3 b = HOOKED_texOff(vec2( 0.0, -1.0)).rgb;
    vec3 c = HOOKED_texOff(vec2( 1.0, -1.0)).rgb;
    vec3 d = HOOKED_texOff(vec2(-1.0,  0.0)).rgb;
    vec4 e4 = HOOKED_texOff(vec2(0.0, 0.0));
    vec3 e = e4.rgb;
    vec3 f = HOOKED_texOff(vec2( 1.0,  0.0)).rgb;
    vec3 g = HOOKED_texOff(vec2(-1.0,  1.0)).rgb;
    vec3 h = HOOKED_texOff(vec2( 0.0,  1.0)).rgb;
    vec3 i = HOOKED_texOff(vec2( 1.0,  1.0)).rgb;

    // Minimo y maximo "suavizados": cruz + diagonales
    vec3 mn = min(min(min(d, e), min(f, b)), h);
    vec3 mn2 = min(mn, min(min(a, c), min(g, i)));
    mn += mn2;
    vec3 mx = max(max(max(d, e), max(f, b)), h);
    vec3 mx2 = max(mx, max(max(a, c), max(g, i)));
    mx += mx2;

    // Cuanto afilar: mucho en zonas planas/blandas, poco cerca de bordes ya fuertes.
    // Con valores por encima de 1 (HDR) el factor se hace 0 y no se afila, sin recortar brillos.
    vec3 amp = sqrt(clamp(min(mn, 2.0 - mx) / max(mx, vec3(1e-5)), 0.0, 1.0));

    float peak = -1.0 / mix(8.0, 5.0, clamp(SHARPNESS, 0.0, 1.0));
    vec3 w = amp * peak;

    vec3 outc = (b * w + d * w + f * w + h * w + e) / (1.0 + 4.0 * w);
    return vec4(max(outc, vec3(0.0)), e4.a);
}
