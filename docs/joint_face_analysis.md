# Joint Face Matching Analysis Report

This report analyzes the quality of face matching at body joints across all models
at multiple subdivision levels. The goal is to verify that connected faces are
identical (matching vertex count, area, position, and opposing normals).

## Summary

| Model | Joints | Perfect | Good | Acceptable | Poor | Failing |
|-------|--------|---------|------|------------|------|--------|
| Unit Cube Approximation | 0 | 0 | 0 | 0 | 0 | 0 |
| Snowman | 2 | 0 | 8 | 0 | 0 | 0 |
| Robot Arm | 3 | 4 | 0 | 2 | 6 | 0 |
| Space Station | 2 | 4 | 0 | 0 | 4 | 0 |
| Chess Pawn | 3 | 4 | 0 | 4 | 4 | 0 |
| Rocket | 2 | 8 | 0 | 0 | 0 | 0 |
| Dumbbell | 2 | 0 | 0 | 4 | 4 | 0 |
| Table | 4 | 16 | 0 | 0 | 0 | 0 |
| Spider Bot | 9 | 0 | 16 | 7 | 13 | 0 |
| Satellite | 5 | 4 | 0 | 0 | 16 | 0 |
| Humanoid | 9 | 0 | 0 | 17 | 19 | 0 |
| Windmill | 4 | 0 | 0 | 9 | 7 | 0 |
| **TOTAL** | - | 40 | 24 | 43 | 73 | 0 |

**Overall pass rate:** 59.4%

## Detailed Results

Grading criteria:
- **PERFECT**: center distance < 0.001, normal dot < -0.999, area ratio > 0.999, vertex match < 0.001
- **GOOD**: center distance < 0.01, normal dot < -0.99, area ratio > 0.95, vertex match < 0.02
- **ACCEPTABLE**: center distance < 0.05, normal dot < -0.95, area ratio > 0.8
- **POOR**: center distance < 0.2, normal dot < -0.8
- **FAILING**: everything else

### Unit Cube Approximation

File: `assets/bodies/01_unit_cube.json`  
Joints: 0

_No joints to analyze (single-node body)._

### Snowman

File: `assets/bodies/02_snowman.json`  
Joints: 2

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | body -> torso | 🟢 GOOD | 0.000000 | -0.996994 | 1.0000 | YES (3/3) | 0.012041 |
| 1 | torso -> head | 🟢 GOOD | 0.000000 | -0.996994 | 1.0000 | YES (3/3) | 0.008500 |
| 2 | body -> torso | 🟢 GOOD | 0.000000 | -0.998327 | 1.0000 | YES (3/3) | 0.010313 |
| 2 | torso -> head | 🟢 GOOD | 0.000000 | -0.998327 | 1.0000 | YES (3/3) | 0.007280 |
| 4 | body -> torso | 🟢 GOOD | 0.000000 | -0.999497 | 1.0000 | YES (3/3) | 0.005945 |
| 4 | torso -> head | 🟢 GOOD | 0.000000 | -0.999497 | 1.0000 | YES (3/3) | 0.004197 |
| 8 | body -> torso | 🟢 GOOD | 0.000000 | -0.999868 | 1.0000 | YES (3/3) | 0.003088 |
| 8 | torso -> head | 🟢 GOOD | 0.000000 | -0.999868 | 1.0000 | YES (3/3) | 0.002180 |

_All joints passing._

### Robot Arm

