# ProjectilePerf
Unreal Engine project demoing a Performant Projectile System, from my [Optimization for Game Programmers (What every Game Programmer should know about Performance)]() talk.

This project has two kinds of projectiles, one using the built in **ProjectileMovementComponent** with an Actor, and another one written from scratch in a **ProjectileSubsystem**, called *Actorless*. The projectiles simply move through the world at a constant velocity and destroy themselves when they impact something. A spawner will keep spawning projectiles if they were destroyed to keep a consistent amount of projectiles alive.

The Actorless projectiles are designed to utilize the CPU cache, prefetcher and branch predictor in order to achieve a good baseline performance. The projectile states are just stored in a struct, inside an array (SoA), compared to Actor/Components that are all heap allocated.


## Performance Results (100 Projectiles)
|             | Spawning | Ticking |
| ----------- | -------- | ------- |
| `Actor`     | 7.1ms    | 1ms     |
| `Actorless` | 0.042ms  | 0.097ms |

It's important to note, I did **NOT** optimize the  Actorless projectiles. This is literally just the first thing I typed into the code editor. By just understanding how the hardware works, and designing our code around that we get a 10x speedup. The Actorless projectiles can be easily optimized with Multithreading or SIMD.

## Running the Project
There should be a BP_ProjectileConfig in the Content Explorer. This is how you configure the projectiles to spawn. You can add more of these configs by adding another **AProjectileSpawner** into the world.

Then you can run Unreal Insights. Make sure to disable Debug Drawing inside of the Config files to get more accurate performance results.
