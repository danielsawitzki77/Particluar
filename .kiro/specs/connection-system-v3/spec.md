# Connection System V3 â€” Complete Rewrite Spec

## Problem Statement

The current connection system (v2) is fundamentally broken. It attempts to deform mesh geometry at connection points by "tapering", "hole-cutting", or "scaling faces toward a ring". All these approaches produce visual artifacts (inverted faces, gaps, skewed geometry) because they try to force incompatible topology changes on meshes at runtime.

## Core Insight

The connection system should NOT modify mesh topology. Instead:

1. **Two connected bodies remain independent, complete, closed meshes.**
2. **They are positioned so one face from each body is at exactly the same location** (coincident faces, touching perfectly).
3. **The face topology compatibility is guaranteed by construction** â€” only connections between faces of the same polygon type (quadâ†”quad, ngonâ†”ngon) are allowed.
4. **At any given subdivision level N, both shapes produce matching faces at the connection boundary** â€” this is what "topologically compatible" means.
5. **Size matching ("meeting in the middle")**: when connected shapes have different radii, the connection face on each shape is resized (scaled uniformly from its own center), and its immediate edge-sharing neighbors stretch to accommodate. This is a local vertex adjustment, NOT a topological change.

## Primitive Types and Their Face Grids

Each primitive, when subdivided, produces a grid of faces with known topology:

### Cylinder (segments=N, height_segments=H)
- **Lateral faces**: NÃ—H quads, organized in a grid (column=angular position, row=height position)
- **Top cap**: 1 N-gon (N vertices)
- **Bottom cap**: 1 N-gon (N vertices)
- Face indexing: caps first (index 0=top, 1=bottom), then lateral quads in row-major order (index 2 + row*N + col)

### Cone (segments=N)
- **Lateral faces**: N triangles (apex to base edge)
- **Base cap**: 1 N-gon
- Face indexing: base first (index 0), then lateral triangles (index 1..N)

### Torus (ring_segments=R, tube_segments=T)
- **All faces**: RÃ—T quads in a grid (ring position Ã— tube position)
- No caps â€” fully closed without flat faces
- Face indexing: ring_idx * T + tube_idx

### Sphere (slices=S, stacks=T)
- **Pole triangles**: S at north pole, S at south pole
- **Mid-band quads**: SÃ—(T-2) quads
- Face indexing: north pole tris (0..S-1), mid quads (S + row*S + col), south pole tris at end

### Capsule (segments=N, height_segments=H)
- Same as cylinder for the lateral portion (NÃ—H quads)
- Hemisphere caps instead of flat caps
- Lateral side faces are quads, cap faces are triangles/quads

## Connection Compatibility Matrix

Two attachment points are **compatible** if and only if they produce faces of the same polygon type at any subdivision level:

| Parent Face Type | Child Face Type | Compatible? | Example |
|---|---|---|---|
| Quad (lateral) | Quad (lateral) | âœ… | cylinder.side â†” torus.surface |
| Quad (lateral) | Quad (lateral) | âœ… | cylinder.side â†” sphere.surface |
| Quad (lateral) | Quad (lateral) | âœ… | cylinder.side â†” cylinder.side |
| N-gon (cap) | N-gon (cap) | âœ… | cylinder.top â†” cylinder.bottom |
| N-gon (cap) | N-gon (cap) | âœ… | cylinder.top â†” cone.base |
| Triangle (cone side) | Triangle (cone side) | âœ… | cone.side â†” cone.side |
| N-gon (cap) | Quad (lateral) | âŒ | cylinder.top â†” torus.surface |
| N-gon (cap) | Quad (lateral) | âŒ | cylinder.bottom â†” sphere.surface |
| Triangle (cone side) | Quad (lateral) | âŒ | cone.side â†” cylinder.side |
| Triangle (cone side) | N-gon (cap) | âŒ | cone.side â†” cylinder.top |

**Key rule**: A cylinder bottom/top (N-gon) can NEVER connect to a torus surface (quad) or sphere surface (quad). This is invalid by topology.

## How Connection Positioning Works

1. The **ConnectionSolver** computes a transform that positions the child so its connection face is coincident with the parent's connection face.
2. The parent's connection face and child's connection face end up at the **same world-space position**.
3. Both faces have the same vertex count (guaranteed by compatibility).
4. Both bodies are rendered as complete closed meshes â€” no face suppression needed.

## Size Matching ("Meeting in the Middle")

When two connected shapes have different radii at the connection point:

1. Compute the **midpoint radius**: `matched = (parent_connection_radius + child_connection_radius) / 2`
2. On **each** shape independently:
   - Identify the connection face (by grid position from UV parameters)
   - Scale that face's vertices uniformly from the face's own center to reach the midpoint radius
   - For each neighboring face that shares an edge with the connection face: the shared vertices are already moved (they're the same vertices). The non-shared vertices stay in place. This naturally creates stretched/tapered neighbors.
3. The result: both shapes have their connection face at the same size, with smooth stretching on either side.

**Critical**: This is purely a vertex position adjustment. No faces are added, removed, or re-topologized. No face normal recomputation is needed beyond standard flat-shading normal computation.

## What "height_segments" Enables

For a cylinder to connect via its side to a torus, the cylinder needs height subdivision. Without it, each lateral quad spans the full height â€” too large to match a torus tube quad. With `height_segments=T` (matching the torus tube_segments), each cylinder side quad is the same height as a torus tube quad, enabling a clean 1:1 face match.

This applies to:
- Cylinder: needs `height_segments` for side-to-side connections with torus
- Cone: needs lateral subdivision for side connections (currently produces triangles which limits compatibility)

## JSON Format (v2 parametric â€” no changes needed)

The existing v2 format with `parent_attach`/`child_attach` regions and UV parameters is correct. No format changes needed. Only the CONNECTION VALIDATION and FACE MATCHING code needs to be rewritten.

```json
{
  "connection": {
    "parent_attach": { "region": "surface", "u": 0.0, "v": 0.25 },
    "child_attach": { "region": "side", "u": 0.0, "v": 0.0 },
    "rotation": 0.0
  }
}
```

## Implementation Plan

### Phase 1: Disable all deformation (get positioning right)
- Make `GenerateWithConnections` return unmodified faces (just `FaceGenerator::Generate`)
- Verify that ConnectionSolver positions bodies so they touch correctly
- Fix any positioning bugs (bodies should be flush, no gaps)
- All models should render as clean closed solids, just touching at one face

### Phase 2: Add face identification by grid position
- Map UV parameters to face grid indices for each primitive type
- Verify the correct face is identified at each connection point
- Add debug visualization (highlight the connection face in a different color)

### Phase 3: Add size matching deformation
- Compute midpoint radius between parent and child
- Scale connection face vertices from face center
- Propagate shared vertex moves to neighbors
- Verify smooth stretching with no topology changes

### Phase 4: Validate all models
- Ensure all example models use only compatible connections
- Verify visual quality matches the reference (torus-cylinder side connection)
- Run property tests

## What NOT to Do

- âŒ Do NOT cut holes in meshes
- âŒ Do NOT insert ring faces
- âŒ Do NOT taper mesh regions toward a point
- âŒ Do NOT use "closest face by world-space distance" â€” use grid position
- âŒ Do NOT modify face topology (add/remove vertices from faces)
- âŒ Do NOT suppress or skip faces during rendering
- âŒ Do NOT try to make an N-gon cap face connect to a quad surface face