File: `assets/bodies/03_robot_arm.json`  
Joints: 3

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | base_cylinder -> joint | 🟡 ACCEPTABLE | 0.000000 | -0.999248 | 0.8691 | NO (4/3) | 0.221111 |
| 1 | joint -> forearm | 🟡 ACCEPTABLE | 0.000000 | -0.999248 | 0.8691 | NO (3/4) | 0.210428 |
| 1 | forearm -> hand | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (9/9) | 0.000000 |
| 2 | base_cylinder -> joint | 🟠 POOR | 0.000000 | -0.999582 | 0.6281 | NO (4/3) | 0.243202 |
| 2 | joint -> forearm | 🟠 POOR | 0.000000 | -0.999582 | 0.6281 | NO (3/4) | 0.188302 |
| 2 | forearm -> hand | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (17/17) | 0.000000 |
| 4 | base_cylinder -> joint | 🟠 POOR | 0.000000 | -0.999874 | 0.5437 | NO (4/3) | 0.266368 |
| 4 | joint -> forearm | 🟠 POOR | 0.000000 | -0.999874 | 0.5437 | NO (3/4) | 0.183307 |
| 4 | forearm -> hand | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (33/33) | 0.000000 |
| 8 | base_cylinder -> joint | 🟠 POOR | 0.000000 | -0.999967 | 0.5141 | NO (4/3) | 0.280862 |
| 8 | joint -> forearm | 🟠 POOR | 0.000001 | -0.999967 | 0.5141 | NO (3/4) | 0.192288 |
| 8 | forearm -> hand | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (65/65) | 0.000001 |

**Issues found:**

- 🟠 **base_cylinder -> joint** (subdiv 2): vertex count mismatch (4 vs 3), area mismatch (ratio=0.628), 
- 🟠 **joint -> forearm** (subdiv 2): vertex count mismatch (3 vs 4), area mismatch (ratio=0.628), 
- 🟠 **base_cylinder -> joint** (subdiv 4): vertex count mismatch (4 vs 3), area mismatch (ratio=0.544), 
- 🟠 **joint -> forearm** (subdiv 4): vertex count mismatch (3 vs 4), area mismatch (ratio=0.544), 
- 🟠 **base_cylinder -> joint** (subdiv 8): vertex count mismatch (4 vs 3), area mismatch (ratio=0.514), 
- 🟠 **joint -> forearm** (subdiv 8): vertex count mismatch (3 vs 4), area mismatch (ratio=0.514), 

### Space Station

File: `assets/bodies/04_space_station.json`  
Joints: 2

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | ring -> spoke | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.082716 |
| 1 | spoke -> antenna | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (10/10) | 0.000000 |
| 2 | ring -> spoke | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.069958 |
| 2 | spoke -> antenna | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (18/18) | 0.000000 |
| 4 | ring -> spoke | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.067610 |
| 4 | spoke -> antenna | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (34/34) | 0.000000 |
| 8 | ring -> spoke | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.067773 |
| 8 | spoke -> antenna | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (66/66) | 0.000000 |

**Issues found:**

- 🟠 **ring -> spoke** (subdiv 1): area mismatch (ratio=0.000), 
- 🟠 **ring -> spoke** (subdiv 2): area mismatch (ratio=0.000), 
- 🟠 **ring -> spoke** (subdiv 4): area mismatch (ratio=0.000), 
- 🟠 **ring -> spoke** (subdiv 8): area mismatch (ratio=0.000), 

### Chess Pawn

File: `assets/bodies/05_chess_pawn.json`  
Joints: 3

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | base_disc -> body | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 1 | body -> collar | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9778 | NO (8/9) | 0.121554 |
| 1 | collar -> head | 🟠 POOR | 0.000000 | -0.999248 | 0.0624 | NO (4/3) | 0.122806 |
| 2 | base_disc -> body | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 2 | body -> collar | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9970 | NO (16/17) | 0.064588 |
| 2 | collar -> head | 🟠 POOR | 0.000000 | -0.999582 | 0.0426 | NO (4/3) | 0.159776 |
| 4 | base_disc -> body | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 4 | body -> collar | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9996 | NO (32/33) | 0.033307 |
| 4 | collar -> head | 🟠 POOR | 0.000000 | -0.999874 | 0.0364 | NO (4/3) | 0.184990 |
| 8 | base_disc -> body | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |
| 8 | body -> collar | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 1.0000 | NO (64/65) | 0.016915 |
| 8 | collar -> head | 🟠 POOR | 0.000000 | -0.999967 | 0.0343 | NO (4/3) | 0.199052 |

