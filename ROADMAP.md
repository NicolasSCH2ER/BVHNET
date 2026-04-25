# Path Tracer — Plan d'implémentation

## Étape 1 — Squelette de base ✅
- Vec3, Ray
- Camera sténopé (pinhole)
- Hittable / HitRecord
- Sphere
- Matériaux : Lambertian, Mirror, Emissive
- Scene (liste linéaire)
- Output PNG (stb_image_write)

## Étape 2 — Diélectrique + Parallélisme ✅
- Matériau `Dielectric` : loi de Snell, réflexion totale interne, Fresnel (Schlick)
- OpenMP sur la boucle de pixels (`#pragma omp parallel for`)
- Compteur de progression thread-safe (`std::atomic`)

---

## Étape 3 — BVH (Bounding Volume Hierarchy)
**Objectif :** passer l'intersection de O(n) à O(log n).  
Sans ça, une scène avec des milliers de triangles sera inutilisable.

- Struct `AABB` (Axis-Aligned Bounding Box) avec test d'intersection rayon/boîte
- Struct `BVHNode : Hittable` — arbre binaire construit récursivement
- Trier les objets par centroïde sur l'axe le plus large (SAH simplifié)
- Remplacer `Scene` par `BVHNode` comme racine de la hiérarchie

## Étape 4 — Triangles + import OBJ
**Objectif :** pouvoir charger de vrais modèles 3D.

- Struct `Triangle : Hittable` — intersection de Möller–Trumbore
- Struct `Mesh` — liste de triangles avec un matériau partagé
- Parser OBJ via `tinyobjloader` (single-header, même principe que stb)
- UVs et normales interpolées (smooth shading)
- Tester avec un mesh Blender exporté en OBJ

## Étape 5 — Qualité d'image
**Objectif :** réduire le bruit sans augmenter les samples.

- **Next Event Estimation (NEE)** — à chaque rebond, tirer directement vers les sources de lumière au lieu d'espérer les toucher par hasard. Gain de bruit spectaculaire.
- **Russian Roulette** — terminer les chemins de façon probabiliste plutôt qu'à `MAX_DEPTH` fixe. Non-biaisé et plus efficace.
- **Tone mapping ACES** — meilleure conversion HDR→LDR que le simple clamp sRGB actuel.

## Étape 6 — Caméra avancée
**Objectif :** effets photographiques.

- **Depth of field** — caméra thin-lens avec ouverture (`aperture`) et distance de mise au point (`focus_dist`). Effet bokeh.
- **Exposition / shutter** — paramètre de luminosité global.

## Étape 7 — Textures
**Objectif :** matériaux visuellement riches.

- Interface `Texture` avec `value(u, v, point)`
- `SolidColor` — couleur unie (remplace les albédos actuels)
- `CheckerTexture` — damier procédural
- `ImageTexture` — PNG/JPEG mappé via UVs (via `stb_image`)
- Brancher les textures sur `Lambertian` et `Dielectric`

## Étape 8 — Matériaux PBR
**Objectif :** matériaux physiquement corrects compatibles glTF/Blender.

- Modèle **GGX/Microfacet** : roughness + metallic
- BRDF de Cook-Torrance
- Remplacer `Mirror` par un matériau PBR unifié
- Compatible avec les exports Blender (roughness/metallic maps)

## Étape 9 — Import glTF
**Objectif :** charger une scène Blender complète en une commande.

- Parser glTF via `tinygltf` (single-header)
- Charger : meshes, matériaux PBR, textures, caméra, lumières
- Pipeline : `scene.blend` → `File > Export > glTF 2.0` → `renderer.exe scene.glb`

## Étape 10 — Port GPU (CUDA / OptiX)
**Objectif :** exploiter la RTX 5070 Ti (RT cores, 16 Go VRAM).

- **Option A — CUDA brut** : réécrire le path tracer en kernels CUDA. Pédagogique, contrôle total.
- **Option B — OptiX** : framework NVIDIA haut niveau, gère le BVH et le lancer de rayons via les RT cores. Écrire uniquement les shaders de matériaux.

Recommandation : OptiX pour la performance, CUDA pour comprendre les internals.

Pipeline final visé :
```
scene.blend  →  blender --background --python export.py  →  scene.glb
scene.glb    →  renderer.exe                              →  output.png
```

## Étape 11 — Optimisation du BVH pour le GPU

**Objectif :** remplacer le BVH CPU (SAH binned, récursif) par une structure adaptée au GPU massivement parallèle.

### HLBVH (Hierarchical Linear BVH)
- Calcul des **codes de Morton** pour chaque primitive (encode position 3D → entier 1D sur une courbe de Hilbert)
- Tri radix des codes → ordre spatial cohérent en mémoire
- Construction de l'arbre par **common prefix** sur les codes triés (algorithme Karras 2012)
- Entièrement parallélisable sur GPU (chaque thread construit un nœud)
- Layout mémoire linéaire (tableau plat) → accès coalescés sur GPU, pas de pointeurs

### Réseau de neurones pour la traversal
- Entraîner un petit MLP à prédire **quel enfant visiter en premier** selon la direction du rayon et la position du nœud
- Remplace l'heuristique classique (distance à la boîte) par une décision apprise
- Intégrable comme shader CUDA dans OptiX
- Papiers de référence : *Neural BVH* (Meister et al. 2021), *Learning to Traverse* (2022)

### Représentation alternative : CWBVH
- Compressed Wide BVH — regroupe 8 enfants par nœud au lieu de 2
- Réduit la profondeur de l'arbre, améliore la cohérence des warps GPU
- Utilisé dans les moteurs de production NVIDIA (Turing+)

---

## Références utiles
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) — parser OBJ single-header
- [tinygltf](https://github.com/syoyo/tinygltf) — parser glTF single-header
- [stb_image](https://github.com/nothings/stb) — chargement d'images (textures)
- [Intel OIDN](https://www.openimagedenoise.org/) — dénoiser neural (optionnel, étape bonus)
- [OptiX SDK](https://developer.nvidia.com/optix) — ray tracing GPU NVIDIA
- [Physically Based Rendering (PBRT)](https://pbr-book.org/) — référence théorique complète, gratuite en ligne