**Issues found:**

- 🟠 **collar -> head** (subdiv 1): vertex count mismatch (4 vs 3), area mismatch (ratio=0.062), 
- 🟠 **collar -> head** (subdiv 2): vertex count mismatch (4 vs 3), area mismatch (ratio=0.043), 
- 🟠 **collar -> head** (subdiv 4): vertex count mismatch (4 vs 3), area mismatch (ratio=0.036), 
- 🟠 **collar -> head** (subdiv 8): vertex count mismatch (4 vs 3), area mismatch (ratio=0.034), 

### Rocket

File: `assets/bodies/06_rocket.json`  
Joints: 2

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | nosecone -> fuselage | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 1 | fuselage -> engine_bell | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 2 | nosecone -> fuselage | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 2 | fuselage -> engine_bell | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 4 | nosecone -> fuselage | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 4 | fuselage -> engine_bell | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 8 | nosecone -> fuselage | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |
| 8 | fuselage -> engine_bell | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |

_All joints passing._

### Dumbbell

File: `assets/bodies/07_dumbbell.json`  
Joints: 2

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | weight_a -> bar | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8169 | YES (4/4) | 0.097514 |
| 1 | bar -> weight_b | 🟡 ACCEPTABLE | 0.000000 | -0.999717 | 0.8787 | YES (4/4) | 0.084739 |
| 2 | weight_a -> bar | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9108 | YES (4/4) | 0.020219 |
| 2 | bar -> weight_b | 🟡 ACCEPTABLE | 0.000000 | -0.999994 | 0.8772 | YES (4/4) | 0.031369 |
| 4 | weight_a -> bar | 🟠 POOR | 0.000001 | -1.000000 | 0.5045 | YES (4/4) | 0.092101 |
| 4 | bar -> weight_b | 🟠 POOR | 0.000000 | -1.000000 | 0.4951 | YES (4/4) | 0.096981 |
| 8 | weight_a -> bar | 🟠 POOR | 0.000002 | -1.000000 | 0.2568 | YES (4/4) | 0.129034 |
| 8 | bar -> weight_b | 🟠 POOR | 0.000000 | -1.000000 | 0.2544 | YES (4/4) | 0.131339 |

**Issues found:**

- 🟠 **weight_a -> bar** (subdiv 4): area mismatch (ratio=0.504), 
- 🟠 **bar -> weight_b** (subdiv 4): area mismatch (ratio=0.495), 
- 🟠 **weight_a -> bar** (subdiv 8): area mismatch (ratio=0.257), 
- 🟠 **bar -> weight_b** (subdiv 8): area mismatch (ratio=0.254), 

### Table

File: `assets/bodies/08_table.json`  
Joints: 4

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | tabletop -> leg1 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 1 | tabletop -> leg2 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 1 | tabletop -> leg3 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 1 | tabletop -> leg4 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 2 | tabletop -> leg1 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 2 | tabletop -> leg2 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 2 | tabletop -> leg3 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 2 | tabletop -> leg4 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 4 | tabletop -> leg1 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 4 | tabletop -> leg2 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 4 | tabletop -> leg3 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 4 | tabletop -> leg4 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 8 | tabletop -> leg1 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |
| 8 | tabletop -> leg2 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |
| 8 | tabletop -> leg3 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |
| 8 | tabletop -> leg4 | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |

_All joints passing._

### Spider Bot

File: `assets/bodies/09_spider_bot.json`  
Joints: 9

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | body_core -> leg_front_right | 🟡 ACCEPTABLE | 0.044670 | -1.000000 | 0.9788 | YES (4/4) | 0.087340 |
| 1 | leg_front_right -> lower_leg_fr | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.016968 |
| 1 | body_core -> leg_back_right | 🟡 ACCEPTABLE | 0.041338 | -0.989159 | 0.9833 | YES (4/4) | 0.200731 |
| 1 | leg_back_right -> lower_leg_br | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.016968 |
| 1 | body_core -> leg_front_left | 🟡 ACCEPTABLE | 0.044670 | -1.000000 | 0.9788 | YES (4/4) | 0.087340 |
| 1 | leg_front_left -> lower_leg_fl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.016968 |
| 1 | body_core -> leg_back_left | 🟡 ACCEPTABLE | 0.041337 | -0.989159 | 0.9833 | YES (4/4) | 0.200731 |
| 1 | leg_back_left -> lower_leg_bl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.016968 |
| 1 | body_core -> head | 🟠 POOR | 0.000000 | -0.999996 | 0.7505 | NO (3/4) | 0.266536 |
| 2 | body_core -> leg_front_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5072 | YES (4/4) | 0.054862 |
| 2 | leg_front_right -> lower_leg_fr | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.008502 |
| 2 | body_core -> leg_back_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5072 | YES (4/4) | 0.357862 |
| 2 | leg_back_right -> lower_leg_br | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.008502 |
| 2 | body_core -> leg_front_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5072 | YES (4/4) | 0.054862 |
| 2 | leg_front_left -> lower_leg_fl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.008503 |
| 2 | body_core -> leg_back_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5072 | YES (4/4) | 0.357862 |
| 2 | leg_back_left -> lower_leg_bl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.008502 |
| 2 | body_core -> head | 🟡 ACCEPTABLE | 0.000000 | -0.999885 | 0.8147 | YES (4/4) | 0.100668 |
| 4 | body_core -> leg_front_right | 🟠 POOR | 0.000000 | -1.000000 | 0.3973 | YES (4/4) | 0.093820 |
| 4 | leg_front_right -> lower_leg_fr | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.004254 |
| 4 | body_core -> leg_back_right | 🟠 POOR | 0.000000 | -1.000000 | 0.3973 | YES (4/4) | 0.322524 |
| 4 | leg_back_right -> lower_leg_br | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.004254 |
| 4 | body_core -> leg_front_left | 🟠 POOR | 0.000000 | -1.000000 | 0.3973 | YES (4/4) | 0.093820 |
| 4 | leg_front_left -> lower_leg_fl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.004254 |
| 4 | body_core -> leg_back_left | 🟠 POOR | 0.000000 | -1.000000 | 0.3973 | YES (4/4) | 0.322525 |
| 4 | leg_back_left -> lower_leg_bl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.004254 |
| 4 | body_core -> head | 🟡 ACCEPTABLE | 0.000000 | -0.999995 | 0.8669 | YES (4/4) | 0.135179 |
| 8 | body_core -> leg_front_right | 🟠 POOR | 0.000003 | -1.000000 | 0.2234 | YES (4/4) | 0.124652 |
| 8 | leg_front_right -> lower_leg_fr | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.002127 |
| 8 | body_core -> leg_back_right | 🟠 POOR | 0.000003 | -1.000000 | 0.2234 | YES (4/4) | 0.333171 |
| 8 | leg_back_right -> lower_leg_br | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.002127 |
| 8 | body_core -> leg_front_left | 🟠 POOR | 0.000003 | -1.000000 | 0.2234 | YES (4/4) | 0.124652 |
| 8 | leg_front_left -> lower_leg_fl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.002127 |
| 8 | body_core -> leg_back_left | 🟠 POOR | 0.000003 | -1.000000 | 0.2234 | YES (4/4) | 0.333171 |
| 8 | leg_back_left -> lower_leg_bl | 🟢 GOOD | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.002127 |
| 8 | body_core -> head | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8896 | YES (4/4) | 0.137396 |

**Issues found:**

- 🟠 **body_core -> head** (subdiv 1): vertex count mismatch (3 vs 4), area mismatch (ratio=0.750), 
- 🟠 **body_core -> leg_front_right** (subdiv 2): area mismatch (ratio=0.507), 
- 🟠 **body_core -> leg_back_right** (subdiv 2): area mismatch (ratio=0.507), 
- 🟠 **body_core -> leg_front_left** (subdiv 2): area mismatch (ratio=0.507), 
- 🟠 **body_core -> leg_back_left** (subdiv 2): area mismatch (ratio=0.507), 
- 🟠 **body_core -> leg_front_right** (subdiv 4): area mismatch (ratio=0.397), 
- 🟠 **body_core -> leg_back_right** (subdiv 4): area mismatch (ratio=0.397), 
- 🟠 **body_core -> leg_front_left** (subdiv 4): area mismatch (ratio=0.397), 
- 🟠 **body_core -> leg_back_left** (subdiv 4): area mismatch (ratio=0.397), 
- 🟠 **body_core -> leg_front_right** (subdiv 8): area mismatch (ratio=0.223), 
- 🟠 **body_core -> leg_back_right** (subdiv 8): area mismatch (ratio=0.223), 
- 🟠 **body_core -> leg_front_left** (subdiv 8): area mismatch (ratio=0.223), 
- 🟠 **body_core -> leg_back_left** (subdiv 8): area mismatch (ratio=0.223), 

### Satellite

File: `assets/bodies/10_satellite.json`  
Joints: 5

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | central_hub -> solar_panel_right | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367605 |
| 1 | central_hub -> solar_panel_left | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367605 |
| 1 | central_hub -> antenna_dish | 🟠 POOR | 0.000000 | -0.999248 | 0.3315 | NO (4/3) | 0.250279 |
| 1 | antenna_dish -> feed_horn | 🟠 POOR | 0.000000 | -0.999248 | 0.4108 | NO (3/9) | 0.021214 |
| 1 | central_hub -> thruster | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (8/8) | 0.000000 |
| 2 | central_hub -> solar_panel_right | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367605 |
| 2 | central_hub -> solar_panel_left | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367605 |
| 2 | central_hub -> antenna_dish | 🟠 POOR | 0.000000 | -0.999582 | 0.2401 | NO (4/3) | 0.267198 |
| 2 | antenna_dish -> feed_horn | 🟠 POOR | 0.000000 | -0.999582 | 0.2666 | NO (3/17) | 0.034997 |
| 2 | central_hub -> thruster | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (16/16) | 0.000000 |
| 4 | central_hub -> solar_panel_right | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367605 |
| 4 | central_hub -> solar_panel_left | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367605 |
| 4 | central_hub -> antenna_dish | 🟠 POOR | 0.000000 | -0.999874 | 0.2116 | NO (4/3) | 0.276691 |
| 4 | antenna_dish -> feed_horn | 🟠 POOR | 0.000000 | -0.999874 | 0.1486 | NO (3/33) | 0.041203 |
| 4 | central_hub -> thruster | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (32/32) | 0.000000 |
| 8 | central_hub -> solar_panel_right | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367604 |
| 8 | central_hub -> solar_panel_left | 🟠 POOR | 0.000000 | -1.000000 | 0.0000 | YES (4/4) | 0.367604 |
| 8 | central_hub -> antenna_dish | 🟠 POOR | 0.000000 | -0.999967 | 0.2026 | NO (4/3) | 0.285619 |
| 8 | antenna_dish -> feed_horn | 🟠 POOR | 0.000000 | -0.999967 | 0.0772 | NO (3/65) | 0.043075 |
| 8 | central_hub -> thruster | ✅ PERFECT | 0.000000 | -1.000000 | 1.0000 | YES (64/64) | 0.000000 |

**Issues found:**

- 🟠 **central_hub -> solar_panel_right** (subdiv 1): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> solar_panel_left** (subdiv 1): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> antenna_dish** (subdiv 1): vertex count mismatch (4 vs 3), area mismatch (ratio=0.331), 
- 🟠 **antenna_dish -> feed_horn** (subdiv 1): vertex count mismatch (3 vs 9), area mismatch (ratio=0.411), 
- 🟠 **central_hub -> solar_panel_right** (subdiv 2): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> solar_panel_left** (subdiv 2): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> antenna_dish** (subdiv 2): vertex count mismatch (4 vs 3), area mismatch (ratio=0.240), 
- 🟠 **antenna_dish -> feed_horn** (subdiv 2): vertex count mismatch (3 vs 17), area mismatch (ratio=0.267), 
- 🟠 **central_hub -> solar_panel_right** (subdiv 4): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> solar_panel_left** (subdiv 4): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> antenna_dish** (subdiv 4): vertex count mismatch (4 vs 3), area mismatch (ratio=0.212), 
- 🟠 **antenna_dish -> feed_horn** (subdiv 4): vertex count mismatch (3 vs 33), area mismatch (ratio=0.149), 
- 🟠 **central_hub -> solar_panel_right** (subdiv 8): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> solar_panel_left** (subdiv 8): area mismatch (ratio=0.000), 
- 🟠 **central_hub -> antenna_dish** (subdiv 8): vertex count mismatch (4 vs 3), area mismatch (ratio=0.203), 
- 🟠 **antenna_dish -> feed_horn** (subdiv 8): vertex count mismatch (3 vs 65), area mismatch (ratio=0.077), 

### Humanoid

File: `assets/bodies/11_humanoid.json`  
Joints: 9

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | torso -> head | 🟠 POOR | 0.000000 | -0.999510 | 0.7148 | NO (4/3) | 0.165170 |
| 1 | torso -> arm_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5378 | YES (4/4) | 0.148091 |
| 1 | arm_right -> hand_right | 🟡 ACCEPTABLE | 0.000000 | -0.999716 | 0.8799 | YES (4/4) | 0.035078 |
| 1 | torso -> arm_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5378 | YES (4/4) | 0.148091 |
| 1 | arm_left -> hand_left | 🟡 ACCEPTABLE | 0.000000 | -0.999716 | 0.8799 | YES (4/4) | 0.035078 |
| 1 | torso -> leg_right | 🟡 ACCEPTABLE | 0.000000 | -0.996182 | 0.9130 | YES (4/4) | 0.164927 |
| 1 | leg_right -> foot_right | 🟠 POOR | 0.000000 | -0.999540 | 0.5841 | NO (4/8) | 0.015337 |
| 1 | torso -> leg_left | 🟡 ACCEPTABLE | 0.000000 | -0.991844 | 0.9130 | YES (4/4) | 0.264063 |
| 1 | leg_left -> foot_left | 🟠 POOR | 0.000000 | -0.999540 | 0.5841 | NO (4/8) | 0.015337 |
| 2 | torso -> head | 🟡 ACCEPTABLE | 0.000000 | -0.999596 | 0.9849 | NO (4/3) | 0.208402 |
| 2 | torso -> arm_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5495 | YES (4/4) | 0.145139 |
| 2 | arm_right -> hand_right | 🟡 ACCEPTABLE | 0.000000 | -0.999994 | 0.8351 | YES (4/4) | 0.033615 |
| 2 | torso -> arm_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5495 | YES (4/4) | 0.145139 |
| 2 | arm_left -> hand_left | 🟡 ACCEPTABLE | 0.000000 | -0.999994 | 0.8351 | YES (4/4) | 0.033615 |
| 2 | torso -> leg_right | 🟡 ACCEPTABLE | 0.000000 | -0.999754 | 0.9411 | YES (4/4) | 0.158700 |
| 2 | leg_right -> foot_right | 🟠 POOR | 0.000000 | -0.999993 | 0.5271 | NO (4/16) | 0.016766 |
| 2 | torso -> leg_left | 🟡 ACCEPTABLE | 0.000000 | -0.999613 | 0.9411 | YES (4/4) | 0.121451 |
| 2 | leg_left -> foot_left | 🟠 POOR | 0.000000 | -0.999993 | 0.5271 | NO (4/16) | 0.016766 |
| 4 | torso -> head | 🟠 POOR | 0.000000 | -0.999875 | 0.5513 | NO (4/3) | 0.236477 |
| 4 | torso -> arm_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5519 | YES (4/4) | 0.143679 |
| 4 | arm_right -> hand_right | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8164 | YES (4/4) | 0.033122 |
| 4 | torso -> arm_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5519 | YES (4/4) | 0.143679 |
| 4 | arm_left -> hand_left | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8164 | YES (4/4) | 0.033122 |
| 4 | torso -> leg_right | 🟡 ACCEPTABLE | 0.000000 | -0.999980 | 0.9684 | YES (4/4) | 0.114269 |
| 4 | leg_right -> foot_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5137 | NO (4/32) | 0.003624 |
| 4 | torso -> leg_left | 🟡 ACCEPTABLE | 0.000000 | -0.999976 | 0.9684 | YES (4/4) | 0.106527 |
| 4 | leg_left -> foot_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5137 | NO (4/32) | 0.003624 |
| 8 | torso -> head | 🟠 POOR | 0.000001 | -0.999967 | 0.2843 | NO (4/3) | 0.251510 |
| 8 | torso -> arm_right | 🟠 POOR | 0.000001 | -1.000000 | 0.5523 | YES (4/4) | 0.142963 |
| 8 | arm_right -> hand_right | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8082 | YES (4/4) | 0.032916 |
| 8 | torso -> arm_left | 🟠 POOR | 0.000001 | -1.000000 | 0.5523 | YES (4/4) | 0.142963 |
| 8 | arm_left -> hand_left | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8082 | YES (4/4) | 0.032916 |
| 8 | torso -> leg_right | 🟡 ACCEPTABLE | 0.000001 | -0.999999 | 0.9838 | YES (4/4) | 0.090717 |
| 8 | leg_right -> foot_right | 🟠 POOR | 0.000000 | -1.000000 | 0.5104 | NO (4/64) | 0.003033 |
| 8 | torso -> leg_left | 🟡 ACCEPTABLE | 0.000001 | -0.999999 | 0.9838 | YES (4/4) | 0.099458 |
| 8 | leg_left -> foot_left | 🟠 POOR | 0.000000 | -1.000000 | 0.5104 | NO (4/64) | 0.003033 |

**Issues found:**

- 🟠 **torso -> head** (subdiv 1): vertex count mismatch (4 vs 3), area mismatch (ratio=0.715), 
- 🟠 **torso -> arm_right** (subdiv 1): area mismatch (ratio=0.538), 
- 🟠 **torso -> arm_left** (subdiv 1): area mismatch (ratio=0.538), 
- 🟠 **leg_right -> foot_right** (subdiv 1): vertex count mismatch (4 vs 8), area mismatch (ratio=0.584), 
- 🟠 **leg_left -> foot_left** (subdiv 1): vertex count mismatch (4 vs 8), area mismatch (ratio=0.584), 
- 🟠 **torso -> arm_right** (subdiv 2): area mismatch (ratio=0.549), 
- 🟠 **torso -> arm_left** (subdiv 2): area mismatch (ratio=0.549), 
- 🟠 **leg_right -> foot_right** (subdiv 2): vertex count mismatch (4 vs 16), area mismatch (ratio=0.527), 
- 🟠 **leg_left -> foot_left** (subdiv 2): vertex count mismatch (4 vs 16), area mismatch (ratio=0.527), 
- 🟠 **torso -> head** (subdiv 4): vertex count mismatch (4 vs 3), area mismatch (ratio=0.551), 
- 🟠 **torso -> arm_right** (subdiv 4): area mismatch (ratio=0.552), 
- 🟠 **torso -> arm_left** (subdiv 4): area mismatch (ratio=0.552), 
- 🟠 **leg_right -> foot_right** (subdiv 4): vertex count mismatch (4 vs 32), area mismatch (ratio=0.514), 
- 🟠 **leg_left -> foot_left** (subdiv 4): vertex count mismatch (4 vs 32), area mismatch (ratio=0.514), 
- 🟠 **torso -> head** (subdiv 8): vertex count mismatch (4 vs 3), area mismatch (ratio=0.284), 
- 🟠 **torso -> arm_right** (subdiv 8): area mismatch (ratio=0.552), 
- 🟠 **torso -> arm_left** (subdiv 8): area mismatch (ratio=0.552), 
- 🟠 **leg_right -> foot_right** (subdiv 8): vertex count mismatch (4 vs 64), area mismatch (ratio=0.510), 
- 🟠 **leg_left -> foot_left** (subdiv 8): vertex count mismatch (4 vs 64), area mismatch (ratio=0.510), 

### Windmill

File: `assets/bodies/12_windmill.json`  
Joints: 4

| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |
|--------|-------|-------|------|------------|------------|-----------|-------------|
| 1 | tower -> hub | 🟠 POOR | 0.012012 | -1.000000 | 0.1323 | NO (3/4) | 0.148735 |
| 1 | hub -> blade_1 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9777 | YES (4/4) | 0.065410 |
| 1 | hub -> blade_2 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9777 | YES (4/4) | 0.064238 |
| 1 | hub -> blade_3 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9777 | YES (4/4) | 0.064238 |
| 2 | tower -> hub | 🟠 POOR | 0.000000 | -1.000000 | 0.1033 | NO (3/4) | 0.196563 |
| 2 | hub -> blade_1 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8337 | YES (4/4) | 0.030299 |
| 2 | hub -> blade_2 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8337 | YES (4/4) | 0.092800 |
| 2 | hub -> blade_3 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.8337 | YES (4/4) | 0.092800 |
| 4 | tower -> hub | 🟠 POOR | 0.000000 | -1.000000 | 0.0561 | NO (3/4) | 0.202100 |
| 4 | hub -> blade_1 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9315 | YES (4/4) | 0.005445 |
| 4 | hub -> blade_2 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9315 | YES (4/4) | 0.124685 |
| 4 | hub -> blade_3 | 🟡 ACCEPTABLE | 0.000000 | -1.000000 | 0.9315 | YES (4/4) | 0.124685 |
| 8 | tower -> hub | 🟠 POOR | 0.000000 | -1.000000 | 0.0292 | NO (3/4) | 0.204886 |
| 8 | hub -> blade_1 | 🟠 POOR | 0.000004 | -1.000000 | 0.5412 | YES (4/4) | 0.029195 |
| 8 | hub -> blade_2 | 🟠 POOR | 0.000004 | -1.000000 | 0.5412 | YES (4/4) | 0.144392 |
| 8 | hub -> blade_3 | 🟠 POOR | 0.000004 | -1.000000 | 0.5412 | YES (4/4) | 0.144392 |

**Issues found:**

- 🟠 **tower -> hub** (subdiv 1): vertex count mismatch (3 vs 4), area mismatch (ratio=0.132), 
- 🟠 **tower -> hub** (subdiv 2): vertex count mismatch (3 vs 4), area mismatch (ratio=0.103), 
- 🟠 **tower -> hub** (subdiv 4): vertex count mismatch (3 vs 4), area mismatch (ratio=0.056), 
- 🟠 **tower -> hub** (subdiv 8): vertex count mismatch (3 vs 4), area mismatch (ratio=0.029), 
- 🟠 **hub -> blade_1** (subdiv 8): area mismatch (ratio=0.541), 
- 🟠 **hub -> blade_2** (subdiv 8): area mismatch (ratio=0.541), 
- 🟠 **hub -> blade_3** (subdiv 8): area mismatch (ratio=0.541), 

## Analysis Notes

This analysis tests the core goal of the connection system: that at each joint,
the parent body's connection face and the child body's connection face are identical
(same position in world space, opposing normals, matching geometry).

Key areas to investigate if joints are POOR/FAILING:

1. **Normal dot != -1.0**: The face-center snapping or orientation computation is off.
2. **Center distance != 0**: The positioning transform doesn't align face centers.
3. **Vertex count mismatch**: Subdivision derivation isn't propagating segment counts correctly.
4. **Area mismatch**: Size-matching deformation phase isn't equalizing face sizes.
5. **High vertex distance**: Even with matching counts, the rotational alignment is off.

